#include <common.h>

#include <util_misc.h>
#include <util_sdl.h>

//
// defines
//

#define DEFAULT_WIN_WIDTH    1920
#define DEFAULT_WIN_HEIGHT   1002   // xxx 125 * 8  + 2

#define MAX_AVAIL_PLAYERS         (sizeof(avail_players) / sizeof(avail_players[0]))
#define MAX_TOURNAMENT_PLAYERS    (sizeof(tournament_players) / sizeof(tournament_players[0]))

#define GAME_STATE_RESET         0
#define GAME_STATE_START         1
#define GAME_STATE_ACTIVE        2
#define GAME_STATE_COMPLETE      3

#define FONTSZ  70   // xxx use 50 for linux later

// xxx solve name for WHITE and BLACK problem

//
// typedefs
//

typedef struct {
    bool    enabled;
    int     select_idx;
    int     pb_idx;
    int     pw_idx;
    int     games_played[20];   //xxx
    double  games_won[20];
} tournament_t;

//
// variables
// xxx may need to re-init all of these for android
// xxx make some of these const
//

static int                 win_width  = DEFAULT_WIN_WIDTH;
static int                 win_height = DEFAULT_WIN_HEIGHT;

static int                 game_state = GAME_STATE_RESET;

static board_t             board;
static int                 whose_turn;
static possible_moves_t    possible_moves;

static player_t           *player_black;
static player_t           *player_white;
static player_t           *game_mode_player_black;  // xxx try to not have this global
static player_t           *game_mode_player_white;
static player_t           *avail_players[] = { &human, 
                                               &cpu_1, &cpu_2, &cpu_3, &cpu_4, &cpu_5, &cpu_6,
                                               &cpu_random };

static tournament_t        tournament;
static player_t           *tournament_players[] = { &cpu_1, &cpu_2, &cpu_3, &cpu_4, &cpu_5, /*&cpu_6*/ };

static int                 player_black_board_eval = BOARD_EVAL_NONE;  // xxx tbd
static int                 player_white_board_eval = BOARD_EVAL_NONE;  // xxx tbd

//
// prototypes
//

static void initialize(void);

static void *game_thread(void *cx);
static void init_board(void);
static void set_game_state(int new_game_state);
static int get_game_state(void);

static void tournament_get_players(player_t **pb, player_t **pw);
static void tournament_tally_game_result(void);

static int pane_hndlr(pane_cx_t *pane_cx, int request, void * init_params, sdl_event_t * event);
static void REG_EVENT(pane_cx_t *pane_cx, double r, double c, int event, char *fmt, ...)  // xxx name  
                __attribute__ ((format (printf, 5, 6)));

// -----------------  MAIN  -------------------------------------------------------

