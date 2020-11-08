static inline int64_t reasonable_moves(board_t *b, possible_moves_t *pm)
{
    int i, cnt = pm->max;
#if 0
    int my_color = b->whose_turn;
    int other_color = OTHER_COLOR(my_color);
#endif

    for (i = 0; i < pm->max; i++) {
        int r,c;
        MOVE_TO_RC(pm->move[i],r,c);
        if ((r == 2 && c == 2 && b->pos[1][1] == NONE) ||
            (r == 2 && c == 7 && b->pos[1][8] == NONE) ||
            (r == 7 && c == 2 && b->pos[8][1] == NONE) ||
            (r == 7 && c == 7 && b->pos[8][8] == NONE))
        {
            cnt--;
            continue;
        }

#if 0
        // testing showed no benefit for this
        if (r == 1 && c == 2 && b->pos[1][1] == NONE && b->pos[1][3] == other_color) {
            cnt--;
            continue;
        }
        if (r == 1 && c == 7 && b->pos[1][8] == NONE && b->pos[1][6] == other_color) {
            cnt--;
            continue;
        }
        if (r == 2 && c == 1 && b->pos[1][1] == NONE && b->pos[3][1] == other_color) {
            cnt--;
            continue;
        }
        if (r == 2 && c == 8 && b->pos[1][8] == NONE && b->pos[3][8] == other_color) {
            cnt--;
            continue;
        }
        if (r == 7 && c == 1 && b->pos[8][1] == NONE && b->pos[6][1] == other_color) {
            cnt--;
            continue;
        }
        if (r == 7 && c == 8 && b->pos[8][8] == NONE && b->pos[6][8] == other_color) {
            cnt--;
            continue;
        }
        if (r == 8 && c == 2 && b->pos[8][1] == NONE && b->pos[8][3] == other_color) {
            cnt--;
            continue;
        }
        if (r == 8 && c == 7 && b->pos[8][8] == NONE && b->pos[8][6] == other_color) {
            cnt--;
            continue;
        }
#endif
    }

    return cnt;
}
