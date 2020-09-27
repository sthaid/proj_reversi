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

#define REVERSI_EMPTY  0
#define REVERSI_BLACK  1
#define REVERSI_WHITE  2

#define MOVE_PASS       -1
#define MOVE_GAME_OVER  -2

//
// typedefs
//

typedef struct {
    unsigned char pos[8][8];
} board_t;

typedef struct {
    char name[100];
    int (*get_move)(board_t *b);
} player_t;

//
// variables
//

bool debug_enabled;

extern player_t human;
extern player_t computer;

//
// prototypes
//

void get_possible_moves(board_t *b, int color, int *moves, int *max_moves);
void apply_move(board_t *b, int color, int move, board_t *new_b);


#endif
