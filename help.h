//       1         2         3
// 456789 123456789 123456789 123456

char help_text[] =
"\
\n\
============\n\
CONTROLS\n\
============\n\
\n\
Text displayed in light blue can\n\
be selected.\n\
\n\
To change the players the game\n\
must not be started.\n\
\n\
When EVAL is enabled, in a HUMAN\n\
vs CPU game, the CPU's evaluation\n\
will be displayed.\n\
\n\
When SHOW is enabled the move and\n\
flipping pieces will highlight.\n\
\n\
When BOOK is enabled the CPU's \n\
move may be obtained from a\n\
preexisting book move file.\n\
\n\
============\n\
SOURCE CODE\n\
============\n\
\n\
github.com/sthaid/proj_reversi\n\
\n\
Tested on: Android, Fedora 31,\n\
and Ubuntu 20.04.1 LTS.\n\
\n\
============\n\
CPU ALGORITHM\n\
============\n\
\n\
The CPU<N> player algorithm\n\
applies possible moves and\n\
opposing countermoves up to a\n\
look ahead level, or game over.\n\
Look ahead level is determined\n\
based on difficulty level 'N'.\n\
\n\
When the look ahead level is\n\
reached, or the game is over,\n\
then a heuristic evaluation of\n\
the board is performed. The move\n\
is chosen which leads to the\n\
highest heuristic value.\n\
\n\
============\n\
TOURNAMENT\n\
============\n\
\n\
Selecting GAME will enter\n\
TOURNAMENT mode. In tournament\n\
mode the different CPU<N> players\n\
are pitted against each other.\n\
The percentage of games won is\n\
displayed for each.\n\
\n\
A primary purpose of tournament\n\
mode is to test new heuristic\n\
evaluators.\n\
";
