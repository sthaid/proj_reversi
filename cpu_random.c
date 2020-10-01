#include <common.h>

static int cpu_random_get_move(board_t *b, int color);

player_t cpu_random = {"cpu_random", cpu_random_get_move};

// -----------------  XXX  --------------------------------------------------

// xxx make const b
static int cpu_random_get_move(board_t *b, int my_color)
{
    possible_moves_t pm, other_pm;

    INFO("called for %s\n", REVERSI_COLOR_STR(my_color));

    // get possible moves
    get_possible_moves(b, my_color, &pm);

    // if no possible moves then
    //   determine if other_color has possible moves
    //   return either PASS or GAME_OVER
    // else if one possilbe move then
    //   return that move
    // endif
    if (pm.max == 0) {
        get_possible_moves(b, OTHER_COLOR(my_color), &other_pm);
        return other_pm.max > 0 ? MOVE_PASS : MOVE_GAME_OVER;
    } else if (pm.max == 1) {
        return pm.move[0];  // XXX why here?
    }

    // there are 2 or more possible moves ...

    // choose a possible move at random and return it
    return pm.move[random() % pm.max];
}
