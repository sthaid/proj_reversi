#include <common.h>

//
// defines
//

#define INFIN             INT64_MAX
#define RANDOMIZE_OPENING 20

//xxx del
//#define ONE64 ((int64_t)1)
//#define BIT64(n)   (ONE64 << (n))
//#define BYTE64(n)  (ONE64 << (8*(n)))

//
// variables
//

//
// prototypes
//

static void create_eval_str(int64_t value, char *eval_str);
static int64_t alphabeta(board_t *b, int depth, int64_t alpha, int64_t beta, bool maximizing_player, int *move);
static int64_t heuristic(board_t *b, bool maximizing_player, bool game_over, possible_moves_t *pm);

// -----------------  CPU PLAYER - GET_MOVE ---------------------------------

// xxx comments needed in this routine
#ifndef OLD
int cpu_get_move(int level, board_t *b, char *eval_str)
#else
int old_get_move(int level, board_t *b, char *eval_str)
#endif
{
    int64_t value;
    int     move, depth, piececnt;
    double  M, B;

    static int MIN_DEPTH[9] =               {0,  1,  2,  3,  4,  5,  6,  7,  8 };
    static int PIECECNT_FOR_EOG_DEPTH[9]  = {0, 56, 55, 54, 53, 52, 51, 50, 49 };

    if (level < 1 || level > 8) {
        FATAL("invlaid level %d\n", level);
    }

    move = bm_get_move(b);
    if (move != MOVE_NONE) {
        INFO("GOT BOOK MOVE %d\n", move);  // xxx temp print
        if (eval_str) {
            eval_str[0] = '\0';
        }
        return move;
    }

    piececnt = b->black_cnt + b->white_cnt;
    M = 1.0;
    B = (64 - PIECECNT_FOR_EOG_DEPTH[level]) - M * PIECECNT_FOR_EOG_DEPTH[level];
    depth = rint(M * piececnt + B);
    if (depth < MIN_DEPTH[level]) depth = MIN_DEPTH[level];

    value = alphabeta(b, depth, -INFIN, +INFIN, true, &move);

    create_eval_str(value, eval_str);
    return move;
}

// -----------------  CREATE GAME FORECAST EVALUATION STRING  ----------------

//xxx fix this 
static void create_eval_str(int64_t value, char *eval_str)
{
    if (eval_str == NULL) {
        return;
    }

    // eval_str should not exceed 16 char length, 
    // to avoid characters being off the window
    if (value > 10000000) {
        sprintf(eval_str, "CPU TO WIN BY %d", (int)(value-10000000));
    } else if (value == 10000000) {
        sprintf(eval_str, "TIE");
    } else if (value < -10000000) {
        sprintf(eval_str, "HUMAN CAN WIN BY %d", (int)(-value-10000000));
    } else if (value > 50000) {
        sprintf(eval_str, "CPU ADVANTAGE");
    } else if (value < -50000) {
        sprintf(eval_str, "HUMAN ADVANTAGE");
    } else {
        eval_str[0] = '\0';
    }
}

// -----------------  CHOOSE BEST MOVE (RECURSIVE ROUTINE)  -----------------

static int64_t alphabeta(board_t *b, int depth, int64_t alpha, int64_t beta, bool maximizing_player, int *move)
{
    #define CHILD(mv) \
        ({ b_child = *b; \
           apply_move(&b_child, mv, NULL); \
           &b_child; })

    int64_t          value, v;
    int              i, best_move = MOVE_NONE;
    board_t          b_child;
    bool             game_over;
    possible_moves_t pm;

    // if move is cancelled then just return, this 
    // happens when the game is being reset or restarted
    if (move_cancelled()) {
        if (move) *move = MOVE_NONE;
        return 0;
    }

    // determine values for game_over and pm (possible moves)
    //
    // if all squares are filled then
    //   the game is over and there are no possible moves
    // else
    //   Call get_possible_moves to obtain a list of the possible moves.
    //   If there are no possible moves this means that either the game is
    //    over or the player must pass. The determination of which is made by
    //    checking if the opponent has possible moves. If the oppenent has no
    //    possible moves then the game is over. If the opponent has possible
    //    moves then this player must pass.
    // endif
    game_over = false;
    if (b->black_cnt + b->white_cnt == 64) {
        game_over = true;
        pm.max = 0;
    } else {
        get_possible_moves(b, &pm);
        if (pm.max == 0) {
            possible_moves_t other_pm;
            get_possible_moves(CHILD(MOVE_PASS), &other_pm);
            game_over = (other_pm.max == 0);
            if (!game_over) {
                pm.max = 1;
                pm.move[0] = MOVE_PASS;
            }
        }
    }

    // 
    // The following is the alpha-beta pruning algorithm, ported from
    //  https://en.wikipedia.org/wiki/Alpha%E2%80%93beta_pruning
    // The algorithm has been updated to return the best_move along with the value.
    // 

    if (depth == 0 || game_over) {
        if (move) *move = (game_over ? MOVE_GAME_OVER : MOVE_NONE);
        return heuristic(b, maximizing_player, game_over, &pm);
    }

    if (maximizing_player) {
        value = -INFIN;
        for (i = 0; i < pm.max; i++) {
            if ((v = alphabeta(CHILD(pm.move[i]), depth-1, alpha, beta, false, NULL)) > value) {
                value = v;
                best_move = pm.move[i];
            }
            alpha = max64(alpha, value);
            if (alpha >= beta) {
                break;
            }
        }
    } else {
        value = +INFIN;
        for (i = 0; i < pm.max; i++) {
            if ((v = alphabeta(CHILD(pm.move[i]), depth-1, alpha, beta, true, NULL)) < value) {
                value = v;
                best_move = pm.move[i];
            }
            beta = min64(beta, value);
            if (beta <= alpha) {
                break;
            }
        }
    }

    if (move) *move = best_move;
    return value;
}

