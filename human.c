#include <common.h>

static int human_get_move(board_t *b, int color);

player_t human = {"human", human_get_move};

// --------------------------------------------------------------------------

static int human_get_move(board_t *b, int color)
{
    INFO("called for %s\n", REVERSI_COLOR_STR(color));

    move_select = -1;
    while (move_select == -1 && game_restart_requested() == false) {
        usleep(10*MS);
    }

    if (game_restart_requested()) {
        INFO("game_restart\n");
        return MOVE_GAME_OVER;
    }

    INFO("got move %d\n", move_select);

    return move_select;
}

