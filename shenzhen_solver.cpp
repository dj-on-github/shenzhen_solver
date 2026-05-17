// shenzhen_solver.cpp - Shenzhen Solitaire solver
//
// Build:  g++ -O3 -std=c++17 -Wall -pthread shenzhen_solver.cpp -o shenzhen_solver
// Run:    ./shenzhen_solver [-j N]
//         -j N  : run N solver threads in parallel
//
// Copyright(c) David Johnston, 2026
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Card encoding (uint8_t):
//   0           : empty / sentinel
//   1..9        : R1..R9          Red   1 to 9
//   10..18      : G1..G9          Green 1 to 9
//   19..27      : B1..B9          Black 1 to 9 (Dragon in black suite is white dragon)
//   28,29,30    : RD, GD, WD      Red, Green White Dragons - 4 of each in the deck
//   31          : F               Flower card - only one
//   32          : LOCKED          Dragon cell with collected dragons

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

using Card = uint8_t;

constexpr Card EMPTY  = 0;
constexpr Card RD     = 28;
constexpr Card GD     = 29;
constexpr Card WD     = 30;
constexpr Card FLOWER = 31;
constexpr Card LOCKED = 32;

inline bool is_number(Card c) { return c >= 1 && c <= 27; }
inline bool is_dragon(Card c) { return c >= RD && c <= WD; }
inline int  card_value(Card c) { return ((c - 1) % 9) + 1; }
inline int  color_idx (Card c) { return (c - 1) / 9; }

inline bool can_stack(Card top, Card bottom) {
    if (!is_number(top) || !is_number(bottom)) return false;
    if (card_value(top) + 1 != card_value(bottom)) return false;
    return color_idx(top) != color_idx(bottom);
}

std::string card_to_str(Card c) {
    if (c == EMPTY)  return "__";
    if (c == FLOWER) return "F";
    if (c == LOCKED) return "X";
    if (c == RD)     return "RD";
    if (c == GD)     return "GD";
    if (c == WD)     return "WD";
    char buf[3];
    buf[0] = "RGB"[color_idx(c)];
    buf[1] = (char)('0' + card_value(c));
    buf[2] = 0;
    return std::string(buf);
}

// --------- state ---------

constexpr int MAX_COL = 24;

struct State {
    std::array<std::array<Card, MAX_COL>, 8> cols{};
    std::array<uint8_t, 8> col_sizes{};
    std::array<Card, 3> dragons{};         // EMPTY / parked card / LOCKED
    bool flower = false;
    std::array<uint8_t, 3> foundations{};  // value 0..9 (0 = empty)
};

// State key: sorted (size, cards…) columns + dragon-cell canonicalization +
// flower + foundations. Sorted columns and multiset dragon cells collapse
// symmetries.
constexpr int KEY_BYTES = 8 * (MAX_COL + 1) + 1 + 3 + 1 + 3;

struct StateKey {
    std::array<uint8_t, KEY_BYTES> data;
    bool operator==(const StateKey& o) const noexcept { return data == o.data; }
};

struct StateKeyHash {
    size_t operator()(const StateKey& k) const noexcept {
        // FNV-1a 64
        size_t h = 14695981039346656037ULL;
        for (uint8_t b : k.data) {
            h ^= b;
            h *= 1099511628211ULL;
        }
        return h;
    }
};

StateKey state_key(const State& s) {
    std::array<std::array<uint8_t, MAX_COL + 1>, 8> sc{};
    for (int i = 0; i < 8; i++) {
        sc[i][0] = s.col_sizes[i];
        for (int k = 0; k < s.col_sizes[i]; k++) sc[i][k + 1] = s.cols[i][k];
    }
    std::sort(sc.begin(), sc.end());

    int locked = 0;
    std::array<Card, 3> parked{};
    int np = 0;
    for (int j = 0; j < 3; j++) {
        if (s.dragons[j] == LOCKED) locked++;
        else if (s.dragons[j] != EMPTY) parked[np++] = s.dragons[j];
    }
    std::sort(parked.begin(), parked.begin() + np);

    StateKey key{};
    int idx = 0;
    for (auto& col : sc) {
        for (int k = 0; k < MAX_COL + 1; k++) key.data[idx++] = col[k];
    }
    key.data[idx++] = (uint8_t)locked;
    for (int j = 0; j < 3; j++) key.data[idx++] = (uint8_t)(j < np ? parked[j] : 0);
    key.data[idx++] = (uint8_t)s.flower;
    for (int j = 0; j < 3; j++) key.data[idx++] = s.foundations[j];
    return key;
}

