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
#include <pthread.h>
#include <errno.h>
#include <assert.h>

#define DEBUG_PRINT_ENABLED (debug_enabled)
#include <util_misc.h>

//
// defines
//

#define REVERSI_EMPTY  0 //xxx names
#define REVERSI_BLACK  1
#define REVERSI_WHITE  2

#define MOVE_PASS       -1
#define MOVE_GAME_OVER  -2

#define REVERSI_COLOR_STR(c) \
    ((c) == REVERSI_BLACK ? "BLACK" : \
     (c) == REVERSI_WHITE ? "WHITE" : \
                            "????")

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

//
// typedefs
//

typedef struct {
    unsigned char pos[10][10];
} board_t;

typedef struct {
    char name[100];
    int (*get_move)(board_t *b, int color);
} player_t;

//
// variables
//

bool debug_enabled;
int move_select;  // XXX name

extern player_t human;
extern player_t computer;

//
// prototypes
//

bool apply_move(board_t *b, int my_color, int move);
bool game_restart_requested(void);

#endif
