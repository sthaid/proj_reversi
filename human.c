#include <common.h>

static int human_get_move(board_t *b, int color);

player_t human = {"human", human_get_move};

// --------------------------------------------------------------------------

// xxx make const b
static int human_get_move(board_t *b, int my_color)
{
    bool             valid_move;
    possible_moves_t pm;
    int              move;

    INFO("called for %s\n", REVERSI_COLOR_STR(my_color));

    // get possible moves
    get_possible_moves(b, my_color, &pm);

    // if no possible moves and other color has no moves then return GAME_OVER
    if (pm.max == 0) {
        possible_moves_t other_pm;
        int other_color = (my_color == REVERSI_WHITE ? REVERSI_BLACK : REVERSI_WHITE);

        get_possible_moves(b, other_color, &other_pm);
        if (other_pm.max == 0) {
            return MOVE_GAME_OVER;
        }
    }

    // loop until a valid move is chosen
    do {
        // wait for either a move_select available or game_restart_requested
        move_select = MOVE_NONE;
        while (true) {
            if (game_restart_requested()) {
                return MOVE_GAME_OVER;
            }
            if ((move = move_select) != MOVE_NONE) {
                break;
            }
            usleep(10*MS);
        }

        INFO("GOT MOVE %d\n", move);

        // determine if move is valid
        if (pm.max == 0) {
            valid_move = (move == MOVE_PASS);
        } else  {
            board_t tmp_b = *b;
            valid_move = apply_move(&tmp_b, my_color, move);
        }
    } while (valid_move == false);

    // return the move
    return move;
}
