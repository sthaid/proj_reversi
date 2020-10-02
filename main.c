#include <common.h>

#include <util_misc.h>
#include <util_sdl.h>

//
// defines
//

#define DEFAULT_WIN_WIDTH    1500
#define DEFAULT_WIN_HEIGHT   1002   // xxx 125 * 8  + 2

#define MAX_AVAIL_PLAYERS         (sizeof(avail_players) / sizeof(avail_players[0]))
#define MAX_TOURNAMENT_PLAYERS    (sizeof(tournament_players) / sizeof(tournament_players[0]))

#define GAME_STATE_NOT_STARTED   0
#define GAME_STATE_ACTIVE        1
#define GAME_STATE_COMPLETE      2
#define GAME_STATE_RESTART       3

//
// typedefs
//

typedef struct {
    bool enabled;
    int  select_idx;
    int  pb_idx;
    int  pw_idx;
} tournament_t;

typedef struct {
    player_t *player;
    int       games_played;
    double    games_won;
} tournament_player_t;

//
// variables
//

static int       win_width  = DEFAULT_WIN_WIDTH;
static int       win_height = DEFAULT_WIN_HEIGHT;

static int       game_state = GAME_STATE_NOT_STARTED;

static board_t   board;
static int       whose_turn;
static possible_moves_t 
                 possible_moves;  // xxx indent

static player_t *player_black;
static player_t *player_white;

static int       player_black_board_eval = BOARD_EVAL_NONE;
static int       player_white_board_eval = BOARD_EVAL_NONE;

static player_t *avail_players[] = { &human, 
                                     &cpu_1, &cpu_2, &cpu_3, &cpu_4, &cpu_5, &cpu_6,
                                     &cpu_random };

static tournament_t        tournament;
static tournament_player_t tournament_players[] = { {&cpu_1}, {&cpu_2}, {&cpu_3}, 
                                                    {&cpu_4}, {&cpu_5}, {&cpu_6}  };

//
// prototypes
//

static void game_init(char *player_black_name, char *player_white_name);
static void *game_thread(void *cx);
static void set_game_state(int new_game_state);
static int get_game_state(void);

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);

// -----------------  MAIN  -------------------------------------------------------

int main(int argc, char **argv)
{
    int requested_win_width;
    int requested_win_height;

    // verfify 2 player names are supplied
    if (argc != 3) {
        FATAL("argc=%d\n", argc);
    }

    // init game control software
    game_init(argv[1], argv[2]);

    // init sdl
    requested_win_width  = win_width;
    requested_win_height = win_height;
    if (sdl_init(&win_width, &win_height, false, false) < 0) {
        FATAL("sdl_init %dx%d failed\n", win_width, win_height);
    }
    INFO("requested win_width=%d win_height=%d\n", requested_win_width, requested_win_height);
    INFO("actual    win_width=%d win_height=%d\n", win_width, win_height);
    // XXX must be requested

    // run the pane manger, this is the runtime loop
    sdl_pane_manager(
        NULL,           // context
        NULL,           // called prior to pane handlers
        NULL,           // called after pane handlers
        10000,          // 0=continuous, -1=never, else us
        1,              // number of pane handler varargs that follow
        pane_hndlr, NULL, 0, 0, win_width, win_height, PANE_BORDER_STYLE_NONE);

    // done
    return 0;
}

// -----------------  GAME CONTROL  -----------------------------------------------

static void game_init(char *player_black_name, char *player_white_name) 
{
    pthread_t tid;
    int i;

    // search for players matching the supplied names
    for (i = 0; i < MAX_AVAIL_PLAYERS; i++) {
        if (strcasecmp(player_black_name, avail_players[i]->name) == 0) {
            player_black = avail_players[i];
        }
        if (strcasecmp(player_white_name, avail_players[i]->name) == 0) {
            player_white = avail_players[i];
        }
    }

    // if player_black or player_white has not been set then fatal error
    if (player_black == NULL) {
        FATAL("XXX\n");
    }
    if (player_white == NULL) {
        FATAL("XXX\n");
    }
    INFO("BLACK = %s   WHITE = %s\n", player_black->name, player_white->name);

    // create game_thread
    pthread_create(&tid, NULL, game_thread, NULL);
}