int main(int argc, char **argv)
{
    int requested_win_width;
    int requested_win_height;

    // initialize
    initialize();

    // init sdl
    // xxx use full screen for android,  use default or -g wxh for linux
    requested_win_width  = win_width;
    requested_win_height = win_height;
    if (sdl_init(&win_width, &win_height, false, false) < 0) {
        FATAL("sdl_init %dx%d failed\n", win_width, win_height);
    }
    INFO("requested win_width=%d win_height=%d\n", requested_win_width, requested_win_height);
    INFO("actual    win_width=%d win_height=%d\n", win_width, win_height);
    // xxx must be requested  - NOT LATER

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

// -----------------  INITIALIZE  -------------------------------------------------

static void initialize(void)
{
    pthread_t tid;

    // XXX config file
    // XXX - setting for projection

    // xxx use config file
    game_mode_player_black = &human;
    game_mode_player_white = &cpu_5;

    // xxx
    player_black = game_mode_player_black;
    player_white = game_mode_player_white;

    // xxx init all vars (maybe needed on android)
    
    // create game_thread
    pthread_create(&tid, NULL, game_thread, NULL);
}

// -----------------  GAME THREAD  ------------------------------------------------

static void *game_thread(void *cx)
{
    int  move;
    bool tournament_game;

restart:
    // init board
    init_board();

    // game state must be RESET or START
    if (get_game_state() != GAME_STATE_RESET && get_game_state() != GAME_STATE_START) {
        FATAL("invalid game_state %d\n", get_game_state());
    }

    // wait for GAME_STATE_START
    while (get_game_state() != GAME_STATE_START) {
        usleep(10*MS);
    }

    // get the 2 players
    tournament_game = tournament.enabled;
    if (tournament_game) {
        tournament_get_players(&player_black, &player_white);
    } else {
        player_black = game_mode_player_black;
        player_white = game_mode_player_white;
    }

    // xxx comment
    whose_turn = 0;  // xxx use define for NO_TURN
    set_game_state(GAME_STATE_ACTIVE);

    // loop until game is finished
    while (true) {
        whose_turn = REVERSI_BLACK;
        get_possible_moves(&board, REVERSI_BLACK, &possible_moves);
        move = player_black->get_move(&board, REVERSI_BLACK, &player_black_board_eval);
        possible_moves.max = -1;
        if (get_game_state() != GAME_STATE_ACTIVE) goto restart;
        if (move == MOVE_GAME_OVER) break;
        apply_move(&board, REVERSI_BLACK, move);

        whose_turn = REVERSI_WHITE;
        get_possible_moves(&board, REVERSI_WHITE, &possible_moves);
        move = player_white->get_move(&board, REVERSI_WHITE, &player_white_board_eval);
        possible_moves.max = -1;
        if (get_game_state() != GAME_STATE_ACTIVE) goto restart;
        if (move == MOVE_GAME_OVER) break;
        apply_move(&board, REVERSI_WHITE, move);
    }

    // game is over
    whose_turn = 0; //xxx  use define for no-turn
    set_game_state(GAME_STATE_COMPLETE);

    // if in tournament mode
    // - tally result
    // - play next game
    // else 
    //   xxx comment
    // endif
    if (tournament_game) {
        tournament_tally_game_result();
        set_game_state(tournament.enabled ? GAME_STATE_START : GAME_STATE_RESET);
        goto restart;
    } else {
        while (get_game_state() == GAME_STATE_COMPLETE) {
            usleep(10*MS);
        }
        goto restart;
    }

    return NULL;
}

static void init_board(void)
{
    memset(board.pos, REVERSI_EMPTY, sizeof(board.pos));
    board.pos[4][4] = REVERSI_WHITE;
    board.pos[4][5] = REVERSI_BLACK;
    board.pos[5][4] = REVERSI_BLACK;
    board.pos[5][5] = REVERSI_WHITE;
    board.black_cnt = 2;
    board.white_cnt = 2;
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

static void tournament_get_players(player_t **pb, player_t **pw)
{
    do {
        tournament.pb_idx = (tournament.select_idx / MAX_TOURNAMENT_PLAYERS) % MAX_TOURNAMENT_PLAYERS;
        tournament.pw_idx = (tournament.select_idx % MAX_TOURNAMENT_PLAYERS);
        tournament.select_idx++;
    } while (tournament.pb_idx == tournament.pw_idx);

    *pb = tournament_players[tournament.pb_idx];
    *pw = tournament_players[tournament.pw_idx];
}

static void tournament_tally_game_result(void)
{
#if 0
    // xxx display game over and final score   xxx and who wins
    // xxx print score
    bool black_should_win = (tournament.pb_idx > tournament.pw_idx);
    bool white_should_win = (tournament.pw_idx > tournament.pb_idx);
    bool wrong_winner = (black_should_win && board.black_cnt <= board.white_cnt) ||
                        (white_should_win && board.white_cnt <= board.black_cnt);
    INFO("GAME COMPLETE - BLACK=%s WHITE=%s - %d,%d - %s   %s\n",
         player_black->name, player_white->name,
         board.black_cnt, board.white_cnt, WINNER_STR(&board),
         wrong_winner ? "WRONG_WINNER" : "");
#endif

    //sleep(1);

    tournament.games_played[tournament.pb_idx]++;
    tournament.games_played[tournament.pw_idx]++;
    if (board.black_cnt == board.white_cnt) {
        tournament.games_won[tournament.pb_idx] += 0.5;
        tournament.games_won[tournament.pw_idx] += 0.5;
    } else if (board.black_cnt > board.white_cnt) {
        tournament.games_won[tournament.pb_idx] += 1;
    } else {
        tournament.games_won[tournament.pw_idx] += 1;
    }

#if 0  // xxx publish result at end of each round
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
#endif
}

// -----------------  GAME UTILS  -------------------------------------------------

static int r_incr_tbl[8] = {0, -1, -1, -1,  0,  1, 1, 1};
static int c_incr_tbl[8] = {1,  1,  0, -1, -1, -1, 0, 1};

void  apply_move(board_t *b, int my_color, int move)
{
    int  r, c, i, j, other_color;
    int *my_color_cnt, *other_color_cnt;
    bool succ;

    if (move == MOVE_PASS) {
        return;
    }

    succ = false;
    other_color = OTHER_COLOR(my_color);
    MOVE_TO_RC(move, r, c);
    if (b->pos[r][c] != REVERSI_EMPTY) {
        FATAL("pos[%d][%d] = %d\n", r, c, b->pos[r][c]);
    }

    if (my_color == REVERSI_BLACK) {
        my_color_cnt    = &b->black_cnt;
        other_color_cnt = &b->white_cnt;
    } else {
        my_color_cnt    = &b->white_cnt;
        other_color_cnt = &b->black_cnt;
    }

    b->pos[r][c] = my_color;
    *my_color_cnt += 1;

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
            for (j = 0; j < cnt; j++) {
                r_next -= r_incr;
                c_next -= c_incr;
                b->pos[r_next][c_next] = my_color;
            }
            *my_color_cnt += cnt;
            *other_color_cnt -= cnt;
        }
    }

    if (!succ) {
        FATAL("invalid call to apply_move\n");
    }
}

