# Reversi Game

## References

[Reversi](https://en.wikipedia.org/wiki/Reversi)

[Computer_Othello](https://en.wikipedia.org/wiki/Computer_Othello)

[Minimax_Algorithm](https://en.wikipedia.org/wiki/Minimax#Combinatorial_game_theory)

[Alpha_Beta_Pruning](https://en.wikipedia.org/wiki/Alpha%E2%80%93beta_pruning).

## Program Overview

Source code is on github: [Source_Code](https://github.com/sthaid/proj_reversi)

This program has been tested on Fedora, Ubuntu, and Android.

This program is primarily intended for games between a Human and the
Computer. Computer vs Computer and Human vs Human games are also supported.

## Algorithm

The Alpha-Beta Pruning algorithm is used.

Possible moves and opposing countermoves are considered up to a look ahead depth, or game over. 
The look ahead depth is determined based on CPU difficulty level.

When the look ahead depth is reached, or the game is over, then a heuristic evaluation of the
board is performed. The move is chosen which leads to the highest heuristic value.

The heuristic evaluation takes the following into account, listed in order of importance:
- winning
- number of corner squares
- number of squares diagonally inside an unoccupied corner
- number of possible moves

## Future Impovements

Improve heuristic evaluation. For example check for edge patterns that lead to
corner capture.
