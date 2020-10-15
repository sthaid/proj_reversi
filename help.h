//       1         2         3
// 456789 123456789 123456789 12

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
When MOVE is enabled the move and\n\
flipping pieces will highlight.\n\
\n\
============\n\
SOURCE CODE\n\
============\n\
\n\
github.com/sthaid/proj_reversi\n\
\n\
Tested on: Android, Fedora 31,\n\
and Ubuntu XXX.\n\
\n\
============\n\
CPU ALGORITHM\n\
============\n\
\n\
The CPU<N> player algorithm\n\
applies all possible moves and\n\
opposing countermoves up to a\n\
look ahead level. The look ahead\n\
level is determined based on\n\
difficulty level 'N'.\n\
\n\
When the look ahead level is\n\
reached, or the game is over,\n\
then a static evaluation of the\n\
board is performed. The move is\n\
chosen which leads to the\n\
highest static evaluation.\n\
\n\
The look ahead depth depends on\n\
difficulty level 'N'. Example,\n\
when N=5 the look ahead level is\n\
5, except when there are greater\n\
than 51 pieces on the board then\n\
the look ahead terminates when\n\
the game is over.\n\
\n\
The static evaluator takes the\n\
following into account, these\n\
criteria are listed in order of\n\
importance:\n\
- winning\n\
- number of corner squares\n\
- number of squares diagonally\n\
  inside an unoccupied corner\n\
- number of possible moves\n\
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
The results are as expected, the\n\
CPU<N> players with the higher\n\
'N' win a higher percentage of\n\
their games.\n\
\n\
The primary purpose of tournament\n\
mode is to test new static\n\
evaluation algorithms. Additional\n\
static evaluators can be added to\n\
cpu.c, and CPU players defined to\n\
use the new static evaluator.\n\
";
