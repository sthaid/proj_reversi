#include <common.h>

#define OLD_C

#if !defined(CPU_C) && !defined(OLD_C)
#error "must define CPU_C or OLD_C"
#endif

//
// defines
//

#define INFIN INT64_MAX
#define ONE64 ((int64_t)1)

//
// variables
//

//
// prototypes
//

static int64_t alphabeta(const board_t *b, int depth, int64_t alpha, int64_t beta, bool maximizing_player, int *move);
static void init_edge_gateway_to_corner(void);
static int64_t heuristic(const board_t *b, bool maximizing_player, bool game_over, possible_moves_t *pm);

// -----------------  CPU PLAYER - GET_MOVE ---------------------------------

static int  MIN_DEPTH[9]              = {0,  1,  2,  3,  4,  5,  6,  7,  8 };
static int  PIECECNT_FOR_EOG_DEPTH[9] = {0, 56, 55, 54, 53, 52, 51, 50, 49 };
static bool initialized               = false;

static inline int get_depth(int level, int piececnt)
{
    double M, B;
    int depth;

    M = 1.0;
    B = (64 - PIECECNT_FOR_EOG_DEPTH[level]) - M * PIECECNT_FOR_EOG_DEPTH[level];
    depth = rint(M * piececnt + B);
    if (depth < MIN_DEPTH[level]) depth = MIN_DEPTH[level];
    return depth;
}

#ifdef CPU_C
static void create_eval_str(int64_t value, char *eval_str);

int cpu_get_move(int level, const board_t *b, char *eval_str, bool *is_book_move)
{
    int64_t value;
    int     move, depth, piececnt;
    bool    book_move_being_generated = false;

    // sanity check level arg
    if (level < 1 || level > 8) {
        FATAL("invlaid level %d\n", level);
    }

    // initialization
    if (initialized == false) {
        init_edge_gateway_to_corner();
        initialized = true;
    }
    piececnt = b->black_cnt + b->white_cnt;
    *is_book_move = false;

    // book move lookup
    if (book_move_enabled() || opt_book_move_gen_mode) {
        move = bm_get_move(b);
        if (move != MOVE_NONE) {
            if (eval_str) eval_str[0] = '\0';
            *is_book_move = true;
            return move;
        }
    }

    // get lookahead depth
    depth = get_depth(level, piececnt);

    // if opt_book_move_gen_mode is enabled and there are < 20 pieces on the board then
    //   set a high lookahead depth of 10
    // endif
    if (opt_book_move_gen_mode && piececnt < 20) {
        book_move_being_generated = true;
        depth = 10;
        INFO("bmgen - generating book move using depth %d\n", depth);
    }

    // call alphabeta to get the best move, and associated heuristic value
    value = alphabeta(b, depth, -INFIN, +INFIN, true, &move);
    if (move_cancelled()) {
        INFO("move_cancelled, returning MOVE_NONE\n");
        if (eval_str) eval_str[0] = '\0';
        return MOVE_NONE;
    }

    // if a book move was generated then 
    // call bm_add_move to add the move to reversi.book file
    if (book_move_being_generated) {
        bm_add_move(b, move);
    }

    // create evaluation string, based on value returned from alphabeta;
    // this string can optionally be displayed by caller
    create_eval_str(value, eval_str);

    // return the move, that was obtained by call to alphabeta
    return move;
}

static void create_eval_str(int64_t value, char *eval_str)
{
    // eval_str should not exceed 16 char length, 
    // to avoid characters being off the window

    if (eval_str == NULL) {
        return;
    }

    if (value == (ONE64 << 56)) {
        sprintf(eval_str, "TIE");
    } else if (value > (ONE64 << 56)) {
        sprintf(eval_str, "CPU TO WIN BY %d", (int)((value >> 56) - 1));
    } else if (value < -(ONE64 << 56)) {
        sprintf(eval_str, "HUMAN CAN WIN BY %d", (int)(-(value >> 56) - 1));
    } else {
        eval_str[0] = '\0';
    }
}
#endif

// -----------------  OLD CPU PLAYER - GET_MOVE -----------------------------

// This is used:
// - in tournament mode to compare the ability of the new cpu code against the old.
// - in tournament/book_move_gen_mode: to introduce occasional random moves;
//   this increases the number of book moves that get saved in reversi.book
//
// Note:
// - book move lookup is not performed by this routine; this allows tournament
//   mode to evaluate the benefit of the book move lookup that is performed in cpu_get_move
// - old_get_move does not updae eval_str

