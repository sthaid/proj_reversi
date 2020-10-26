#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>

#define DEBUG_PRINT_ENABLED (debug_enabled)
#include <util_misc.h>

//
// defines
//

#define NONE   0
#define BLACK  10   // using 'BLACK' and 'WHITE' here to refer to 
#define WHITE  9    // the Reversi Piece; these defines are also in util_sdl.h

#define MOVE_PASS       -1
#define MOVE_GAME_OVER  -2
#define MOVE_NONE       -9

#define MOVE_TO_RC(m,r,c) \
    do { \
        (r) = (m) / 10; \
        (c) = (m) % 10; \
    } while (0)

#define RC_TO_MOVE(r,c,m) \
    do { \
        (m) = ((r) * 10) + (c); \
    } while (0)

#define OTHER_COLOR(c) ((c) == WHITE ? BLACK : \
                        (c) == BLACK ? WHITE : \
                                       ({FATAL("OTHER_COLOR c=%d\n", c);0;}))

#define MS 1000L

#define SWAP(a,b) \
    do { \
        typeof(a) temp;  \
        temp = (a);  \
        (a) = (b);  \
        (b) = temp;  \
    } while (0)

#define MEMBER_SIZE(type, member) sizeof(((type *)0)->member)

//
// typedefs
//

typedef struct {
    unsigned char pos[10][10];
    int black_cnt;
    int white_cnt;
    int whose_turn;
} board_t;

typedef struct {
    int move[64];
    int max;
    int color;
} possible_moves_t;

//
// variables
//

bool debug_enabled;
int human_move_select;

extern char *version;

//
// prototypes
//

void apply_move(board_t *b, int move, unsigned char highlight[][10]);
void get_possible_moves(board_t *b, possible_moves_t *pm);
bool move_cancelled(void);

int human_get_move(int level, board_t *b, char *eval_str);
int cpu_get_move(int level, board_t *b, char *eval_str);
int oldb_get_move(int level, board_t *b, char *eval_str);

//
// inline procedures
//

static inline int min(int a, int b)
{
    return a < b ? a : b;
}

static inline int max(int a, int b)
{
    return a > b ? a : b;
}

#endif
