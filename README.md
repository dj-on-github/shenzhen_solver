# shenzhen_solver
A solver for Shenzhen Solitaire (find it on Steam).

The purpose is to find the true solvability rate for the Shenzhen
Solitaire game. Humans are not good at solving the more complex deals
so the perception of solvability by human players is closer to 50%
while the actual solvability is certainly rate above 98%.

This uses pthreads to run many simulated games and determines whether or
not the game can be solved. It returns the pass/fail rate. It defaults
to 10,000 runs.

This is fairly slow. Some games resolve quickly and some have a very deep
solution path over 500 moves long. That is why pthreads was used to speed
it up a bit.

For example on a 2025 macbook with an M4 cpu and 10 cores it takes this
long:

```
% time ./shenzhen_solver
Running 10000 trials with 10 thread(s)...

Successes: 9903
 Failures: 97
    ratio: 0.9903
./shenzhen_solver  396.68s user 4.73s system 576% cpu 1:09.62 total
```

There is a -j <threads> option to set the number of threads.

```
Usage: ./shenzhen_solver [-j N]
  -j N : run N solver threads in parallel
         default on this machine: 10
         on Apple silicon, try -j <#P-cores> first; E-cores
         tend to drag tail latency on the hardest deals.
```

Compile with


``` g++ -O3 -std=c++17 -Wall -pthread shenzhen.cpp -o shenzhen```