#ifdef OLD_C
int old_get_move(int level, const board_t *b, char *eval_str, bool *is_book_move)
{
    int move, depth, piececnt;

    // sanity check level arg
    if (level < 1 || level > 8) {
        FATAL("invlaid level %d\n", level);
    }

    // initialization
    if (initialized == false) {
        init_edge_gateway_to_corner();
        initialized = true;
    }
    piececnt = b->black_cnt + b->white_cnt;
    *is_book_move = false;

    // When generating book moves, cpu.c is generating the book moves, 
    //  in response to the moves which are determined by old.c.
    // If old.c were to always to pick what it considers the best move then
    //  there is little game variety and not many book moves are generated.
    // The following code causes old.c to choose random moves in every other
    //  game, and with a probabilty of 0.10. 
    // Random number generator is reseeded every 100 games, this also
    //  adds to game variety.
    if (opt_book_move_gen_mode) {
        static unsigned int game_count;
        if (piececnt <= 5) {
            INFO("game_count=%d\n", game_count);
            game_count++;
            if ((game_count % 100) == 0) {
                INFO("reseeding random number generator, game_count=%d\n", game_count);
                srandom(microsec_timer());
            }
        }
        if (game_count & 1) {
            if (piececnt < 20 && (random() % 10) == 0) {
                possible_moves_t pm;
                get_possible_moves(b, &pm);
                if (pm.max == 0) return MOVE_PASS;
                move = pm.move[ random()%pm.max ];
                INFO("bmgen - random move %d color=%d\n", move, b->whose_turn);
                return move;
            }
        }
    }

    // get lookahead depth
    depth = get_depth(level, piececnt);

    // call alphabeta to get the best move
    alphabeta(b, depth, -INFIN, +INFIN, true, &move);
    if (move_cancelled()) {
        INFO("move_cancelled, returning MOVE_NONE\n");
        return MOVE_NONE;
    }

    // return the move, that was obtained by call to alphabeta
    return move;
}
#endif

// -----------------  CHOOSE BEST MOVE (RECURSIVE ROUTINE)  -----------------

static int64_t alphabeta(const board_t *b, int depth, int64_t alpha, int64_t beta, bool maximizing_player, int *move)
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

