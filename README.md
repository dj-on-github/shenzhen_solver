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

An example run:

```
% ./shenzhen_solver 
Running 10000 trials with 10 thread(s)...

Successes: 9898
 Failures: 102
    ratio: 0.9898
```

For each game, the deck and the moves to complete the game are written to trial_results_shenzhen.txt.
An example log for a game with a solution sequence of 119 moves:

```
Initial layout:
  col 0: ['RD', 'B4', 'B2', 'RD', 'R5']
  col 1: ['RD', 'G3', 'GD', 'WD', 'GD']
  col 2: ['R7', 'G4', 'B5', 'G2', 'B6']
  col 3: ['WD', 'R6', 'B8', 'B9', 'R4']
  col 4: ['RD', 'R9', 'R2', 'R3', 'B1']
  col 5: ['WD', 'R1', 'G1', 'B3', 'G5']
  col 6: ['G7', 'F', 'R8', 'G6', 'G8']
  col 7: ['B7', 'G9', 'GD', 'GD', 'WD']

Solve time: 0.00053s, states visited: 156
Solution: 119 moves
  1. col 4 -> foundation
  2. move 1 card(s): col 0 -> col 2
  3. move 1 card(s): col 3 -> col 5
  4. move 1 card(s): col 6 -> col 3
  5. move 1 card(s): col 2 -> col 6
  6. move 2 card(s): col 5 -> col 2
  7. move 1 card(s): col 5 -> col 2
  8. col 5 -> foundation
  9. col 5 -> foundation
  10. col 0 -> dragon cell 0
  11. col 0 -> foundation
  12. col 2 -> foundation
  13. col 0 -> foundation
  14. col 0 -> dragon cell 1
  15. move 3 card(s): col 2 -> col 0
  16. col 2 -> foundation
  17. col 2 -> foundation
  18. move 1 card(s): col 2 -> col 6
  19. move 3 card(s): col 0 -> col 2
  20. move 4 card(s): col 2 -> col 3
  21. move 6 card(s): col 3 -> col 0
  22. move 5 card(s): col 0 -> col 2
  23. move 4 card(s): col 2 -> col 3
  24. move 1 card(s): col 2 -> col 0
  25. move 5 card(s): col 3 -> col 2
  26. move 4 card(s): col 2 -> col 0
  27. move 2 card(s): col 0 -> col 3
  28. col 0 -> foundation
  29. move 3 card(s): col 6 -> col 0
  30. move 4 card(s): col 0 -> col 2
  31. move 1 card(s): col 4 -> col 2
  32. col 4 -> foundation
  33. col 2 -> foundation
  34. col 3 -> foundation
  35. move 5 card(s): col 2 -> col 4
  36. move 6 card(s): col 4 -> col 2
  37. move 4 card(s): col 2 -> col 0
  38. col 2 -> dragon cell 2
  39. move 5 card(s): col 0 -> col 2
  40. move 1 card(s): col 6 -> col 0
  41. col 6 -> flower pile
  42. move 2 card(s): col 3 -> col 6
  43. move 3 card(s): col 6 -> col 0
  44. move 5 card(s): col 2 -> col 6
  45. dragon cell 2 -> col 2
  46. move 4 card(s): col 6 -> col 2
  47. col 0 -> dragon cell 2
  48. move 4 card(s): col 2 -> col 6
  49. move 2 card(s): col 0 -> col 2
  50. dragon cell 2 -> col 2
  51. col 0 -> dragon cell 2
  52. move 5 card(s): col 6 -> col 0
  53. move 4 card(s): col 2 -> col 6
  54. move 5 card(s): col 0 -> col 2
  55. dragon cell 2 -> col 0
  56. move 3 card(s): col 6 -> col 0
  57. move 4 card(s): col 2 -> col 6
  58. col 2 -> dragon cell 2
  59. move 5 card(s): col 6 -> col 2
  60. move 4 card(s): col 0 -> col 6
  61. dragon cell 2 -> col 0
  62. move 4 card(s): col 2 -> col 0
  63. move 3 card(s): col 6 -> col 2
  64. col 3 -> dragon cell 2
  65. move 5 card(s): col 0 -> col 3
  66. move 3 card(s): col 2 -> col 6
  67. move 5 card(s): col 3 -> col 0
  68. move 4 card(s): col 0 -> col 2
  69. move 5 card(s): col 2 -> col 3
  70. move 4 card(s): col 3 -> col 0
  71. move 5 card(s): col 0 -> col 2
  72. move 4 card(s): col 2 -> col 3
  73. move 4 card(s): col 6 -> col 0
  74. move 4 card(s): col 3 -> col 2
  75. move 5 card(s): col 2 -> col 6
  76. move 4 card(s): col 6 -> col 3
  77. move 5 card(s): col 3 -> col 2
  78. move 4 card(s): col 2 -> col 3
  79. move 4 card(s): col 3 -> col 6
  80. move 3 card(s): col 0 -> col 2
  81. move 4 card(s): col 2 -> col 3
  82. move 5 card(s): col 6 -> col 2
  83. move 1 card(s): col 1 -> col 6
  84. collect 4 W-dragons into dragon cell 2
  85. move 5 card(s): col 2 -> col 5
  86. move 4 card(s): col 3 -> col 2
  87. move 4 card(s): col 5 -> col 3
  88. move 3 card(s): col 2 -> col 0
  89. move 4 card(s): col 3 -> col 2
  90. move 5 card(s): col 2 -> col 3
  91. move 4 card(s): col 3 -> col 5
  92. move 5 card(s): col 5 -> col 2
  93. move 4 card(s): col 0 -> col 5
  94. move 5 card(s): col 2 -> col 0
  95. move 4 card(s): col 0 -> col 3
  96. move 5 card(s): col 3 -> col 2
  97. move 4 card(s): col 2 -> col 0
  98. move 5 card(s): col 0 -> col 3
  99. move 4 card(s): col 5 -> col 0
  100. move 1 card(s): col 1 -> col 5
  101. col 1 -> foundation
  102. col 3 -> foundation
  103. col 0 -> foundation
  104. col 3 -> foundation
  105. col 0 -> foundation
  106. col 3 -> foundation
  107. col 0 -> foundation
  108. col 3 -> foundation
  109. collect 4 R-dragons into dragon cell 0
  110. col 0 -> foundation
  111. col 3 -> foundation
  112. move 1 card(s): col 2 -> col 1
  113. col 2 -> foundation
  114. move 1 card(s): col 7 -> col 2
  115. collect 4 G-dragons into dragon cell 1
  116. col 7 -> foundation
  117. col 7 -> foundation
  118. col 1 -> foundation
  119. col 0 -> foundation
Verified: True
```