static void *game_thread(void *cx)
{
    int  move;

    // XXX temp
    tournament.enabled = true;

restart:
    // if in tournament mode then pick 2 players
    if (tournament.enabled) {
        do {
            tournament.pb_idx = (tournament.select_idx / MAX_TOURNAMENT_PLAYERS) % MAX_TOURNAMENT_PLAYERS;
            tournament.pw_idx = (tournament.select_idx % MAX_TOURNAMENT_PLAYERS);
            tournament.select_idx++;
        } while (tournament.pb_idx == tournament.pw_idx);
        player_black = tournament_players[tournament.pb_idx].player;
        player_white = tournament_players[tournament.pw_idx].player;
    }

    // init board
    memset(board.pos, REVERSI_EMPTY, sizeof(board.pos));
    board.pos[4][4] = REVERSI_WHITE;
    board.pos[4][5] = REVERSI_BLACK;
    board.pos[5][4] = REVERSI_BLACK;
    board.pos[5][5] = REVERSI_WHITE;
    board.black_cnt = 2;
    board.white_cnt = 2;

    // xxx
    whose_turn = 0;  // XXX  XXX maybe can delete this
    set_game_state(GAME_STATE_ACTIVE);

    //INFO("GAME STARTING - BLACK=%s WHITE=%s\n",
    //     player_black->name, player_white->name);

    // loop until game is finished
    // XXX may need a mutex around display update
    // XXX may want to slow down processing when human not invovled
    while (true) {
        // XXX make this a loop
        whose_turn = REVERSI_BLACK;
        get_possible_moves(&board, REVERSI_BLACK, &possible_moves);
        move = player_black->get_move(&board, REVERSI_BLACK, &player_black_board_eval);
        possible_moves.max = -1;
        if (game_restart_requested()) goto restart;
        if (move == MOVE_GAME_OVER) break;
        apply_move(&board, REVERSI_BLACK, move);  // XXX check to ensure it succeeded

        whose_turn = REVERSI_WHITE;
        get_possible_moves(&board, REVERSI_WHITE, &possible_moves);
        move = player_white->get_move(&board, REVERSI_WHITE, &player_white_board_eval);
        possible_moves.max = -1;
        if (game_restart_requested()) goto restart;
        if (move == MOVE_GAME_OVER) break;
        apply_move(&board, REVERSI_WHITE, move);
    }

    // game is over
    // XXX display game over and final score   XXX and who wins
    // XXX AAA print score
    bool black_should_win = (tournament.pb_idx > tournament.pw_idx);
    bool white_should_win = (tournament.pw_idx > tournament.pb_idx);
    bool wrong_winner = (black_should_win && board.black_cnt <= board.white_cnt) ||
                        (white_should_win && board.white_cnt <= board.black_cnt);

    INFO("GAME COMPLETE - BLACK=%s WHITE=%s - %d,%d - %s   %s\n",
         player_black->name, player_white->name,
         board.black_cnt, board.white_cnt, WINNER_STR(&board),
         wrong_winner ? "WRONG_WINNER" : "");

    whose_turn = 0; //XXX
    set_game_state(GAME_STATE_COMPLETE);

    // if in tournament mode
    // - tally result
    // - play next game
    if (tournament.enabled) {
        sleep(2);

        tournament_players[tournament.pb_idx].games_played++;
        tournament_players[tournament.pw_idx].games_played++;
        if (board.black_cnt == board.white_cnt) {
            tournament_players[tournament.pb_idx].games_won += 0.5;
            tournament_players[tournament.pw_idx].games_won += 0.5;
        } else if (board.black_cnt > board.white_cnt) {
            tournament_players[tournament.pb_idx].games_won += 1;
        } else {
            tournament_players[tournament.pw_idx].games_won += 1;
        }

        //INFO("tsi %d\n", tournament.select_idx);
        if ((tournament.select_idx % (MAX_TOURNAMENT_PLAYERS*MAX_TOURNAMENT_PLAYERS)) == 
             (MAX_TOURNAMENT_PLAYERS*MAX_TOURNAMENT_PLAYERS-1))
        {
            int i;
            char s[200], *p=s;
            //char *p = s;
            for (i = 0; i < MAX_TOURNAMENT_PLAYERS; i++) {
                p += sprintf(p, "%0.1f/%d  ", 
                    tournament_players[i].games_won, tournament_players[i].games_played);
            }
            p += sprintf(p, " - ");
            for (i = 0; i < MAX_TOURNAMENT_PLAYERS; i++) {
                p += sprintf(p, "%2.0f ",
                        100. * tournament_players[i].games_won / tournament_players[i].games_played);
            }
            INFO("%s\n", s);
        }

        goto restart;
    }

    // wait for request to restart
    while (game_restart_requested() == false) {
        usleep(10*MS);
    }
    goto restart;

    return NULL;
}

static void set_game_state(int new_game_state)
{
    game_state = new_game_state;
    __sync_synchronize();
}

static int get_game_state(void)
{
    __sync_synchronize();
    return game_state;
}

bool game_restart_requested(void)
{
    return get_game_state() == GAME_STATE_RESTART;
}

// -----------------  GAME UTILS  -------------------------------------------------

static int r_incr_tbl[8] = {0, -1, -1, -1,  0,  1, 1, 1};
static int c_incr_tbl[8] = {1,  1,  0, -1, -1, -1, 0, 1};

