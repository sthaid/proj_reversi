#include <common.h>

// XXX should be odd
#define MAX_RECURSION_DEPTH  5

static int cpu_get_move(board_t *b, int color);
static int eval(board_t *b, int my_color, int recursion_depth, int *ret_move);
static int static_eval(board_t *b, int my_color, bool game_over, possible_moves_t *pm);

player_t cpu = {"cpu", cpu_get_move};

// -----------------  xxx  --------------------------------------------------

static int cpu_get_move(board_t *b, int my_color)
{
    int move;

    eval(b, my_color, 0, &move);

    if (move == MOVE_NONE) {
        FATAL("invalid move\n");
    }

    return move;
}

// -----------------  xxx  --------------------------------------------------

static int eval(board_t *b, int my_color, int recursion_depth, int *ret_move)
{
    int              other_color = OTHER_COLOR(my_color);
    int              min_val, val[64], i, best_move, dummy_ret_move;
    possible_moves_t pm;

    // if caller doesn't want ret_move return value then
    // set ret_move to point to dummy variable
    if (ret_move == NULL) {
        ret_move = &dummy_ret_move;
    }

    // XXX use empty_cnt
    // if all squares are used then the game is over, so
    // return the static board evaluation metric;
    // note: this code is a performance optimization
    if (b->black_cnt + b->white_cnt == 64) {
        *ret_move = MOVE_GAME_OVER;
        return static_eval(b, my_color, true, NULL);
    }

    // get possible moves
    get_possible_moves(b, my_color, &pm);

    // if there are no possible moves then the game might be over;
    // determine whether the game is over by checking if the other_color
    //  has any possible moves;
    // if the game is over return the static board evaluation metric
    if (pm.max == 0) {
        possible_moves_t other_pm;
        get_possible_moves(b, other_color, &other_pm);
        if (other_pm.max == 0) {
            *ret_move = MOVE_GAME_OVER;
            return static_eval(b, my_color, true, NULL);
        }
    }

    // if reached maximum recursion_depth then 
    // return the static board evaluation metric
    if (recursion_depth == MAX_RECURSION_DEPTH) {
        *ret_move = MOVE_NONE;
        return static_eval(b, my_color, false, &pm);
    }

    // if there are no possible moves then
    // return the board eval metric via recursive call
    if (pm.max == 0) {
        int v = eval(b, other_color, recursion_depth+1, NULL);
        *ret_move = MOVE_PASS;
        return -v;
    }

    // loop over possible moves
    // - make a copy of the board, and apply the move to it
    // - obtaine the board eval metric via recursive call
    for (i = 0; i < pm.max; i++) {
        board_t new_board = *b;
        apply_move(&new_board, my_color, pm.move[i]);
        val[i] = eval(&new_board, other_color, recursion_depth+1, NULL);
    }

    // using the array of board eval metrics obtained above, find the
    // minimum; the minimum is chosen because this player is chosing the
    // move that gives the opponent its worst position
    min_val = INT_MAX;
    best_move = MOVE_NONE;
    for (i = 0; i < pm.max; i++) {
        if (val[i] < min_val) {
            min_val   = val[i];
            best_move = pm.move[i];
        }
    }

    // return the board eval metric for this player's best_move
    // which was chosen by the above loop
    *ret_move = best_move;
    return -min_val;
}

static int static_eval(board_t *b, int my_color, bool game_over, possible_moves_t *pm)
{
    int piece_cnt_diff, corner_cnt_diff;
    int other_color = OTHER_COLOR(my_color);

    if (game_over) {
        piece_cnt_diff = (my_color == REVERSI_BLACK 
                          ? b->black_cnt - b->white_cnt 
                          : b->white_cnt - b->black_cnt);
        if (piece_cnt_diff > 0) {
            return 1000 + piece_cnt_diff;
        } else {
            return -1000 + piece_cnt_diff;
        }
    }

    corner_cnt_diff = 0;
    corner_cnt_diff += (b->pos[1][1] == my_color ? 1 : b->pos[1][1] == other_color ? -1 : 0);
    corner_cnt_diff += (b->pos[1][8] == my_color ? 1 : b->pos[1][8] == other_color ? -1 : 0);
    corner_cnt_diff += (b->pos[8][1] == my_color ? 1 : b->pos[8][1] == other_color ? -1 : 0);
    corner_cnt_diff += (b->pos[8][8] == my_color ? 1 : b->pos[8][8] == other_color ? -1 : 0);

    return corner_cnt_diff * 100 + pm->max;
}
