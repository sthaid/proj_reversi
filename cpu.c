#include <common.h>

typedef struct {
    int val;
    int move;
} val_t;

static int eval(board_t *b, int my_color, int recursion_depth, int max_recursion_depth, int *ret_move);
static int static_eval(board_t *b, int my_color, bool game_over, possible_moves_t *pm);

// -----------------  xxx  --------------------------------------------------

// XXX also adjust the 52
// XXX notes
//  51 is okay for a high level, there are occasional long delays
//  50 is okay for a high level, there are occasional long delays

#define DEFPROC_CPU_GET_MOVE(mrd,xxx) \
static int cpu_##mrd##_get_move(board_t *b, int my_color, int *b_eval) \
{ \
    int move; \
    int max_recursion_depth = (b->black_cnt + b->white_cnt > (xxx) ? 100 : (mrd)); \
    *b_eval = eval(b, my_color, 0, max_recursion_depth, &move); \
    if (move == MOVE_NONE) FATAL("invalid move\n"); \
    return move; \
} \
player_t cpu_##mrd = {"CPU-" #mrd, cpu_##mrd##_get_move};

DEFPROC_CPU_GET_MOVE(1,55)
DEFPROC_CPU_GET_MOVE(2,54)
DEFPROC_CPU_GET_MOVE(3,53)
DEFPROC_CPU_GET_MOVE(4,52)
DEFPROC_CPU_GET_MOVE(5,51)
DEFPROC_CPU_GET_MOVE(6,51)

// -----------------  xxx  --------------------------------------------------

static int eval(board_t *b, int my_color, int recursion_depth, int max_recursion_depth, int *ret_move)
{
    int              other_color = OTHER_COLOR(my_color);
    int              i, min_val, min_val_cnt, dummy_ret_move;
    possible_moves_t pm;
    val_t            val[64];

    // if caller doesn't want ret_move return value then
    // set ret_move to point to dummy variable
    if (ret_move == NULL) {
        ret_move = &dummy_ret_move;
    }

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
    if (recursion_depth == max_recursion_depth) {
        *ret_move = MOVE_NONE;
        return static_eval(b, my_color, false, &pm);
    }

    // if there are no possible moves then
    // return the board eval metric via recursive call
    if (pm.max == 0) {
        int v = eval(b, other_color, recursion_depth+1, max_recursion_depth, NULL);
        *ret_move = MOVE_PASS;
        return -v;
    }

    // loop over possible moves ...
    min_val = INT_MAX;
    for (i = 0; i < pm.max; i++) {
        // make a copy of the board, and apply the move to it,
        // this is now the board that the oppenent needs to choose a move from
        board_t new_board = *b;
        apply_move(&new_board, my_color, pm.move[i], false);

        // obtaine the opponent's board eval metric via recursive call to eval
        val[i].val = eval(&new_board, other_color, recursion_depth+1, max_recursion_depth, NULL);
        val[i].move = pm.move[i];

        // keep track of the min_val, which is used by the code below
        // note: the min_val is used because this player is chosing the
        //       move that gives the opponent its worst position
        if (val[i].val < min_val) {
            min_val = val[i].val;
        }
    }

    // re-order the array of board eval metrics placing all min_val entries at the begining
    min_val_cnt = 0;
    for (i = 0; i < pm.max; i++) {
        if (val[i].val == min_val) {
            SWAP(val[i], val[min_val_cnt]);
            min_val_cnt++;
        }
    }

    // XXX temp test code
    if (min_val_cnt == 0) {
        FATAL("XXX min_val_cnt = 0 \n");
    }
    for (i = 0; i < min_val_cnt; i++) {
        if (val[i].val != min_val) {
            FATAL("XXX val[%d].val=%d min_val=%d\n",
              i, val[i].val, min_val);
        }
    }

    // return move randomly selected from the choices which have board eval equal min_val
    i = (min_val_cnt == 1 ? 0 : (random() % min_val_cnt));
    *ret_move = val[i].move;
    return -min_val;
}