bool apply_move(board_t *b, int my_color, int move)
{
    int  r, c, i, other_color;
    int *my_color_cnt, *other_color_cnt;
    bool succ;

    succ = false;
    other_color = (my_color == REVERSI_WHITE ? REVERSI_BLACK : REVERSI_WHITE);  // XXX use macro
    MOVE_TO_RC(move, r, c);
    if (b->pos[r][c] != REVERSI_EMPTY) {
        return false;
    }

    if (my_color == REVERSI_BLACK) {
        my_color_cnt    = &b->black_cnt;
        other_color_cnt = &b->white_cnt;
    } else {
        my_color_cnt    = &b->white_cnt;
        other_color_cnt = &b->black_cnt;
    }

    for (i = 0; i < 8; i++) {
        int r_incr = r_incr_tbl[i];
        int c_incr = c_incr_tbl[i];
        int r_next = r + r_incr;
        int c_next = c + c_incr;
        int cnt    = 0;

        while (b->pos[r_next][c_next] == other_color) {
            r_next += r_incr;
            c_next += c_incr;
            cnt++;
        }

        if (cnt > 0 && b->pos[r_next][c_next] == my_color) {
            succ = true;
            for (i = 0; i < cnt; i++) {
                r_next -= r_incr;
                c_next -= c_incr;
                b->pos[r_next][c_next] = my_color;
            }
            *my_color_cnt += cnt;
            *other_color_cnt -= cnt;
        }
    }

    if (succ) {
        b->pos[r][c] = my_color;
        *my_color_cnt += 1;
    }

    return succ;
}

void get_possible_moves(board_t *b, int my_color, possible_moves_t *pm)
{
    int r, c, i, other_color, move;

    pm->max = 0;
    pm->color = my_color;

    other_color = (my_color == REVERSI_WHITE ? REVERSI_BLACK : REVERSI_WHITE);

    for (r = 1; r <= 8; r++) {
        for (c = 1; c <= 8; c++) {
            if (b->pos[r][c] != REVERSI_EMPTY) {
                continue;
            }

            for (i = 0; i < 8; i++) {
                int r_incr = r_incr_tbl[i];
                int c_incr = c_incr_tbl[i];
                int r_next = r + r_incr;
                int c_next = c + c_incr;
                int cnt    = 0;

                while (b->pos[r_next][c_next] == other_color) {
                    r_next += r_incr;
                    c_next += c_incr;
                    cnt++;
                }

                if (cnt > 0 && b->pos[r_next][c_next] == my_color) {
                    RC_TO_MOVE(r, c, move);
                    pm->move[pm->max++] = move;
                    break;
                }
            }
        }
    }
}