bool is_won(const State& s) {
    for (int i = 0; i < 8; i++) if (s.col_sizes[i] > 0) return false;
    if (!s.flower) return false;
    for (int i = 0; i < 3; i++) if (s.foundations[i] != 9) return false;
    int locked = 0;
    for (int j = 0; j < 3; j++) if (s.dragons[j] == LOCKED) locked++;
    return locked == 3;
}

bool safe_for_auto(Card c, const std::array<uint8_t, 3>& found) {
    if (!is_number(c)) return false;
    int ci = color_idx(c);
    int v = card_value(c);
    if (v != found[ci] + 1) return false;
    int mn = INT_MAX;
    for (int i = 0; i < 3; i++) if (i != ci) mn = std::min(mn, (int)found[i]);
    return v <= mn + 1;
}

int top_run_length(const State& s, int col) {
    int sz = s.col_sizes[col];
    if (sz == 0) return 0;
    if (!is_number(s.cols[col][sz - 1])) return 1;
    int n = 1;
    for (int k = sz - 1; k > 0; k--) {
        if (is_number(s.cols[col][k - 1]) && can_stack(s.cols[col][k], s.cols[col][k - 1])) n++;
        else break;
    }
    return n;
}

// --------- moves ---------

enum MoveType : uint8_t {
    MV_MOVE = 0,            // a=src col, b=dst col, c=n
    MV_COL_TO_CELL,         // a=src col, b=cell
    MV_CELL_TO_COL,         // a=cell,    b=dst col
    MV_COL_TO_FOUND,        // a=src col
    MV_CELL_TO_FOUND,       // a=cell
    MV_COL_TO_FLOWER,       // a=src col
    MV_COLLECT_DRAGONS,     // a=0/1/2 (R/G/W), b=dest cell
};

struct Move {
    uint8_t type, a, b, c;
};

void apply_auto(State& s, std::vector<Move>& moves) {
    while (true) {
        bool progressed = false;

        if (!s.flower) {
            for (int i = 0; i < 8; i++) {
                if (s.col_sizes[i] > 0 && s.cols[i][s.col_sizes[i] - 1] == FLOWER) {
                    s.col_sizes[i]--;
                    s.flower = true;
                    moves.push_back({MV_COL_TO_FLOWER, (uint8_t)i, 0, 0});
                    progressed = true;
                    break;
                }
            }
            if (progressed) continue;
        }

        for (int i = 0; i < 8; i++) {
            if (s.col_sizes[i] > 0) {
                Card c = s.cols[i][s.col_sizes[i] - 1];
                if (safe_for_auto(c, s.foundations)) {
                    s.col_sizes[i]--;
                    s.foundations[color_idx(c)] = (uint8_t)card_value(c);
                    moves.push_back({MV_COL_TO_FOUND, (uint8_t)i, 0, 0});
                    progressed = true;
                    break;
                }
            }
        }
        if (progressed) continue;

        for (int j = 0; j < 3; j++) {
            Card c = s.dragons[j];
            if (c != EMPTY && c != LOCKED && safe_for_auto(c, s.foundations)) {
                s.foundations[color_idx(c)] = (uint8_t)card_value(c);
                s.dragons[j] = EMPTY;
                moves.push_back({MV_CELL_TO_FOUND, (uint8_t)j, 0, 0});
                progressed = true;
                break;
            }
        }

        if (!progressed) break;
    }
}