static int static_eval(board_t *b, int my_color, bool game_over, possible_moves_t *pm)
{
    int piece_cnt_diff, corner_cnt_diff;
    int other_color = OTHER_COLOR(my_color);

    // XXX update this comment
    // XXX play this against the original 

    // the static evaluation return value is:
    // - when game is over:
    //   - if winner:  10000 + piece_cnt_diff
    //   - if loser:  -10000 + piece_cnt_diff  (note piece_cnt_diff is negative in this case)
    // - when game is not over
    //   - 1000 * corner_cnt_diff + number_of_possible_moves
    //     for example: 
    //       . my_color is REVERSI_BLACK, 
    //       . black has 2 corner, and white has 1 corner
    //       . black has 5 possible moves
    //     result = 1000 * (2 - 1) + 5 = 1005

    // this evaluation algorithm gives:
    // - first precedence to winning
    // - second precedence to obtaining corners
    // - third precedence to number of possible moved

    piece_cnt_diff = (my_color == REVERSI_BLACK 
                      ? b->black_cnt - b->white_cnt 
                      : b->white_cnt - b->black_cnt);
    if (game_over) {
        if (piece_cnt_diff > 0) {
            return 10000 + piece_cnt_diff;
        } else {
            return -10000 + piece_cnt_diff;
        }
    }

    corner_cnt_diff = 0;
    corner_cnt_diff += (b->pos[1][1] == my_color ? 1 : b->pos[1][1] == other_color ? -1 : 0);
    corner_cnt_diff += (b->pos[1][8] == my_color ? 1 : b->pos[1][8] == other_color ? -1 : 0);
    corner_cnt_diff += (b->pos[8][1] == my_color ? 1 : b->pos[8][1] == other_color ? -1 : 0);
    corner_cnt_diff += (b->pos[8][8] == my_color ? 1 : b->pos[8][8] == other_color ? -1 : 0);

    //return 1000 * corner_cnt_diff + pm->max;
    return 1000 * corner_cnt_diff - piece_cnt_diff;
}
#if 0
static int static_eval(board_t *b, int my_color, bool game_over, possible_moves_t *pm)
{
    int piece_cnt_diff, corner_cnt_diff;
    int other_color = OTHER_COLOR(my_color);

    // the static evaluation return value is:
    // - when game is over:
    //   - if winner:  10000 + piece_cnt_diff
    //   - if loser:  -10000 + piece_cnt_diff  (note piece_cnt_diff is negative in this case)
    // - when game is not over
    //   - 1000 * corner_cnt_diff + number_of_possible_moves
    //     for example: 
    //       . my_color is REVERSI_BLACK, 
    //       . black has 2 corner, and white has 1 corner
    //       . black has 5 possible moves
    //     result = 1000 * (2 - 1) + 5 = 1005

    // this evaluation algorithm gives:
    // - first precedence to winning
    // - second precedence to obtaining corners
    // - third precedence to number of possible moved

    if (game_over) {
        piece_cnt_diff = (my_color == REVERSI_BLACK 
                          ? b->black_cnt - b->white_cnt 
                          : b->white_cnt - b->black_cnt);
        if (piece_cnt_diff > 0) {
            return 10000 + piece_cnt_diff;
        } else {
            return -10000 + piece_cnt_diff;
        }
    }

    corner_cnt_diff = 0;
    corner_cnt_diff += (b->pos[1][1] == my_color ? 1 : b->pos[1][1] == other_color ? -1 : 0);
    corner_cnt_diff += (b->pos[1][8] == my_color ? 1 : b->pos[1][8] == other_color ? -1 : 0);
    corner_cnt_diff += (b->pos[8][1] == my_color ? 1 : b->pos[8][1] == other_color ? -1 : 0);
    corner_cnt_diff += (b->pos[8][8] == my_color ? 1 : b->pos[8][8] == other_color ? -1 : 0);

    return 1000 * corner_cnt_diff + pm->max;
}
#endif