// -----------------  HEURISTIC  ---------------------------------------------------

// XXX get my_color from board,  and dont pass these my_color and other_color
static inline int64_t corner_count(board_t *b, int my_color, int other_color)
{
    int cnt = 0;
    if (b->pos[1][1] == my_color) cnt++;
    if (b->pos[1][1] == other_color) cnt--;
    if (b->pos[1][8] == my_color) cnt++;
    if (b->pos[1][8] == other_color) cnt--;
    if (b->pos[8][1] == my_color) cnt++;
    if (b->pos[8][1] == other_color) cnt--;
    if (b->pos[8][8] == my_color) cnt++;
    if (b->pos[8][8] == other_color) cnt--;
    return cnt;
}

static inline int64_t corner_moves(board_t *b)
{
    int cnt = 0;
    int my_color = b->whose_turn;
    int other_color = OTHER_COLOR(my_color);
    int which_corner;

    for (which_corner = 0; which_corner < 4; which_corner++) {
        if (is_corner_move_possible(b, which_corner)) cnt++;
    }

    b->whose_turn = other_color;
    for (which_corner = 0; which_corner < 4; which_corner++) {
        if (is_corner_move_possible(b, which_corner)) cnt--;
    }
    b->whose_turn = my_color;

    return cnt;
}

static inline int64_t diagnol_gateways_to_corner(board_t *b, int my_color, int other_color)
{
    int cnt = 0;

    if (b->pos[1][1] == NONE) {
        if (b->pos[2][2] == other_color) cnt++;
        if (b->pos[2][2] == my_color) cnt--;
    }
    if (b->pos[1][8] == NONE) {
        if (b->pos[2][7] == other_color) cnt++;
        if (b->pos[2][7] == my_color) cnt--;
    }
    if (b->pos[8][1] == NONE) {
        if (b->pos[7][2] == other_color) cnt++;
        if (b->pos[7][2] == my_color) cnt--;
    }
    if (b->pos[8][8] == NONE) {
        if (b->pos[7][7] == other_color) cnt++;
        if (b->pos[7][7] == my_color) cnt--;
    }

    return cnt;
}

static inline int64_t edge_gatewas_to_corner(board_t *b, int my_color, int other_color)
{
    int cnt = 0;
    return cnt;
}

static inline int64_t reasonable_moves(board_t *b, possible_moves_t *pm)
{
    int i, cnt = pm->max;

    // XXX others, 
    // - moves on edge that give a corner

    for (i = 0; i < pm->max; i++) {
        int r,c;
        MOVE_TO_RC(pm->move[i],r,c);
        if ((r == 2 && c == 2 && b->pos[1][1] == NONE) ||
            (r == 2 && c == 7 && b->pos[1][8] == NONE) ||
            (r == 7 && c == 2 && b->pos[8][1] == NONE) ||
            (r == 7 && c == 7 && b->pos[8][8] == NONE))
        {
            cnt--;
        }
    }

    return cnt;
}

// - - - - - - - - - - - - - - - - - - 

static int64_t heuristic(board_t *b, bool maximizing_player, bool game_over, possible_moves_t *pm)
{
    int64_t value;
    int my_color = b->whose_turn;
    int other_color = OTHER_COLOR(my_color);

    // handle game over case
    if (game_over) {
        int64_t piece_cnt_diff;

        // the game is over, return a large positive or negative value, 
        // incorporating by how many pieces the game has been won or lost
        piece_cnt_diff = (my_color == BLACK ? b->black_cnt - b->white_cnt
                                            : b->white_cnt - b->black_cnt);
        if (piece_cnt_diff >= 0) {
            value = (piece_cnt_diff+1) << 56;
        } else {
            value = (piece_cnt_diff-1) << 56;
        }

        // the returned heuristic value measures the favorability 
        // for the maximizing player
        return (maximizing_player ? value : -value);
    }

    // game is not over ...

    // - corner count
    // - corner moves
    // - diagnol gateways to corner
    // - edge gateways to corner
    // - reasonable moves
    // - random value so that the same move is not always chosen

    // before
    // - corner
    // - gateway to corner
    // - reasonable moves

    value = 0;
    value += (corner_count(b, my_color, other_color) << 48);
    value += (corner_moves(b) << 40);
    value += (diagnol_gateways_to_corner(b, my_color, other_color) << 32);
    value += (edge_gatewas_to_corner(b, my_color, other_color) << 24);
    value += (reasonable_moves(b, pm) << 16);
    if (b->black_cnt + b->white_cnt < RANDOMIZE_OPENING) {
        value += ((random() & 127) - 64);
    }

    // the returned heuristic value measures the favorability 
    // for the maximizing player
    return (maximizing_player ? value : -value);
}

#ifndef OLD
// -----------------  BOOK MOVE GENERATOR  -----------------------------------------

int cpu_book_move_generator(board_t *b)
{
    int depth=12;
    int move;

    alphabeta(b, depth, -INFIN, +INFIN, true, &move);
    return move;
}
#endif