void legal_moves(const State& s, std::vector<Move>& out) {
    int canon_ec = -1, canon_cell = -1;
    for (int i = 0; i < 8; i++) if (s.col_sizes[i] == 0) { canon_ec   = i; break; }
    for (int j = 0; j < 3; j++) if (s.dragons[j] == EMPTY) { canon_cell = j; break; }

    // 1. collect-dragons
    for (int dc = 0; dc < 3; dc++) {
        Card d = (dc == 0) ? RD : (dc == 1) ? GD : WD;
        int accessible = 0;
        for (int i = 0; i < 8; i++)
            if (s.col_sizes[i] > 0 && s.cols[i][s.col_sizes[i] - 1] == d) accessible++;
        for (int j = 0; j < 3; j++) if (s.dragons[j] == d) accessible++;
        if (accessible == 4) {
            int dest = -1;
            for (int j = 0; j < 3; j++) if (s.dragons[j] == d) { dest = j; break; }
            if (dest == -1) dest = canon_cell;
            if (dest != -1) out.push_back({MV_COLLECT_DRAGONS, (uint8_t)dc, (uint8_t)dest, 0});
        }
    }

    // 2. manual col -> foundation
    for (int i = 0; i < 8; i++) {
        if (s.col_sizes[i] > 0) {
            Card c = s.cols[i][s.col_sizes[i] - 1];
            if (is_number(c) && card_value(c) == s.foundations[color_idx(c)] + 1) {
                out.push_back({MV_COL_TO_FOUND, (uint8_t)i, 0, 0});
            }
        }
    }

    // 3. manual cell -> foundation
    for (int j = 0; j < 3; j++) {
        Card c = s.dragons[j];
        if (c != EMPTY && c != LOCKED && is_number(c) &&
            card_value(c) == s.foundations[color_idx(c)] + 1) {
            out.push_back({MV_CELL_TO_FOUND, (uint8_t)j, 0, 0});
        }
    }

    // 4. col -> col
    for (int i = 0; i < 8; i++) {
        int sz = s.col_sizes[i];
        if (sz == 0) continue;
        int max_n = top_run_length(s, i);
        for (int n = 1; n <= max_n; n++) {
            int bpos = sz - n;
            Card bottom = s.cols[i][bpos];
            if (n > 1) {
                bool ok = true;
                for (int k = 0; k < n; k++) {
                    if (!is_number(s.cols[i][bpos + k])) { ok = false; break; }
                }
                if (!ok) continue;
            }
            for (int j = 0; j < 8; j++) {
                if (i == j) continue;
                if (s.col_sizes[j] == 0) {
                    if (j != canon_ec) continue;
                    if (n == sz) continue;                       // whole-col -> empty == no-op
                    out.push_back({MV_MOVE, (uint8_t)i, (uint8_t)j, (uint8_t)n});
                } else {
                    Card t = s.cols[j][s.col_sizes[j] - 1];
                    if (is_number(bottom) && is_number(t) && can_stack(bottom, t)) {
                        out.push_back({MV_MOVE, (uint8_t)i, (uint8_t)j, (uint8_t)n});
                    }
                }
            }
        }
    }

    // 5. cell -> col (dedupe by card identity)
    bool seen[33] = {false};
    for (int j = 0; j < 3; j++) {
        Card d = s.dragons[j];
        if (d == EMPTY || d == LOCKED || seen[d]) continue;
        seen[d] = true;
        for (int i = 0; i < 8; i++) {
            if (s.col_sizes[i] == 0) {
                if (i != canon_ec) continue;
                out.push_back({MV_CELL_TO_COL, (uint8_t)j, (uint8_t)i, 0});
            } else {
                Card t = s.cols[i][s.col_sizes[i] - 1];
                if (is_number(d) && is_number(t) && can_stack(d, t)) {
                    out.push_back({MV_CELL_TO_COL, (uint8_t)j, (uint8_t)i, 0});
                }
            }
        }
    }

    // 6. col -> cell
    if (canon_cell != -1) {
        for (int i = 0; i < 8; i++) {
            if (s.col_sizes[i] > 0) {
                out.push_back({MV_COL_TO_CELL, (uint8_t)i, (uint8_t)canon_cell, 0});
            }
        }
    }
}

