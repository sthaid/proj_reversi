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
#include <pthread.h>
#include <errno.h>
#include <assert.h>

#define DEBUG_PRINT_ENABLED (debug_enabled)
#include <util_misc.h>

//
// defines
//

#define NONE   0
#define BLACK  10
#define WHITE  9

#define MOVE_PASS       -1
#define MOVE_GAME_OVER  -2
#define MOVE_NONE       -9

#define BOARD_EVAL_NONE 999999

#define MOVE_TO_RC(m,r,c) \
    do { \
        (r) = (m) / 10; \
        (c) = (m) % 10; \
    } while (0)
#define RC_TO_MOVE(r,c,m) \
    do { \
        (m) = ((r) * 10) + (c); \
    } while (0)

#define MS 1000L

// xxx FATAL
#define OTHER_COLOR(c)  ((c) == WHITE ? BLACK : WHITE)

#define SWAP(a,b) \
    do { \
        typeof(a) temp;  \
        temp = (a);  \
        (a) = (b);  \
        (b) = temp;  \
    } while (0)

#define WINNER_STR(b) \
    ((b)->black_cnt == (b)->white_cnt ? "TIE"        : \
     (b)->black_cnt >  (b)->white_cnt ? "BLACK-WINS" : \
                                        "WHITE-WINS")

//
// typedefs
//

typedef struct {
    unsigned char pos[10][10];
    int black_cnt;
    int white_cnt;
} board_t;

typedef struct {
    char name[100];
    int (*get_move)(board_t *b, int my_color, int *b_eval);
} player_t;

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

extern player_t human;
extern player_t cpu_1, cpu_2, cpu_3, cpu_4, cpu_5, cpu_6;
extern player_t cpu_random;

//
// prototypes
//

void apply_move(board_t *b, int my_color, int move, bool highlight);
void get_possible_moves(board_t *b, int my_color, possible_moves_t *pm);
bool move_cancelled(void);

#endif
