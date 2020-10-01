#include <common.h>

#define MAX_RECURSION_DEPTH  5

static int cpu_get_move(board_t *b, int color);
static int eval(board_t *b, int my_color, int recursion_depth, int *ret_move);
static int static_eval(board_t *b, int my_color, bool game_over, possible_moves_t *pm);

player_t cpu = {"cpu", cpu_get_move};

// -----------------  xxx  --------------------------------------------------

static int cpu_get_move(board_t *b, int my_color)
{
    int move;

    // xxx also get move back
    eval(b, my_color, 0, &move);

    // xxx sanity check it is a valid move

    // return move
    return move;
}

// -----------------  xxx  --------------------------------------------------

static int eval(board_t *b, int my_color, int recursion_depth, int *ret_move)
{
    int              other_color = OTHER_COLOR(my_color);
    int              min_val, val[64], i, best_move, dummy_ret_move;
    possible_moves_t pm;

    // xxx debug prints
    //INFO("called for %s\n", REVERSI_COLOR_STR(my_color));

    // xxx comment
    if (ret_move == NULL) {
        ret_move = &dummy_ret_move;
    }

    // if all squares are used then GAME_OVER
    if (b->black_cnt + b->white_cnt == 64) {
        *ret_move = MOVE_GAME_OVER;
        return static_eval(b, my_color, true, NULL);
    }

    // get possible moves
    get_possible_moves(b, my_color, &pm);

    // if there are no possible moves then the code 
    // needs to either PASS or GAME_OVER
    if (pm.max == 0) {
        possible_moves_t other_pm;
        get_possible_moves(b, other_color, &other_pm);
        *ret_move = MOVE_GAME_OVER;
        return static_eval(b, my_color, true, NULL);
    }

    // if reached recursion_depth then  ...
    if (recursion_depth >= MAX_RECURSION_DEPTH) {
        // xxx print of recur_depth is >
        *ret_move = MOVE_NONE;
        return static_eval(b, my_color, false, &pm);
    }

    // xxx comments

    // pass
    if (pm.max == 0) {
        val[0] = eval(b, other_color, recursion_depth+1, NULL);
        *ret_move = MOVE_PASS;
        return -val[0];
    }

    // loop over possible moves
    for (i = 0; i < pm.max; i++) {
        board_t new_board;

        // make a new board, and apply the move to it
        new_board = *b;
        apply_move(&new_board, my_color, pm.move[i]);

        // evaluate the new board from perspective of the other color
        val[i] = eval(&new_board, other_color, recursion_depth+1, NULL);
    }

    // choose the minimum of the evaluations
    min_val = INT_MAX;
    best_move = MOVE_NONE;
    for (i = 0; i < pm.max; i++) {
        if (val[i] < min_val) {
            min_val   = val[i];
            best_move = pm.move[i];
        }
    }

    // xxx
    *ret_move = best_move;
    return -min_val;
}

static int static_eval(board_t *b, int my_color, bool game_over, possible_moves_t *pm)
{
    int piece_cnt_diff, corner_cnt_diff;
    int other_color = OTHER_COLOR(my_color);

    // xxx print evals

    if (game_over) {
        piece_cnt_diff = (my_color == REVERSI_BLACK 
                          ? b->black_cnt - b->white_cnt 
                          : b->white_cnt - b->black_cnt);
        if (piece_cnt_diff > 0) {
            return 1000000 + piece_cnt_diff;
        } else {
            return -1000000 + piece_cnt_diff;
        }
    }

    corner_cnt_diff = 0;
    corner_cnt_diff += (b->pos[1][1] == my_color ? 1 : b->pos[1][1] == other_color ? -1 : 0);
    corner_cnt_diff += (b->pos[1][8] == my_color ? 1 : b->pos[1][8] == other_color ? -1 : 0);
    corner_cnt_diff += (b->pos[8][1] == my_color ? 1 : b->pos[8][1] == other_color ? -1 : 0);
    corner_cnt_diff += (b->pos[8][8] == my_color ? 1 : b->pos[8][8] == other_color ? -1 : 0);

    return corner_cnt_diff * 1000 + pm->max;
}