void apply_move(State& s, const Move& m) {
    switch (m.type) {
        case MV_MOVE: {
            int src = m.a, dst = m.b, n = m.c;
            int ssz = s.col_sizes[src], dsz = s.col_sizes[dst];
            for (int k = 0; k < n; k++) s.cols[dst][dsz + k] = s.cols[src][ssz - n + k];
            s.col_sizes[src] = (uint8_t)(ssz - n);
            s.col_sizes[dst] = (uint8_t)(dsz + n);
            break;
        }
        case MV_COL_TO_CELL: {
            int src = m.a, cell = m.b;
            s.dragons[cell] = s.cols[src][s.col_sizes[src] - 1];
            s.col_sizes[src]--;
            break;
        }
        case MV_CELL_TO_COL: {
            int cell = m.a, dst = m.b;
            s.cols[dst][s.col_sizes[dst]++] = s.dragons[cell];
            s.dragons[cell] = EMPTY;
            break;
        }
        case MV_COL_TO_FOUND: {
            int src = m.a;
            Card c = s.cols[src][--s.col_sizes[src]];
            s.foundations[color_idx(c)] = (uint8_t)card_value(c);
            break;
        }
        case MV_CELL_TO_FOUND: {
            int cell = m.a;
            Card c = s.dragons[cell];
            s.dragons[cell] = EMPTY;
            s.foundations[color_idx(c)] = (uint8_t)card_value(c);
            break;
        }
        case MV_COL_TO_FLOWER: {
            int src = m.a;
            s.col_sizes[src]--;
            s.flower = true;
            break;
        }
        case MV_COLLECT_DRAGONS: {
            int dc = m.a, dest = m.b;
            Card d = (dc == 0) ? RD : (dc == 1) ? GD : WD;
            for (int i = 0; i < 8; i++) {
                if (s.col_sizes[i] > 0 && s.cols[i][s.col_sizes[i] - 1] == d) s.col_sizes[i]--;
            }
            for (int j = 0; j < 3; j++) if (s.dragons[j] == d) s.dragons[j] = EMPTY;
            s.dragons[dest] = LOCKED;
            break;
        }
    }
}

inline int move_priority(const Move& m) {
    static const int p[] = {4, 6, 5, 1, 2, 3, 0};
    return p[m.type];
}

struct MoveCmp {
    bool operator()(const Move& a, const Move& b) const noexcept {
        int pa = move_priority(a), pb = move_priority(b);
        if (pa != pb) return pa < pb;
        if (a.type == MV_MOVE && b.type == MV_MOVE) return a.c > b.c;  // larger stacks first
        return false;
    }
};

// --------- solver ---------

struct Solver {
    std::unordered_set<StateKey, StateKeyHash> visited;
    size_t max_states = 2'000'000;
    bool gave_up = false;
    std::vector<Move> path;

