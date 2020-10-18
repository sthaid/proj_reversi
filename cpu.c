#include <common.h>

//
// defines
//

#define INFIN             INT_MAX
#define RANDOMIZE_OPENING 20

//
// variables
//

static int maximizing_color;
static int (*heuristic)(board_t *b, bool maximizing_player, bool game_over, possible_moves_t *pm);

//
// prototypes
//

static void create_eval_str(int eval_int, char *eval_str);
static int alphabeta(board_t *b, int depth, int alpha, int beta, bool maximizing_player, int *move);
static int heuristic_a(board_t *b, bool maximizing_player, bool game_over, possible_moves_t *pm);

// -----------------  CPU PLAYER - GET_MOVE ---------------------------------

#define DEFPROC_CPU_GET_MOVE(name,max_depth,piececnt,hid) \
static int name##_get_move(board_t *b, int my_color, char *eval_str) \
{ \
    int move, value, depth; \
    depth = (b->black_cnt + b->white_cnt > (piececnt) ? 100 : (max_depth)); \
    maximizing_color = my_color; \
    heuristic = heuristic_##hid; \
    value = alphabeta(b, depth, -INFIN, +INFIN, true, &move); \
    create_eval_str(value, eval_str); \
    return move; \
} \
player_t name = {#name, name##_get_move};

DEFPROC_CPU_GET_MOVE(CPU1,1,55,a)
DEFPROC_CPU_GET_MOVE(CPU2,2,54,a)
DEFPROC_CPU_GET_MOVE(CPU3,3,53,a)
DEFPROC_CPU_GET_MOVE(CPU4,4,52,a)
DEFPROC_CPU_GET_MOVE(CPU5,5,51,a)
DEFPROC_CPU_GET_MOVE(CPU6,6,50,a)
DEFPROC_CPU_GET_MOVE(CPU7,7,49,a)
DEFPROC_CPU_GET_MOVE(CPU8,8,48,a)

// -----------------  CREATE GAME FORECAST EVALUATION STRING  ----------------

static void create_eval_str(int eval_int, char *eval_str)
{
    if (eval_str == NULL) {
        return;
    }

    // eval_str should not exceed 16 char length, 
    // to avoid characters being off the window
    if (eval_int > 1000000) {
        sprintf(eval_str, "CPU TO WIN BY %d", eval_int-1000000);
    } else if (eval_int == 1000000) {
        sprintf(eval_str, "TIE");
    } else if (eval_int < -1000000) {
        sprintf(eval_str, "HUMAN CAN WIN BY %d", -eval_int-1000000);
    } else if (eval_int > 500) {
        sprintf(eval_str, "CPU ADVANTAGE");
    } else if (eval_int < -500) {
        sprintf(eval_str, "HUMAN ADVANTAGE");
    } else {
        eval_str[0] = '\0';
    }
}

// -----------------  CHOOSE BEST MOVE (RECURSIVE ROUTINE)  -----------------

static int alphabeta(board_t *b, int depth, int alpha, int beta, bool maximizing_player, int *move)
{
    #define CHILD(i) \
        ({ b_child = *b; \
           apply_move(&b_child, my_color, pm.move[i], NULL); \
           &b_child; })

    int              my_color    = (maximizing_player ? maximizing_color : OTHER_COLOR(maximizing_color));
    int              other_color = OTHER_COLOR(my_color);
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
        get_possible_moves(b, my_color, &pm);
        if (pm.max == 0) {
            possible_moves_t other_pm;
            get_possible_moves(b, other_color, &other_pm);
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
            if ((v = alphabeta(CHILD(i), depth-1, alpha, beta, false, NULL)) > value) {
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
            if ((v = alphabeta(CHILD(i), depth-1, alpha, beta, true, NULL)) < value) {
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
    int my_color = (maximizing_player ? maximizing_color : OTHER_COLOR(maximizing_color));

    if (game_over) {
        // the game is over, return a large positive or negative value, 
        // incorporating by how many pieces the game has been won or lost
        piece_cnt_diff = (my_color == BLACK ? b->black_cnt - b->white_cnt
                                            : b->white_cnt - b->black_cnt);
        if (piece_cnt_diff >= 0) {
            value = 1000000 + piece_cnt_diff;
        } else {
            value = -1000000 + piece_cnt_diff;
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

        value = (100000 * (corner_cnt_my_color - corner_cnt_other_color)) +
                (-10000 * (gateway_cnt_my_color - gateway_cnt_other_color)) +
                (100    * pm->max);

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