static inline int64_t corner_count(const board_t *b)
{
    int cnt = 0;
    int my_color = b->whose_turn;
    int other_color = OTHER_COLOR(my_color);

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

static inline int64_t corner_moves(const board_t *b)
{
    int cnt = 0;
    int my_color = b->whose_turn;
    int other_color = OTHER_COLOR(my_color);
    int which_corner;

    for (which_corner = 0; which_corner < 4; which_corner++) {
        if (is_corner_move_possible(b, which_corner)) cnt++;
    }

    ((board_t*)b)->whose_turn = other_color;
    for (which_corner = 0; which_corner < 4; which_corner++) {
        if (is_corner_move_possible(b, which_corner)) cnt--;
    }
    ((board_t*)b)->whose_turn = my_color;

    return cnt;
}

static inline int64_t diagonal_gateways_to_corner(const board_t *b)
{
    int cnt = 0;
    int my_color = b->whose_turn;
    int other_color = OTHER_COLOR(my_color);

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

static uint8_t black_gateway_to_corner_bitmap[8192];
static uint8_t white_gateway_to_corner_bitmap[8192];

static inline int64_t edge_gateway_to_corner(const board_t *b)
{
    #define HORIZONTAL_EDGE(b, r) \
        ((b->pos[r][1] << 14) | \
         (b->pos[r][2] << 12) | \
         (b->pos[r][3] << 10) | \
         (b->pos[r][4] <<  8) | \
         (b->pos[r][5] <<  6) | \
         (b->pos[r][6] <<  4) | \
         (b->pos[r][7] <<  2) | \
         (b->pos[r][8] <<  0))
    #define VERTICAL_EDGE(b, c) \
        ((b->pos[1][c] << 14) | \
         (b->pos[2][c] << 12) | \
         (b->pos[3][c] << 10) | \
         (b->pos[4][c] <<  8) | \
         (b->pos[5][c] <<  6) | \
         (b->pos[6][c] <<  4) | \
         (b->pos[7][c] <<  2) | \
         (b->pos[8][c] <<  0))

    int       my_color    = b->whose_turn;
    int       other_color = OTHER_COLOR(my_color);
    int       cnt         = 0;
    uint16_t  edge;
    uint8_t  *my_color_gateway_to_corner_bitmap;
    uint8_t  *other_color_gateway_to_corner_bitmap;

    my_color_gateway_to_corner_bitmap = (my_color == BLACK 
        ? black_gateway_to_corner_bitmap : white_gateway_to_corner_bitmap);
    other_color_gateway_to_corner_bitmap = (other_color == BLACK 
        ? black_gateway_to_corner_bitmap : white_gateway_to_corner_bitmap);

    edge = HORIZONTAL_EDGE(b,1);
    if (getbit(my_color_gateway_to_corner_bitmap,edge)) cnt++;
    if (getbit(other_color_gateway_to_corner_bitmap,edge)) cnt--;

    edge = HORIZONTAL_EDGE(b,8);
    if (getbit(my_color_gateway_to_corner_bitmap,edge)) cnt++;
    if (getbit(other_color_gateway_to_corner_bitmap,edge)) cnt--;

    edge = VERTICAL_EDGE(b,1);
    if (getbit(my_color_gateway_to_corner_bitmap,edge)) cnt++;
    if (getbit(other_color_gateway_to_corner_bitmap,edge)) cnt--;

    edge = VERTICAL_EDGE(b,8);
    if (getbit(my_color_gateway_to_corner_bitmap,edge)) cnt++;
    if (getbit(other_color_gateway_to_corner_bitmap,edge)) cnt--;

    return cnt;
}

static void init_edge_gateway_to_corner(void)
{
    #define REVERSE(x) \
        ((((x) & 0x0003) << 14) | \
         (((x) & 0x000c) << 10) | \
         (((x) & 0x0030) <<  6) | \
         (((x) & 0x00c0) <<  2) | \
         (((x) & 0x0300) >>  2) | \
         (((x) & 0x0c00) >>  6) | \
         (((x) & 0x3000) >> 10) | \
         (((x) & 0xc000) >> 14))


    #define MAX_BLACK_GATEWAY_TO_CORNER_PATTERNS (sizeof(black_gateway_to_corner_patterns)/sizeof(char*))

    static char *black_gateway_to_corner_patterns[] = {
                ".W.W....",
                ".W.WW...",
                ".W.WWW..",
                ".W.WWWW.",
                ".WW.W...",
                ".WW.WW..",
                ".WW.WWW.",
                ".WWW.W..",
                ".WWW.WW.",
                ".W...W..",
                ".W...WW.",
                ".W..B...",
                ".WW..B..",
                ".W.B.B..",
                            };

    int i,j;
    uint16_t edge, edge_reversed;

    for (i = 0; i < MAX_BLACK_GATEWAY_TO_CORNER_PATTERNS; i++) {
        char *s = black_gateway_to_corner_patterns[i];
        edge = 0;
        for (j = 0; j < 8; j++) {
            if (s[j] == 'W') {
                edge |= (WHITE << (14-2*j));
            } else if (s[j] == 'B') {
                edge |= (BLACK << (14-2*j));
            }
        }
        edge_reversed = REVERSE(edge);
        //INFO("BLACK PATTERN  %04x  %04x\n", edge, edge_reversed);
        setbit(black_gateway_to_corner_bitmap, edge);
        setbit(black_gateway_to_corner_bitmap, edge_reversed);
    }

    for (i = 0; i < MAX_BLACK_GATEWAY_TO_CORNER_PATTERNS; i++) {
        char *s = black_gateway_to_corner_patterns[i];
        edge = 0;
        for (j = 0; j < 8; j++) {
            if (s[j] == 'W') {
                edge |= (BLACK << (14-2*j));
            } else if (s[j] == 'B') {
                edge |= (WHITE << (14-2*j));
            }
        }
        edge_reversed = REVERSE(edge);
        //INFO("WHITE PATTERN  %04x  %04x\n", edge, edge_reversed);
        setbit(white_gateway_to_corner_bitmap, edge);
        setbit(white_gateway_to_corner_bitmap, edge_reversed);
    }
}

static inline int64_t reasonable_moves(const board_t *b, possible_moves_t *pm)
{
    int i, cnt = pm->max;

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

static int64_t heuristic(const board_t *b, bool maximizing_player, bool game_over, possible_moves_t *pm)
{
    int64_t value;

    // handle game over case
    if (game_over) {
        int64_t piece_cnt_diff;

        // the game is over, return a large positive or negative value, 
        // incorporating by how many pieces the game has been won or lost
        piece_cnt_diff = (b->whose_turn == BLACK ? b->black_cnt - b->white_cnt
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

    // The following board characteristics are utilized to generate the heuristic value.
    //
    // These are listed in order of importance / numeric weight.
    //
    // Except as noted, each characteristic is evaluated as a poistive for the color 
    // whose turn it is, and a negative for the other color. For example, if it is
    // black's turn and black has pieces in 2 corners and white has pieces in 1 corner
    // then the corner_count would be 1.
    //
    // - corner_count: count of occupied corners
    //
    // - corner_moves: number of corners that can be captured
    //
    // - diagonal_gateways_to_corner: number of pieces that are diagonally inside
    //   an unoccupied corner; this count is negative for my_color and positive for other_color
    //
    // - edge_gateways_to_corner: certain edge patterns can lead to future corner 
    //   capture; this counts the number of occurances of an edge pattern that can
    //   lead to future corner capture
    //
    // - reasonable_moves: this characteristic is evaluated only for the color whose turn it
    //   is; this is the count of possible moves minus possible moves that are not
    //   reasonable; the moves that are not considered reasonable are moves that provide
    //   a diagonal gateway to a corner
    //
    // - random value so that the same move is not always chosen; this covers the
    //   case where heuristic value calculated from the above characteristics are the
    //   same for differnent board inputs

    value = 0;
    value += (corner_count(b) << 48);
    value += (corner_moves(b) << 40);
    value += (diagonal_gateways_to_corner(b) << 32);
    value += (edge_gateway_to_corner(b) << 24);
    value += (reasonable_moves(b, pm) << 16);
    value += ((random() & 127) - 64);

    // the returned heuristic value measures the favorability 
    // for the maximizing player
    return (maximizing_player ? value : -value);
}
