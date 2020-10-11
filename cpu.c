// XXX next static eval
// - 1000000 for winning / /loosing
// -  100000 for corners
// -  -10000 for gateway squares to corners
// -       1 for good possible moves
//

// XXX try making a better static_eval, by excluding gateway to corne from pm.max

#include <common.h>

typedef struct {
    int val;
    int move;
} val_t;

static int max_recursion_depth;
static int (*static_eval)(board_t *b, int my_color, bool game_over, possible_moves_t *pm);

static void create_eval_str(int eval_int, char *eval_str);
static int eval(board_t *b, int my_color, int recursion_depth, int *ret_move);
static int static_eval_a(board_t *b, int my_color, bool game_over, possible_moves_t *pm);

// -----------------  xxx  --------------------------------------------------

// xxx comments

#define DEFPROC_CPU_GET_MOVE(name,mrd,piececnt,seid) \
static int name##_get_move(board_t *b, int my_color, char *eval_str) \
{ \
    int move, eval_int; \
    max_recursion_depth = (b->black_cnt + b->white_cnt > (piececnt) ? 100 : (mrd)); \
    static_eval = static_eval_##seid; \
    eval_int = eval(b, my_color, 0, &move); \
    create_eval_str(eval_int, eval_str); \
    if (move == MOVE_NONE) FATAL("invalid move\n"); \
    return move; \
} \
player_t name = {#name, name##_get_move};

DEFPROC_CPU_GET_MOVE(CPU1,1,55,a)
DEFPROC_CPU_GET_MOVE(CPU2,2,54,a)
DEFPROC_CPU_GET_MOVE(CPU3,3,53,a)
DEFPROC_CPU_GET_MOVE(CPU4,4,52,a)
DEFPROC_CPU_GET_MOVE(CPU5,5,51,a)
DEFPROC_CPU_GET_MOVE(CPU6,6,51,a)

// -----------------  xxx  --------------------------------------------------

static void create_eval_str(int eval_int, char *eval_str)
{
    if (eval_str == NULL) {
        return;
    }

    if (eval_int > 1000000) {
        sprintf(eval_str, "CPU WILL WIN BY %d", eval_int-1000000);
    } else if (eval_int == 1000000) {
        sprintf(eval_str, "CPU WILL TIE OR WIN");
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

// -----------------  xxx  --------------------------------------------------

static int eval(board_t *b, int my_color, int recursion_depth, int *ret_move)
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
        int v = eval(b, other_color, recursion_depth+1, NULL);
        *ret_move = MOVE_PASS;
        return -v;
    }

    // loop over possible moves ...
    min_val = INT_MAX;
    for (i = 0; i < pm.max; i++) {
        // make a copy of the board, and apply the move to it,
        // this is now the board that the oppenent needs to choose a move from
        board_t new_board = *b;
        apply_move(&new_board, my_color, pm.move[i], NULL);

        // obtaine the opponent's board eval metric via recursive call to eval
        val[i].val = eval(&new_board, other_color, recursion_depth+1, NULL);
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

// -----------------  xxx  --------------------------------------------------

// xxx comments
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

// xxx comments
// if corner is occupied || diag gateway not occupied
//     ;
// else if diagnol gateway == my color
//   gateway_cnt_my_color++;
// else 
//   gateway_cnt_other_color++;
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

static int static_eval_a(board_t *b, int my_color, bool game_over, possible_moves_t *pm)
{
    int piece_cnt_diff;
    int corner_cnt_my_color = 0, corner_cnt_other_color = 0;
    int gateway_cnt_my_color = 0, gateway_cnt_other_color = 0;
    
    // 1000000 for winning
    //  100000 for corners
    //  -10000 for gateway squares to corners
    //       1 for  possible moves

    // xxx
    if (game_over) {
        piece_cnt_diff = (my_color == BLACK ? b->black_cnt - b->white_cnt 
                                            : b->white_cnt - b->black_cnt);
        if (piece_cnt_diff >= 0) {
            return 1000000 + piece_cnt_diff;
        } else {
            return -1000000 + piece_cnt_diff;
        }
    }

    // corners
    EVAL_CORNER(1,1);
    EVAL_CORNER(1,8);
    EVAL_CORNER(8,1);
    EVAL_CORNER(8,8);

    // gateway to corner
    EVAL_GATEWAY_TO_CORNER(1,1, 2,2);
    EVAL_GATEWAY_TO_CORNER(1,8, 2,7);
    EVAL_GATEWAY_TO_CORNER(8,1, 7,2);
    EVAL_GATEWAY_TO_CORNER(8,8, 7,7);

    //  xxx
    return 100000 * (corner_cnt_my_color - corner_cnt_other_color) + 
           -10000 * (gateway_cnt_my_color - gateway_cnt_other_color) +
           pm->max;
}