void get_possible_moves(board_t *b, int my_color, possible_moves_t *pm)
{
    int r, c, i, other_color, move;

    pm->max = 0;
    pm->color = my_color;
    other_color = OTHER_COLOR(my_color);

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

bool game_cancelled(void)
{
    // xxx use setjmp in cpu.c
    return get_game_state() != GAME_STATE_ACTIVE;
}

// -----------------  DISPLAY  ----------------------------------------------------

// xxx cleanup
#define CTLX  1015
#define CTLY  0

// xxx comments on the modes and sketch the displays
static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    #define RC_TO_LOC(r,c,loc) \
        do { \
            (loc).x = 2 + ((c) - 1) * 125; \
            (loc).y = 2 + ((r) - 1) * 125; \
            (loc).w = 123; \
            (loc).h = 123; \
        } while (0)

    // global events
    #define SDL_EVENT_HELP                    (SDL_EVENT_USER_DEFINED + 0)
    #define SDL_EVENT_QUIT_PGM                (SDL_EVENT_USER_DEFINED + 1)

    // tournament mode events
    #define SDL_EVENT_CHOOSE_GAME_MODE        (SDL_EVENT_USER_DEFINED + 10)

    // game mode events
    #define SDL_EVENT_CHOOSE_TOURNAMENT_MODE  (SDL_EVENT_USER_DEFINED + 20)
    #define SDL_EVENT_GAME_START              (SDL_EVENT_USER_DEFINED + 21)
    #define SDL_EVENT_GAME_RESET              (SDL_EVENT_USER_DEFINED + 22)
    #define SDL_EVENT_HUMAN_MOVE_PASS         (SDL_EVENT_USER_DEFINED + 23)
    #define SDL_EVENT_HUMAN_MOVE_SELECT       (SDL_EVENT_USER_DEFINED + 24)   // length 100

    // game mode, choose player events
    #define SDL_EVENT_CHOOSE_BLACK_PLAYER     (SDL_EVENT_USER_DEFINED + 130)
    #define SDL_EVENT_CHOOSE_WHITE_PLAYER     (SDL_EVENT_USER_DEFINED + 131)
    #define SDL_EVENT_CHOOSE_PLAYER_SELECT    (SDL_EVENT_USER_DEFINED + 132)   // legnth MAX_AVAIL_PLAYERS

    rect_t * pane = &pane_cx->pane;

    static texture_t white_circle;
    static texture_t black_circle;
    static texture_t small_white_circle;
    static texture_t small_black_circle;

    static bool help_mode;   // xxx all need to be initialized, I think - check this
    static int  choose_player_mode;

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

        // draw the 64 green playing squares
        for (r = 1; r <= 8; r++) {
            for (c = 1; c <= 8; c++) {
                RC_TO_LOC(r,c,loc);
                sdl_render_fill_rect(pane, &loc, GREEN);
            }
        }

        // draw the black and white pieces 
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

        // xxx make these routines ...


        // global events
        if (!help_mode) {
            REG_EVENT(pane_cx, 0, -4, SDL_EVENT_HELP, "HELP");
            REG_EVENT(pane_cx, -1, -4, SDL_EVENT_QUIT_PGM, "QUIT");
        }

        if (help_mode) {
            // help mode ...

            // xxx display help screen
            // - mouse wheel scroll
            // - BACK event at lower right

        } else if (tournament.enabled) {
            // tournament mode ..

            // register event to go back to game mode
            REG_EVENT(pane_cx, 0, 0, SDL_EVENT_CHOOSE_GAME_MODE, "TOURNAMENT");

            // display tournament mode status
            char black_player_status_str[100] = "";   // XXX this could be a routine
            char white_player_status_str[100] = "";
            REG_EVENT(pane_cx, 2, 0, 0, "%5s %2d %s",
                      player_black->name, board.black_cnt, black_player_status_str);
            REG_EVENT(pane_cx, 3.5, 0, 0, "%5s %2d %s",
                      player_white->name, board.white_cnt, white_player_status_str);


            // xxx comment
            int i;
            for (i = 0; i < MAX_TOURNAMENT_PLAYERS; i++) {
                // xxx pickup results at end of a serises
                REG_EVENT(pane_cx, 5.5+i, 0, 0, 
                          "%5s %0.0f %d : %2.0f %%",
                          tournament_players[i]->name,
                          tournament.games_won[i],
                          tournament.games_played[i],
                          100. * tournament.games_won[i] / tournament.games_played[i]);
            }


        } else if (choose_player_mode == 0) { //xxx define for 0
            // game mode ...

            // if it is a human player's turn then
            if ((whose_turn == REVERSI_BLACK && player_black == &human) ||
                (whose_turn == REVERSI_WHITE && player_white == &human)) 
            {
                // display the human player's possilbe moves as small circles
                for (i = 0; i < possible_moves.max; i++) {
                    MOVE_TO_RC(possible_moves.move[i], r, c);
                    RC_TO_LOC(r,c,loc);
                    sdl_render_texture(
                        pane, loc.x+52, loc.y+52, 
                        (possible_moves.color == REVERSI_BLACK ? small_black_circle : small_white_circle));
                }

                // register mouse click events for every square
                // xxx maybe these go down below
                for (r = 1; r <= 8; r++) {
                    for (c = 1; c <= 8; c++) {
                        RC_TO_MOVE(r,c,move);
                        RC_TO_LOC(r,c,loc);
                        sdl_register_event(pane, &loc, 
                                           SDL_EVENT_HUMAN_MOVE_SELECT + move,
                                           SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);
                    }
                }

                // if there are no possible moves for this player then
                // register for the HUMAN_MOVE_PASS event
                if (possible_moves.max == 0) {
                    char *str = (possible_moves.color == REVERSI_WHITE
                                ? "WHITE-MUST-PASS" : "BLACK-MUST-PASS");
                    REG_EVENT(pane_cx, 7, 0, SDL_EVENT_HUMAN_MOVE_PASS, str);
                }
            }

            // register game mode events
            REG_EVENT(pane_cx, 0, 0, 
                      (get_game_state() == GAME_STATE_RESET ? SDL_EVENT_CHOOSE_TOURNAMENT_MODE : 0),
                      "GAME");

            if (get_game_state() == GAME_STATE_ACTIVE || get_game_state() == GAME_STATE_COMPLETE) {
                REG_EVENT(pane_cx, 2, 0, SDL_EVENT_GAME_START, "RESTART");
                REG_EVENT(pane_cx, 2, 10, SDL_EVENT_GAME_RESET, "RESET");
            }

            if (get_game_state() == GAME_STATE_RESET) {
                REG_EVENT(pane_cx, 2, 0, SDL_EVENT_GAME_START, "START");
            }

            // display game status
            char black_player_status_str[100] = "";
            char white_player_status_str[100] = "";
            if (get_game_state() == GAME_STATE_RESET) {
                REG_EVENT(pane_cx, 4, 0, SDL_EVENT_CHOOSE_BLACK_PLAYER, "%s", player_black->name);
                REG_EVENT(pane_cx, 5.5, 0, SDL_EVENT_CHOOSE_WHITE_PLAYER, "%s", player_white->name);
            } else {
                REG_EVENT(pane_cx, 4, 0, 0, "%5s %2d %s",
                          player_black->name, board.black_cnt, black_player_status_str);
                REG_EVENT(pane_cx, 5.5, 0, 0, "%5s %2d %s",
                          player_white->name, board.white_cnt, white_player_status_str);
            }

        } else {
            // game mode / choose player ...

            // register player selection events
            // xxx later SDL_EVENT_CHOOSE_PLAYER_SELECT   legnth MAX_AVAIL_PLAYERS
        }
            
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        int rc = PANE_HANDLER_RET_NO_ACTION;

        switch (event->event_id) {
        // global events
        case SDL_EVENT_HELP:
            help_mode = true;
            break;
        case SDL_EVENT_QUIT_PGM:
            rc = PANE_HANDLER_RET_PANE_TERMINATE;
            break;

        // tournament mode events
        case SDL_EVENT_CHOOSE_GAME_MODE:
            tournament.enabled = false;
            set_game_state(GAME_STATE_RESET);
            break;

        // game mode events
        case SDL_EVENT_CHOOSE_TOURNAMENT_MODE:
            memset(&tournament, 0, sizeof(tournament));
            tournament.enabled = true;
            set_game_state(GAME_STATE_START);
            break;
        case SDL_EVENT_GAME_START:
            set_game_state(GAME_STATE_START);
            break;
        case SDL_EVENT_GAME_RESET:
            set_game_state(GAME_STATE_RESET);
            break;
        case SDL_EVENT_HUMAN_MOVE_PASS:
            human_move_select = MOVE_PASS;
            break;
        case SDL_EVENT_HUMAN_MOVE_SELECT ... SDL_EVENT_HUMAN_MOVE_SELECT+99:
            human_move_select = event->event_id - SDL_EVENT_HUMAN_MOVE_SELECT;
            break;

        // game mode, choose player events
        case SDL_EVENT_CHOOSE_BLACK_PLAYER:
            // xxx later
            break;
        case SDL_EVENT_CHOOSE_WHITE_PLAYER:
            // xxx later
            break;
        case SDL_EVENT_CHOOSE_PLAYER_SELECT ... SDL_EVENT_CHOOSE_PLAYER_SELECT+MAX_AVAIL_PLAYERS-1:
            // xxx later
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

// xxx name
static void REG_EVENT(pane_cx_t *pane_cx, double r, double c, int event, char *fmt, ...)
{
    int x, y;
    char str[100];
    va_list ap;
    rect_t * pane = &pane_cx->pane;

    // get x, y
    x = (c >= 0 ? CTLX + COL2X(c,FONTSZ) : win_width  + COL2X(c,FONTSZ));
    y = (r >= 0 ? CTLY + ROW2Y(r,FONTSZ) : win_height + ROW2Y(r,FONTSZ));

    // make str
    va_start(ap,fmt);
    vsprintf(str, fmt, ap);
    va_end(ap);

    // if event is 0 then print else register event
    if (event == 0) {
        sdl_render_text(
            pane, x, y, FONTSZ, str, WHITE, BLACK);
    } else {
        sdl_render_text_and_register_event(
            pane, x, y, FONTSZ, str, LIGHT_BLUE, BLACK, 
            event, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);
    }
}


#if 0
        case SDL_EVENT_GAME_RESTART:
            set_game_state(GAME_STATE_RESET or START events needed
            break;
        case SDL_EVENT_SELECT_MOVE ... SDL_EVENT_SELECT_MOVE+100: {
            move_select = event->event_id - SDL_EVENT_SELECT_MOVE;
            break; }
        case SDL_EVENT_SELECT_MOVE_PASS:
            move_select = MOVE_PASS;
            break;



        sdl_render_text_and_register_event(
            pane, CTLX, ROW2Y(0,FONTSZ), FONTSZ, "RESTART", LIGHT_BLUE, BLACK, 
            SDL_EVENT_GAME_RESTART, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);

        sdl_render_printf(
            pane, CTLX, ROW2Y(2,FONTSZ), FONTSZ, WHITE, BLACK, 
            "%s %s %2d %s",
            "BLACK", player_black->name, board.black_cnt, (whose_turn == REVERSI_BLACK ? "<=" : ""));
        sdl_render_printf(
            pane, CTLX, ROW2Y(3,FONTSZ), FONTSZ, WHITE, BLACK, 
            "%s %s %2d %s",
            "WHITE", player_white->name, board.white_cnt, (whose_turn == REVERSI_WHITE ? "<=" : ""));

        if (player_black_board_eval != BOARD_EVAL_NONE) {
            sdl_render_printf(
                pane, CTLX, ROW2Y(6,FONTSZ), FONTSZ, WHITE, BLACK, 
                "BLACK: % d", player_black_board_eval);
        }
        if (player_white_board_eval != BOARD_EVAL_NONE) {
            sdl_render_printf(
                pane, CTLX, ROW2Y(7,FONTSZ), FONTSZ, WHITE, BLACK, 
                "WHITE: % d", player_white_board_eval);
        }

        if (get_game_state() == GAME_STATE_COMPLETE) {
            sdl_render_printf(
                pane, CTLX, ROW2Y(9,FONTSZ), FONTSZ, WHITE, BLACK, 
                "GAME OVER");
            if (board.black_cnt == board.white_cnt) {
                sdl_render_printf(
                    pane, CTLX, ROW2Y(10,FONTSZ), FONTSZ, WHITE, BLACK, 
                    "TIE");
            } else if (board.black_cnt > board.white_cnt) {
                sdl_render_printf(
                    pane, CTLX, ROW2Y(10,FONTSZ), FONTSZ, WHITE, BLACK, 
                    "BLACK WINS BY %d", board.black_cnt - board.white_cnt);
            } else {
                sdl_render_printf(
                    pane, CTLX, ROW2Y(10,FONTSZ), FONTSZ, WHITE, BLACK, 
                    "WHITE WINS BY %d", board.white_cnt - board.black_cnt);
            }
        }
#endif
