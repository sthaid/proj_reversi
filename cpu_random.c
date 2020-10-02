#include <common.h>

static int cpu_random_get_move(board_t *b, int color, int *b_eval);

player_t cpu_random = {"CPU-RANDOM", cpu_random_get_move};

// -----------------  XXX  --------------------------------------------------

// xxx make const b
static int cpu_random_get_move(board_t *b, int my_color, int *b_eval)
{
    possible_moves_t pm, other_pm;

    // there is no board evaluation performed
    *b_eval = BOARD_EVAL_NONE;

    // get possible moves
    get_possible_moves(b, my_color, &pm);

    // if no possible moves then
    //   determine if other_color has possible moves
    //   return either PASS or GAME_OVER
    // endif
    if (pm.max == 0) {
        get_possible_moves(b, OTHER_COLOR(my_color), &other_pm);
        return other_pm.max > 0 ? MOVE_PASS : MOVE_GAME_OVER;
    }

    // choose a possible move at random and return it
    return pm.move[random() % pm.max];
}