    // Iterative DFS — recursive form blows the OS stack on long solution paths.
    bool dfs(State initial) {
        struct Frame {
            State state;
            std::vector<Move> moves;
            size_t auto_start;        // path size when we entered this frame
            size_t move_idx;          // index of next move to try
            size_t before_move;       // path size before the move pushed into the child
            bool   moves_initialized;
        };

        std::vector<Frame> stack;
        stack.reserve(8192);

        Frame root{};
        root.state = initial;
        root.auto_start = path.size();
        root.moves_initialized = false;
        root.move_idx = 0;
        stack.push_back(std::move(root));

        while (!stack.empty()) {
            if (visited.size() > max_states) { gave_up = true; return false; }

            Frame& top = stack.back();

            if (!top.moves_initialized) {
                apply_auto(top.state, path);
                if (is_won(top.state)) return true;

                StateKey k = state_key(top.state);
                auto ins = visited.insert(k);
                if (!ins.second) {
                    path.resize(top.auto_start);
                    stack.pop_back();
                    continue;
                }

                top.moves.reserve(64);
                legal_moves(top.state, top.moves);
                std::sort(top.moves.begin(), top.moves.end(), MoveCmp{});
                top.move_idx = 0;
                top.moves_initialized = true;
            } else {
                // We're resuming after a child returned failure — undo its move.
                path.resize(top.before_move);
            }

            if (top.move_idx >= top.moves.size()) {
                path.resize(top.auto_start);
                stack.pop_back();
                continue;
            }

            Move m = top.moves[top.move_idx++];
            State next_state = top.state;
            apply_move(next_state, m);
            top.before_move = path.size();
            path.push_back(m);

            Frame child{};
            child.state = next_state;
            child.auto_start = path.size();
            child.moves_initialized = false;
            child.move_idx = 0;
            stack.push_back(std::move(child));
            // `top` is now invalidated; next iteration re-takes the new back().
        }

        return false;
    }
};

// --------- deck / shuffle / init ---------

std::vector<std::vector<Card>> shuffle_deck(std::mt19937& rng) {
    std::vector<Card> deck;
    deck.reserve(40);
    for (Card c = 1; c <= 27; c++) deck.push_back(c);
    for (int i = 0; i < 4; i++) { deck.push_back(RD); deck.push_back(GD); deck.push_back(WD); }
    deck.push_back(FLOWER);
    std::shuffle(deck.begin(), deck.end(), rng);
    std::vector<std::vector<Card>> cols(8);
    for (int i = 0; i < 8; i++) {
        cols[i].reserve(5);
        for (int k = 0; k < 5; k++) cols[i].push_back(deck[i * 5 + k]);
    }
    return cols;
}

State init_state(const std::vector<std::vector<Card>>& deck) {
    State s;
    for (int i = 0; i < 8; i++) {
        for (size_t k = 0; k < deck[i].size(); k++) s.cols[i][k] = deck[i][k];
        s.col_sizes[i] = (uint8_t)deck[i].size();
    }
    return s;
}

bool verify(const std::vector<std::vector<Card>>& deck, const std::vector<Move>& moves) {
    State s = init_state(deck);
    for (auto& m : moves) apply_move(s, m);
    return is_won(s);
}

std::string format_move(const Move& m) {
    std::ostringstream os;
    switch (m.type) {
        case MV_MOVE:
            os << "move " << (int)m.c << " card(s): col " << (int)m.a << " -> col " << (int)m.b; break;
        case MV_COL_TO_CELL:
            os << "col " << (int)m.a << " -> dragon cell " << (int)m.b; break;
        case MV_CELL_TO_COL:
            os << "dragon cell " << (int)m.a << " -> col " << (int)m.b; break;
        case MV_COL_TO_FOUND:
            os << "col " << (int)m.a << " -> foundation"; break;
        case MV_CELL_TO_FOUND:
            os << "dragon cell " << (int)m.a << " -> foundation"; break;
        case MV_COL_TO_FLOWER:
            os << "col " << (int)m.a << " -> flower pile"; break;
        case MV_COLLECT_DRAGONS:
            os << "collect 4 " << "RGW"[m.a] << "-dragons into dragon cell " << (int)m.b; break;
    }
    return os.str();
}

// --------- per-trial result, captured by workers and written to file in order ---------

struct TrialResult {
    bool   found = false;
    double solve_time = 0.0;
    size_t states_visited = 0;
    bool   verified = false;
    std::vector<std::vector<Card>> deck;
    std::vector<Move> path;
};

constexpr int NUM_TRIALS = 10000;

