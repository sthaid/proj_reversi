#include <common.h>

static int human_get_move(board_t *b, int color, int *b_eval);

player_t human = {"HUMAN", human_get_move};

// --------------------------------------------------------------------------

// xxx make const b
static int human_get_move(board_t *b, int my_color, int *b_eval)
{
    bool             valid_move;
    possible_moves_t pm;
    int              move;
    int              i;

    // there is no board evaluation performed
    *b_eval = BOARD_EVAL_NONE;

    // get possible moves
    get_possible_moves(b, my_color, &pm);

    // if no possible moves and other color has no moves then return GAME_OVER
    if (pm.max == 0) {
        possible_moves_t other_pm;
        get_possible_moves(b, OTHER_COLOR(my_color), &other_pm);
        if (other_pm.max == 0) {
            return MOVE_GAME_OVER;
        }
    }

    // loop until a valid move is chosen
    do {
        // wait for either an available selected move or game cancelled
        human_move_select = MOVE_NONE;
        while (true) {
            if (move_cancelled()) {
                return MOVE_NONE;
            }
            if ((move = human_move_select) != MOVE_NONE) {
                break;
            }
            usleep(10*MS);
        }

        // determine if move is valid
        valid_move = false;
        if (pm.max == 0) {
            valid_move = (move == MOVE_PASS);
        } else  {
            for (i = 0; i < pm.max; i++) {
                if (move == pm.move[i]) {
                    valid_move = true;
                    break;
                }
            }
        }
    } while (valid_move == false);

    // return the move
    return move;
}