// -----------------  DISPLAY  ----------------------------------------------------

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    #define RC_TO_LOC(r,c,loc) \
        do { \
            (loc).x = 2 + ((c) - 1) * 125; \
            (loc).y = 2 + ((r) - 1) * 125; \
            (loc).w = 123; \
            (loc).h = 123; \
        } while (0)

    #define SDL_EVENT_GAME_RESTART      (SDL_EVENT_USER_DEFINED + 0)
    #define SDL_EVENT_SELECT_MOVE_PASS  (SDL_EVENT_USER_DEFINED + 1)
    #define SDL_EVENT_SELECT_MOVE       (SDL_EVENT_USER_DEFINED + 10)   // length 100

    rect_t * pane = &pane_cx->pane;

    static texture_t white_circle;
    static texture_t black_circle;
    static texture_t small_white_circle;
    static texture_t small_black_circle;

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        INFO("PANE x,y,w,h  %d %d %d %d\n", pane->x, pane->y, pane->w, pane->h);

        white_circle = sdl_create_filled_circle_texture(50, WHITE);
        black_circle = sdl_create_filled_circle_texture(50, BLACK);

        small_white_circle = sdl_create_filled_circle_texture(10, WHITE);
        small_black_circle = sdl_create_filled_circle_texture(10, BLACK);

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        int r, c, i, move;
        rect_t loc;

        // XXX comments
        for (r = 1; r <= 8; r++) {
            for (c = 1; c <= 8; c++) {
                RC_TO_LOC(r,c,loc);
                sdl_render_fill_rect(pane, &loc, GREEN);
            }
        }

        for (r = 1; r <= 8; r++) {
            for (c = 1; c <= 8; c++) {
                RC_TO_LOC(r,c,loc);
                // xxx single line
                if (board.pos[r][c] == REVERSI_BLACK) {
                    sdl_render_texture(pane, loc.x+12, loc.y+12, black_circle);
                } else if (board.pos[r][c] == REVERSI_WHITE) {
                    sdl_render_texture(pane, loc.x+12, loc.y+12, white_circle);
                } 
            }
        }

        if ((whose_turn == REVERSI_BLACK && player_black == &human) ||
            (whose_turn == REVERSI_WHITE && player_white == &human)) 
        {
            for (i = 0; i < possible_moves.max; i++) {
                MOVE_TO_RC(possible_moves.move[i], r, c);
                RC_TO_LOC(r,c,loc);
                sdl_render_texture(
                    pane, loc.x+52, loc.y+52, 
                    (possible_moves.color == REVERSI_BLACK ? small_black_circle : small_white_circle));
            }
        }

        for (r = 1; r <= 8; r++) {
            for (c = 1; c <= 8; c++) {
                RC_TO_MOVE(r,c,move);
                RC_TO_LOC(r,c,loc);
                sdl_register_event(pane, &loc, 
                                   SDL_EVENT_SELECT_MOVE + move,
                                   SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);
            }
        }

        sdl_render_text_and_register_event(
            pane, 1030, ROW2Y(0,40), 40, "RESTART", LIGHT_BLUE, BLACK, 
            SDL_EVENT_GAME_RESTART, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);

        sdl_render_printf(
            pane, 1030, ROW2Y(2,40), 40, WHITE, BLACK, 
            "%s %s %2d %s",
            "BLACK", player_black->name, board.black_cnt, (whose_turn == REVERSI_BLACK ? "<=" : ""));
        sdl_render_printf(
            pane, 1030, ROW2Y(3,40), 40, WHITE, BLACK, 
            "%s %s %2d %s",
            "WHITE", player_white->name, board.white_cnt, (whose_turn == REVERSI_WHITE ? "<=" : ""));

        if (player_black_board_eval != BOARD_EVAL_NONE) {
            sdl_render_printf(
                pane, 1030, ROW2Y(6,40), 40, WHITE, BLACK, 
                "BLACK: % d", player_black_board_eval);
        }
        if (player_white_board_eval != BOARD_EVAL_NONE) {
            sdl_render_printf(
                pane, 1030, ROW2Y(7,40), 40, WHITE, BLACK, 
                "WHITE: % d", player_white_board_eval);
        }

        if (get_game_state() == GAME_STATE_COMPLETE) {
            sdl_render_printf(
                pane, 1030, ROW2Y(9,40), 40, WHITE, BLACK, 
                "GAME OVER");
            if (board.black_cnt == board.white_cnt) {
                sdl_render_printf(
                    pane, 1030, ROW2Y(10,40), 40, WHITE, BLACK, 
                    "TIE");
            } else if (board.black_cnt > board.white_cnt) {
                sdl_render_printf(
                    pane, 1030, ROW2Y(10,40), 40, WHITE, BLACK, 
                    "BLACK WINS BY %d", board.black_cnt - board.white_cnt);
            } else {
                sdl_render_printf(
                    pane, 1030, ROW2Y(10,40), 40, WHITE, BLACK, 
                    "WHITE WINS BY %d", board.white_cnt - board.black_cnt);
            }
        }

        if (possible_moves.max == 0) {
            char *str = (possible_moves.color == REVERSI_WHITE
                         ? "WHITE-MUST-PASS" : "BLACK-MUST-PASS");
            sdl_render_text_and_register_event(
                pane, 1030, win_height-ROW2Y(2,40), 40, str, LIGHT_BLUE, BLACK, 
                SDL_EVENT_SELECT_MOVE_PASS, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);
        }
            
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        int rc = PANE_HANDLER_RET_NO_ACTION;

        switch (event->event_id) {
        case SDL_EVENT_GAME_RESTART:
            set_game_state(GAME_STATE_RESTART);
            break;
        case SDL_EVENT_SELECT_MOVE ... SDL_EVENT_SELECT_MOVE+100: {
            move_select = event->event_id - SDL_EVENT_SELECT_MOVE;
            break; }
        case SDL_EVENT_SELECT_MOVE_PASS:
            move_select = MOVE_PASS;
            break;
        case 'q':  // quit
            rc = PANE_HANDLER_RET_PANE_TERMINATE;
            break;
        default:
            break;
        }

        return rc;
    }

    // ---------------------------
    // -------- TERMINATE --------
    // ---------------------------

    if (request == PANE_HANDLER_REQ_TERMINATE) {
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // not reached
    assert(0);
    return PANE_HANDLER_RET_NO_ACTION;
}



#if 0
- get possible moves is public again
  - needs a count of possilbe moves too, and maybe the list
- display select PASS available if no possible moves
- human calls get_possible moves to deterine if PASS is an allowed return 
- code in human will automatically determine game over
- redesign display, to also display game_state and color icons

// XXX may want a mutex unless we can atomically flip the board and new_board
pane_hndlr
- button to start the game
#endif
