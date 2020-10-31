#include <common.h>

//
// defines
//

#define INFIN             INT_MAX
#define RANDOMIZE_OPENING 20

//
// variables
//

static int (*heuristic)(board_t *b, bool maximizing_player, bool game_over, possible_moves_t *pm);

//
// prototypes
//

static void create_eval_str(int eval_int, char *eval_str);
static int alphabeta(board_t *b, int depth, int alpha, int beta, bool maximizing_player, int *move);
static int heuristic_a(board_t *b, bool maximizing_player, bool game_over, possible_moves_t *pm);

// -----------------  CPU PLAYER - GET_MOVE ---------------------------------

// xxx comments

int cpu_get_move(int level, board_t *b, char *eval_str)
{
    int    move, value, depth, piececnt;
    double M, B;

    static int MIN_DEPTH[9] =               {0,  1,  2,  3,  4,  5,  6,  7,  8 };
    static int PIECECNT_FOR_EOG_DEPTH[9]  = {0, 56, 55, 54, 53, 52, 51, 50, 49 };

    if (level < 1 || level > 8) {
        FATAL("invlaid level %d\n", level);
    }

    // XXX also get value and create_eval_str
    move = bm_get_move(b);
    if (move != MOVE_NONE) {
        INFO("XXX GOT BOOK MOVE %d\n", move);
        return move;
    }

    piececnt = b->black_cnt + b->white_cnt;
    M = 1.0;
    B = (64 - PIECECNT_FOR_EOG_DEPTH[level]) - M * PIECECNT_FOR_EOG_DEPTH[level];
    depth = rint(M * piececnt + B);
    if (depth < MIN_DEPTH[level]) depth = MIN_DEPTH[level];

    heuristic = heuristic_a;
    value = alphabeta(b, depth, -INFIN, +INFIN, true, &move);

    create_eval_str(value, eval_str);
    return move;
}

int cpu_book_move_generator(board_t *b)
{
    int move, depth=5;  // XXX adjust depth

    heuristic = heuristic_a;
    alphabeta(b, depth, -INFIN, +INFIN, true, &move);
    return move;
}

// -----------------  CREATE GAME FORECAST EVALUATION STRING  ----------------

static void create_eval_str(int eval_int, char *eval_str)
{
    if (eval_str == NULL) {
        return;
    }

    // eval_str should not exceed 16 char length, 
    // to avoid characters being off the window
    if (eval_int > 10000000) {
        sprintf(eval_str, "CPU TO WIN BY %d", eval_int-10000000);
    } else if (eval_int == 10000000) {
        sprintf(eval_str, "TIE");
    } else if (eval_int < -10000000) {
        sprintf(eval_str, "HUMAN CAN WIN BY %d", -eval_int-10000000);
    } else if (eval_int > 50000) {
        sprintf(eval_str, "CPU ADVANTAGE");
    } else if (eval_int < -50000) {
        sprintf(eval_str, "HUMAN ADVANTAGE");
    } else {
        eval_str[0] = '\0';
    }
}

// -----------------  CHOOSE BEST MOVE (RECURSIVE ROUTINE)  -----------------

static int alphabeta(board_t *b, int depth, int alpha, int beta, bool maximizing_player, int *move)
{
    #define CHILD(mv) \
        ({ b_child = *b; \
           apply_move(&b_child, mv, NULL); \
           &b_child; })

    int              i, value, v, best_move = MOVE_NONE;
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
            alpha = max(alpha, value);
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
            beta = min(beta, value);
            if (beta <= alpha) {
                break;
            }
        }
    }

    if (move) *move = best_move;
    return value;
}

// -----------------  HEURISTIC  ---------------------------------------------------

#define EVAL_CORNER(r,c) \
    do { \
        if (b->pos[r][c] == NONE) { \
            ; \
        } else if (b->pos[r][c] == my_color) { \
            corner_cnt_my_color++; \
        } else { \
            corner_cnt_other_color++; \
        } \
    } while (0) 

#define EVAL_GATEWAY_TO_CORNER(r,c,rin,cin) \
    do { \
        if (b->pos[r][c] != NONE || b->pos[rin][cin] == NONE) { \
            ; \
        } else if (b->pos[rin][cin] == my_color) { \
            gateway_cnt_my_color++; \
        } else { \
            gateway_cnt_other_color++; \
        } \
    } while (0)


static int heuristic_a(board_t *b, bool maximizing_player, bool game_over, possible_moves_t *pm)
{
    int value;
    int piece_cnt_diff;
    int corner_cnt_my_color = 0, corner_cnt_other_color = 0;
    int gateway_cnt_my_color = 0, gateway_cnt_other_color = 0;
    int my_color = b->whose_turn;

    if (game_over) {
        // the game is over, return a large positive or negative value, 
        // incorporating by how many pieces the game has been won or lost
        piece_cnt_diff = (my_color == BLACK ? b->black_cnt - b->white_cnt
                                            : b->white_cnt - b->black_cnt);
        if (piece_cnt_diff >= 0) {
            value =  10000000 + piece_cnt_diff;
        } else {
            value = -10000000 + piece_cnt_diff;
        }
    } else {
        // game is not over ...

        // make count of number of corners occupied by my_color and other_color
        EVAL_CORNER(1,1);
        EVAL_CORNER(1,8);
        EVAL_CORNER(8,1);
        EVAL_CORNER(8,8);

        // make count of number of squares that are diagonally adjacent to
        // an unoccupied corner, that are occupied by my_color or other_color
        EVAL_GATEWAY_TO_CORNER(1,1, 2,2);
        EVAL_GATEWAY_TO_CORNER(1,8, 2,7);
        EVAL_GATEWAY_TO_CORNER(8,1, 7,2);
        EVAL_GATEWAY_TO_CORNER(8,8, 7,7);

        value = (1000000 * (corner_cnt_my_color - corner_cnt_other_color)) +
                (-100000 * (gateway_cnt_my_color - gateway_cnt_other_color)) +
                (100     * pm->max);

        // so that cpu vs cpu games are not always the same, 
        // add a random value to the value computed above during
        // the early portion of the game; this random value is small
        // enough so that it only affects the move chosen by alpha-beta
        // when the heuristic would otherwise have been the same 
        if (b->black_cnt + b->white_cnt < RANDOMIZE_OPENING) {
            value += ((random() & 127) - 64);
        }
    }

    // the returned heuristic value measures the favorability 
    // for the maximizing player
    return (maximizing_player ? value : -value);
}