int main(int argc, char** argv) {
    unsigned default_threads = std::thread::hardware_concurrency();
    if (default_threads == 0) default_threads = 1;
    unsigned num_threads = default_threads;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-j" && i + 1 < argc) {
            int v = std::atoi(argv[++i]);
            if (v < 1) v = 1;
            num_threads = (unsigned)v;
        } else if (a == "-h" || a == "--help") {
            std::cout << "Usage: " << argv[0] << " [-j N]\n"
                      << "  -j N : run N solver threads in parallel\n"
                      << "         default on this machine: " << default_threads << "\n"
                      << "         on Apple silicon, try -j <#P-cores> first; E-cores\n"
                      << "         tend to drag tail latency on the hardest deals.\n";
            return 0;
        }
    }

    std::vector<TrialResult> results(NUM_TRIALS);
    std::atomic<int> next_trial{0};

    // Single mutex guards stdout AND the success/fail/completed counters so the
    // per-trial S/F characters, the progress block, and the counters all stay
    // mutually consistent.
    int success = 0, fail = 0, completed = 0;
    std::mutex mtx;

    std::cout << "Running " << NUM_TRIALS << " trials with " << num_threads
              << " thread(s)...\n" << std::flush;

    auto worker = [&]() {
        std::random_device rd;
        std::mt19937 rng(rd());
        Solver solver;
        solver.visited.reserve(200'000);

        while (true) {
            int trial = next_trial.fetch_add(1, std::memory_order_relaxed);
            if (trial >= NUM_TRIALS) return;

            solver.visited.clear();
            solver.path.clear();
            solver.gave_up = false;

            rng.seed(rd());
            auto deck = shuffle_deck(rng);

            auto t0 = std::chrono::high_resolution_clock::now();
            State initial = init_state(deck);
            bool found = solver.dfs(initial);
            auto t1 = std::chrono::high_resolution_clock::now();
            double dt = std::chrono::duration<double>(t1 - t0).count();

            TrialResult& r = results[trial];
            r.found = found;
            r.solve_time = dt;
            r.states_visited = solver.visited.size();
            r.deck = deck;
            if (found) {
                r.path = solver.path;
                r.verified = verify(deck, r.path);
            }

            {
                std::lock_guard<std::mutex> lk(mtx);
                if (found) success++; else fail++;
                completed++;
                //std::cout << (found ? 'S' : 'F') << std::flush;
                //if (completed % 40 == 0 && completed != NUM_TRIALS) {
                //    std::cout << "\ngames     : " << completed << "\n"
                //              << "Successes : " << success << "\n"
                //              << "Failures  : " << fail << "\n\n" << std::flush;
                //}
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (unsigned i = 0; i < num_threads; i++) threads.emplace_back(worker);
    for (auto& t : threads) t.join();

    // Write trial results to file in trial-index order (not completion order),
    // so reruns produce byte-identical files modulo the seeded shuffles.
    std::ofstream of("trial_results_shenzhen.txt");
    for (int i = 0; i < NUM_TRIALS; i++) {
        TrialResult& r = results[i];
        of << "Initial layout:\n";
        for (int c = 0; c < 8; c++) {
            of << "  col " << c << ": [";
            for (size_t k = 0; k < r.deck[c].size(); k++) {
                if (k > 0) of << ", ";
                of << "'" << card_to_str(r.deck[c][k]) << "'";
            }
            of << "]\n";
        }
        of << "\n";
        of << "Solve time: " << r.solve_time << "s, states visited: "
           << r.states_visited << "\n";
        if (!r.found) {
            of << "No solution found\n";
        } else {
            of << "Solution: " << r.path.size() << " moves\n";
            for (size_t k = 0; k < r.path.size(); k++) {
                of << "  " << (k + 1) << ". " << format_move(r.path[k]) << "\n";
            }
            of << "Verified: " << (r.verified ? "True" : "False") << "\n";
        }
    }
    of.close();

    double ratio = (double)success / (double)(success + fail);
    std::cout << "\nSuccesses: " << success << "\n"
              << " Failures: " << fail << "\n"
              << "    ratio: " << ratio << "\n";
    return 0;
}
