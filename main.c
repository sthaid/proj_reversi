// XXX comments and total review
// XXX android version
//     - init code
// XXX help mode

#include <common.h>

#include <util_misc.h>
#include <util_sdl.h>

//
// defines
//

#ifdef ANDROID
#define main SDL_main
#endif

#define DEFAULT_LINUX_WIN_WIDTH    1500
#define DEFAULT_LINUX_WIN_HEIGHT    800

#define MAX_AVAIL_PLAYERS         (sizeof(avail_players) / sizeof(avail_players[0]))
#define MAX_TOURNAMENT_PLAYERS    (sizeof(tournament_players) / sizeof(tournament_players[0]))

#define GAME_STATE_RESET         0
#define GAME_STATE_ACTIVE        1
#define GAME_STATE_COMPLETE      2

#define GAME_REQUEST_NONE        0
#define GAME_REQUEST_RESET       1
#define GAME_REQUEST_START       2
#define GAME_REQUEST_UNDO        3

#define FONTSZ  70

#define CONFIG_FILENAME ".reversi_config"
#define CONFIG_VERSION  1
#define CONFIG_PLAYER_BLACK_IDX_STR  (config[0].value)
#define CONFIG_PLAYER_WHITE_IDX_STR  (config[1].value)
#define CONFIG_SHOW_MOVE_YN          (config[2].value[0])
#define CONFIG_SHOW_EVAL_YN          (config[3].value[0])

//
// typedefs
//

typedef struct {
    bool    enabled;
    int     select_idx;
    int     pb_idx;
    int     pw_idx;
    int     total_games_played;
    int     games_played[100];
    double  games_won[100];
} tournament_t;

typedef struct {
    board_t          board;
    possible_moves_t possible_moves;
    bool             player_is_human;
    char             eval_str[100];
    unsigned char    highlight[10][10];
} game_moves_t;

//
// variables
// xxx make some of these const
//

static int                 win_width  = DEFAULT_LINUX_WIN_WIDTH;
static int                 win_height = DEFAULT_LINUX_WIN_HEIGHT;

static int                 game_state   = GAME_STATE_RESET;
static int                 game_request = GAME_REQUEST_NONE;

static game_moves_t        game_moves[150];
static int                 max_game_moves;

static player_t           *player_black;
static player_t           *player_white;
static player_t           *avail_players[] = 
                            { &human, &CPU1, &CPU2, &CPU3, &CPU4, &CPU5, &CPU6, };

static tournament_t        tournament;
#ifndef ANDROID
static player_t           *tournament_players[] = 
                            { &CPU1, &CPU2, &CPU3, &CPU4, &CPU5, &CPU6, };
#else
static player_t           *tournament_players[] = 
                            { &CPU1, &CPU2, &CPU3, &CPU4, &CPU5, };
#endif

config_t                   config[] = { { "player_black_idx",   "0" },
                                        { "player_white_idx",   "5" },
                                        { "show_move",          "N" },
                                        { "show_eval",          "N" },
                                        { "",                   ""  } };

//
// prototypes
//

static void *game_thread(void *cx);

static void game_mode_get_players(player_t **pb, player_t **pw);
static void tournament_get_players(player_t **pb, player_t **pw);
static void tournament_tally_game_result(void);

static int pane_hndlr(pane_cx_t *pane_cx, int request, void * init_params, sdl_event_t * event);

// -----------------  MAIN  -------------------------------------------------------

int main(int argc, char **argv)
{
    bool fullscreen = false;
    unsigned char opt_char;
    pthread_t tid;

    INFO("STARTING\n");

    // get options
    // -f         - fullscreen
    // -g wwwxhhh - xxx maybe later
    while (true) {
        opt_char = getopt(argc, argv, "f");
        if (opt_char == 0xff) {
            break;
        }
        switch (opt_char) {
        case 'f':
            fullscreen = true;
            break;
        default:
            FATAL("invalid opt_char '%c'\n", opt_char);
            return 1;
        }
    }

    // read configuration file, and print values
    if (config_read(CONFIG_FILENAME, config, CONFIG_VERSION) < 0) {
        FATAL("config_read failed\n");
    }
    INFO("CONFIG_PLAYER_BLACK_IDX_STR = %s\n", CONFIG_PLAYER_BLACK_IDX_STR);
    INFO("CONFIG_PLAYER_WHITE_IDX_STR = %s\n", CONFIG_PLAYER_WHITE_IDX_STR);
    INFO("CONFIG_SHOW_MOVE_YN         = %c\n", CONFIG_SHOW_MOVE_YN);
    INFO("CONFIG_SHOW_EVAL_YN         = %c\n", CONFIG_SHOW_EVAL_YN);

    // create game_thread, and
    // wait for player_black and player_white to be initialized by the game_thread
    pthread_create(&tid, NULL, game_thread, NULL);
    while (player_black == NULL || player_white == NULL) {
        usleep(10*MS);
    }

    // init sdl
    if (sdl_init(&win_width, &win_height, fullscreen, false, false) < 0) {
        FATAL("sdl_init %dx%d failed\n", win_width, win_height);
    }

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

// -----------------  GAME THREAD  ------------------------------------------------

static void *game_thread(void *cx)
{
    int  move, whose_turn;
    bool tournament_game;
    player_t *player;

restart:
    // init game  xxx or just inline may be clearer
    // xxx review and cleanup

    // xxx
    memset(game_moves, 0, sizeof(game_moves));
    game_moves[0].board.pos[4][4] = WHITE;
    game_moves[0].board.pos[4][5] = BLACK;
    game_moves[0].board.pos[5][4] = BLACK;
    game_moves[0].board.pos[5][5] = WHITE;
    game_moves[0].board.black_cnt = 2;
    game_moves[0].board.white_cnt = 2;
    max_game_moves = 1;

    // xxx
    game_state = GAME_STATE_RESET;
    __sync_synchronize();

    // xxx
    if (game_request == GAME_REQUEST_RESET) {
        game_request = GAME_REQUEST_NONE;
    }

    // xxx comment 
    while (true) {
        if (tournament.enabled) {
            tournament_get_players(&player_black, &player_white);
            tournament_game = true;
            break;
        } else {
            game_mode_get_players(&player_black, &player_white);
            if (game_request == GAME_REQUEST_START) {
                tournament_game = false;
                break;
            }
        }
        usleep(10*MS);
    }
    game_request = GAME_REQUEST_NONE;

    // set game_state to active
    game_state = GAME_STATE_ACTIVE;
    __sync_synchronize();

    // loop until game is finished
    // xxx comments and cleanup
    while (true) {
again:
        // xxx
        whose_turn = (max_game_moves & 1) ? BLACK : WHITE;
        player = (whose_turn == BLACK ? player_black : player_white);

        // set game_moves fields:
        // - player_is_human  xxx describe how these are all used
        // - possible_moves  xxx 
        game_moves[max_game_moves-1].player_is_human = (strcasecmp(player->name, "human") == 0);
        get_possible_moves(&game_moves[max_game_moves-1].board, whose_turn, 
                           &game_moves[max_game_moves-1].possible_moves);  

        // get move by calling player->get_move; 
        // this may also set game_moves field:
        // - eval_str - xxx when is this set, and how used
        move = player->get_move(&game_moves[max_game_moves-1].board, 
                                whose_turn, 
                                game_moves[max_game_moves-1].eval_str);

        // xxx
        if (move == MOVE_GAME_OVER) {
            break;
        }

        // xxx
        if (game_request != GAME_REQUEST_NONE) {
            if (game_request == GAME_REQUEST_UNDO) {
                if (max_game_moves > 2) {
                    max_game_moves -= 2;
                }
                INFO("GOT UNDO, max_game_moves now %d\n", max_game_moves);
                game_request = GAME_REQUEST_NONE;
                goto again;
            } else if (game_request == GAME_REQUEST_RESET || game_request == GAME_REQUEST_START) {
                // the game_request is handled at restart
                goto restart;
            } else {
                FATAL("invalid game_request %d\n", game_request);
            }
        }

        // xxx
        if (move == MOVE_NONE) {
            FATAL("invalid move %d\n", move);
        }

        // add new entry to game_moves, for the next player's move, and set fields
        // - board - is first set to a copy of the current board, and then 
        //   apply_move is called to update the board based on the move determined above0
        // - eval_str - is copied from the current eval_str
        memset(&game_moves[max_game_moves], 0, sizeof(game_moves_t));
        game_moves[max_game_moves].board = game_moves[max_game_moves-1].board;
        strcpy(game_moves[max_game_moves].eval_str, game_moves[max_game_moves-1].eval_str);
        max_game_moves++;

        apply_move(&game_moves[max_game_moves-1].board, whose_turn, move, 
                   (CONFIG_SHOW_MOVE_YN == 'Y' && !tournament_game 
                    ? game_moves[max_game_moves-1].highlight : NULL));
    }

    // game is over
    game_state = GAME_STATE_COMPLETE;
    __sync_synchronize();

    // if in tournament mode then tally the game result
    if (tournament_game) {
        tournament_tally_game_result();
    }

    // wait for request to reset or start
    if (!tournament_game) {
        while (game_request != GAME_REQUEST_RESET && game_request != GAME_REQUEST_START) {
            usleep(10*MS);
        }
    }
    goto restart;

    return NULL;
}

static void game_mode_get_players(player_t **pb, player_t **pw)
{
    int pbidx, pwidx;

    if (sscanf(CONFIG_PLAYER_BLACK_IDX_STR, "%d", &pbidx) != 1 ||
        sscanf(CONFIG_PLAYER_WHITE_IDX_STR, "%d", &pwidx) != 1)
    {
        FATAL("config idx str invalid\n");
    }

    // xxx check range too

    *pb = avail_players[pbidx];
    *pw = avail_players[pwidx];
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
    board_t *b = &game_moves[max_game_moves-1].board;

    tournament.total_games_played++;
    tournament.games_played[tournament.pb_idx]++;
    tournament.games_played[tournament.pw_idx]++;
    if (b->black_cnt == b->white_cnt) {
        tournament.games_won[tournament.pb_idx] += 0.5;
        tournament.games_won[tournament.pw_idx] += 0.5;
    } else if (b->black_cnt > b->white_cnt) {
        tournament.games_won[tournament.pb_idx] += 1;
    } else {
        tournament.games_won[tournament.pw_idx] += 1;
    }
}

// -----------------  GAME UTILS  -------------------------------------------------

static int r_incr_tbl[8] = {0, -1, -1, -1,  0,  1, 1, 1};
static int c_incr_tbl[8] = {1,  1,  0, -1, -1, -1, 0, 1};

void  apply_move(board_t *b, int my_color, int move, unsigned char highlight[][10])
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
    if (b->pos[r][c] != NONE) {
        FATAL("pos[%d][%d] = %d\n", r, c, b->pos[r][c]);
    }

    if (my_color == BLACK) {
        my_color_cnt    = &b->black_cnt;
        other_color_cnt = &b->white_cnt;
    } else {
        my_color_cnt    = &b->white_cnt;
        other_color_cnt = &b->black_cnt;
    }

    b->pos[r][c] = my_color;
    *my_color_cnt += 1;

    if (highlight) {
        highlight[r][c] = 2;
        usleep(200000);
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
            r_next = r;
            c_next = c;
            for (j = 0; j < cnt; j++) {
                r_next += r_incr;
                c_next += c_incr;
                b->pos[r_next][c_next] = my_color;
                if (highlight) {
                    highlight[r_next][c_next] = 1;
                    usleep(200000);
                }
            }
            *my_color_cnt += cnt;
            *other_color_cnt -= cnt;
        }
    }

    if (highlight) {
        sleep(1);
        memset(highlight, 0, 100);  // xxx 100?  use sizeof
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
            if (b->pos[r][c] != NONE) {
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

bool move_cancelled(void)
{
    return game_request != GAME_REQUEST_NONE;
}

// -----------------  DISPLAY  ----------------------------------------------------

// xxx comments on the modes and sketch the displays

//
// defines
//

#ifdef ANDROID
#define CTL_X    0
#define CTL_Y    (win_width + 20)
#define CTL_COLS (win_width / sdl_font_char_width(FONTSZ))
#else
#define CTL_X    (win_height + 10)
#define CTL_Y    0
#define CTL_COLS ((win_width - CTL_X) / sdl_font_char_width(FONTSZ))
#endif

//
// define events
//

// events common to multiple modes
#define SDL_EVENT_HELP                    (SDL_EVENT_USER_DEFINED + 0)

// help mode events
#define SDL_EVENT_EXIT_HELP               (SDL_EVENT_USER_DEFINED + 5)

// tournament mode events
#define SDL_EVENT_SET_GAME_MODE           (SDL_EVENT_USER_DEFINED + 10)

// game mode events
#define SDL_EVENT_SET_TOURNAMENT_MODE     (SDL_EVENT_USER_DEFINED + 20)
#define SDL_EVENT_START_GAME              (SDL_EVENT_USER_DEFINED + 21)
#define SDL_EVENT_RESET_GAME              (SDL_EVENT_USER_DEFINED + 22)
#define SDL_EVENT_CHOOSE_BLACK_PLAYER     (SDL_EVENT_USER_DEFINED + 23)
#define SDL_EVENT_CHOOSE_WHITE_PLAYER     (SDL_EVENT_USER_DEFINED + 24)
#define SDL_EVENT_SHOW_EVAL               (SDL_EVENT_USER_DEFINED + 25)
#define SDL_EVENT_SHOW_MOVE               (SDL_EVENT_USER_DEFINED + 26)
#define SDL_EVENT_HUMAN_MOVE_PASS         (SDL_EVENT_USER_DEFINED + 27)
#define SDL_EVENT_HUMAN_UNDO              (SDL_EVENT_USER_DEFINED + 28)
#define SDL_EVENT_HUMAN_MOVE_SELECT       (SDL_EVENT_USER_DEFINED + 29)   // length 100

// game mode, choose player events
#define SDL_EVENT_CHOOSE_PLAYER_SELECT    (SDL_EVENT_USER_DEFINED + 140)   // legnth MAX_AVAIL_PLAYERS

//
// variables
//

static int       piece_circle_radius;
static texture_t piece_black_circle;
static texture_t piece_white_circle;

static int       prompt_circle_radius;
static texture_t prompt_black_circle;
static texture_t prompt_white_circle;

static int       status_circle_radius;
static texture_t status_black_circle;
static texture_t status_white_circle;

static bool      help_mode;
static int       choose_player_mode;

//
// prototypes
//

static void render_help_mode(pane_cx_t *pane_cx);
static int event_help_mode(pane_cx_t *pane_cx, sdl_event_t *event);
static void render_tournament_mode(pane_cx_t *pane_cx);
static int event_tournament_mode(pane_cx_t *pane_cx, sdl_event_t *event);
static void render_game_choose_player_mode(pane_cx_t *pane_cx);
static int event_game_choose_player_mode(pane_cx_t *pane_cx, sdl_event_t *event);
static void render_game_mode(pane_cx_t *pane_cx);
static int event_game_mode(pane_cx_t *pane_cx, sdl_event_t *event);

static void render_board(pane_cx_t *pane_cx);
static void register_event(pane_cx_t *pane_cx, double r, double c, int event, char *fmt, ...)
                __attribute__ ((format (printf, 5, 6)));
static void print(pane_cx_t *pane_cx, double r, double c, char *fmt, ...)
                __attribute__ ((format (printf, 4, 5)));
static rect_t *rc_to_loc(int r_arg, int c_arg);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t * pane = &pane_cx->pane;

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
#ifdef ANDROID
        double sq_wh = (win_width - 2) / 8. - 2;
#else
        double sq_wh = (win_height - 2) / 8. - 2;
#endif

        INFO("PANE x,y,w,h  %d %d %d %d\n", pane->x, pane->y, pane->w, pane->h);

        piece_circle_radius  = rint(.4*sq_wh);
        piece_black_circle   = sdl_create_filled_circle_texture(piece_circle_radius, BLACK);
        piece_white_circle   = sdl_create_filled_circle_texture(piece_circle_radius, WHITE);

        prompt_circle_radius = rint(.08*sq_wh);
        prompt_black_circle  = sdl_create_filled_circle_texture(prompt_circle_radius, BLACK);
        prompt_white_circle  = sdl_create_filled_circle_texture(prompt_circle_radius, WHITE);

        status_circle_radius = rint(.36*FONTSZ);
        status_black_circle  = sdl_create_filled_circle_texture(status_circle_radius, BLACK);
        status_white_circle  = sdl_create_filled_circle_texture(status_circle_radius, WHITE);

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        if (help_mode) {
            render_help_mode(pane_cx);
        } else if (tournament.enabled) {
            render_tournament_mode(pane_cx);
        } else if (choose_player_mode != NONE) {
            render_game_choose_player_mode(pane_cx);
        } else {
            render_game_mode(pane_cx);
        }
            
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        int rc;

        if (help_mode) {
            rc = event_help_mode(pane_cx, event);
        } else if (tournament.enabled) {
            rc = event_tournament_mode(pane_cx, event);
        } else if (choose_player_mode != NONE) {
            rc = event_game_choose_player_mode(pane_cx, event);
        } else {
            rc = event_game_mode(pane_cx, event);
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
    FATAL("not reached\n");
    return PANE_HANDLER_RET_NO_ACTION;
}

// - - - - - - - - -  GAME MODE  - - - - - - - - - - - - - - - - - - 

static void render_game_mode(pane_cx_t *pane_cx)
{
    rect_t *pane = &pane_cx->pane;

    int r, c, i, move;
    game_moves_t *gm = &game_moves[max_game_moves-1];

    // game mode ...

    // render the board and pieces
    render_board(pane_cx);

    // register common events
    register_event(pane_cx, 0, -4, SDL_EVENT_HELP, "HELP");

    // if it is a human player's turn then
    if (game_state == GAME_STATE_ACTIVE && gm->player_is_human) {
        possible_moves_t *pm = &gm->possible_moves;

        // display the human player's possilbe moves as small circles
        for (i = 0; i < pm->max; i++) {
            rect_t loc;
            int offset;

            MOVE_TO_RC(pm->move[i], r, c);
            loc = *rc_to_loc(r,c);
            offset = loc.w/2 - prompt_circle_radius;
            sdl_render_texture(
                pane, loc.x+offset, loc.y+offset, 
                (pm->color == BLACK ? prompt_black_circle : prompt_white_circle));
        }

        // register mouse click events for every square
        for (r = 1; r <= 8; r++) {
            for (c = 1; c <= 8; c++) {
                RC_TO_MOVE(r,c,move);
                sdl_register_event(pane, rc_to_loc(r,c),
                                   SDL_EVENT_HUMAN_MOVE_SELECT + move,
                                   SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);
            }
        }

        // if there are no possible moves for this player then
        // register for the HUMAN_MOVE_PASS event
        if (pm->max == 0) {
            register_event(pane_cx, 9, 10, SDL_EVENT_HUMAN_MOVE_PASS, "PASS");
        }

        // register for the HUMAN_UNDO event
        if (max_game_moves > 2) {
            register_event(pane_cx, 9, 0, SDL_EVENT_HUMAN_UNDO, "UNDO");
        }
    }

    // register game mode events
    if (game_state == GAME_STATE_RESET) {
        register_event(pane_cx, 0, 0, SDL_EVENT_SET_TOURNAMENT_MODE, "GAME");
    } else {
        print(pane_cx, 0, 0, "GAME");
    }

    if (game_state == GAME_STATE_ACTIVE || game_state == GAME_STATE_COMPLETE) {
        register_event(pane_cx, 2, 0, SDL_EVENT_START_GAME, "RESTART");
        register_event(pane_cx, 2, 10, SDL_EVENT_RESET_GAME, "RESET");
    }

    if (game_state == GAME_STATE_RESET) {
        register_event(pane_cx, 2, 0, SDL_EVENT_START_GAME, "START");
    }

    register_event(pane_cx, -1, 0, SDL_EVENT_SHOW_EVAL, "EVAL=%c", CONFIG_SHOW_EVAL_YN);
    register_event(pane_cx, -1, 10, SDL_EVENT_SHOW_MOVE, "MOVE=%c", CONFIG_SHOW_MOVE_YN);

    // display game status
    int offset = FONTSZ/2 - status_circle_radius;
    rect_t loc_black = { CTL_X, CTL_Y + ROW2Y(4,FONTSZ), FONTSZ, FONTSZ };
    sdl_render_fill_rect(pane, &loc_black, GREEN);
    sdl_render_texture(pane, loc_black.x+offset, loc_black.y+offset, status_black_circle);

    rect_t loc_white = { CTL_X, CTL_Y + ROW2Y(5.5,FONTSZ), FONTSZ, FONTSZ };
    sdl_render_fill_rect(pane, &loc_white, GREEN);
    sdl_render_texture(pane, loc_white.x+offset, loc_white.y+offset, status_white_circle);

    if (game_state == GAME_STATE_RESET) {
        register_event(pane_cx, 4, 2, SDL_EVENT_CHOOSE_BLACK_PLAYER, " %s", player_black->name);
        register_event(pane_cx, 5.5, 2, SDL_EVENT_CHOOSE_WHITE_PLAYER, " %s", player_white->name);
    } else {
        board_t *b = &game_moves[max_game_moves-1].board;
        int whose_turn = ((game_state != GAME_STATE_ACTIVE) ? NONE  :
                          (max_game_moves & 1)              ? BLACK : 
                                                              WHITE);
        print(pane_cx, 4, 2, "%c%-5s %2d", 
              whose_turn == BLACK ? '*' : ' ',
              player_black->name,
              b->black_cnt);
        print(pane_cx, 5.5, 2, "%c%-5s %2d", 
              whose_turn == WHITE ? '*' : ' ',
              player_white->name,
              b->white_cnt);
    }

    // display game completion or cpu evaluation status
    if (game_state == GAME_STATE_COMPLETE) {
        board_t *b = &gm->board;
        print(pane_cx, 7, 0, "** GAME OVER **");
        if (b->black_cnt == b->white_cnt) {
            print(pane_cx, 8.5, 0, "TIE");
        } else if (b->black_cnt > b->white_cnt) {
            char *name = (strcmp(player_black->name, player_white->name) != 0
                          ? player_black->name : "BLACK");
            print(pane_cx, 8.5, 0, "%s WINS BY %d", name, b->black_cnt - b->white_cnt);
        } else {
            char *name = (strcmp(player_black->name, player_white->name) != 0
                          ? player_white->name : "WHITE");
            print(pane_cx, 8.5, 0, "%s WINS BY %d", name, b->white_cnt - b->black_cnt);
        }
    } else if (CONFIG_SHOW_EVAL_YN == 'Y' && gm->eval_str[0] != '\0') {
        int human_players_cnt = 0;
        if (strcasecmp(player_black->name, "human") == 0) human_players_cnt++;
        if (strcasecmp(player_white->name, "human") == 0) human_players_cnt++;
        if (human_players_cnt == 1) {
            print(pane_cx, 7, 0, "%s", gm->eval_str);  // xxx or fix sdl to just return
        }
    }
}

static int event_game_mode(pane_cx_t *pane_cx, sdl_event_t *event)
{
    int rc = PANE_HANDLER_RET_DISPLAY_REDRAW;

    switch (event->event_id) {

    // game mode events
    case SDL_EVENT_SET_TOURNAMENT_MODE:
        memset(&tournament, 0, sizeof(tournament));
        tournament.enabled = true;
        game_request = GAME_REQUEST_START;
        break;
    case SDL_EVENT_START_GAME:
        game_request = GAME_REQUEST_START;
        break;
    case SDL_EVENT_RESET_GAME:
        game_request = GAME_REQUEST_RESET;
        break;
    case SDL_EVENT_CHOOSE_BLACK_PLAYER:
        choose_player_mode = BLACK;
        break;
    case SDL_EVENT_CHOOSE_WHITE_PLAYER:
        choose_player_mode = WHITE;
        break;
    case SDL_EVENT_SHOW_EVAL:
        CONFIG_SHOW_EVAL_YN = (CONFIG_SHOW_EVAL_YN == 'N' ? 'Y' : 'N');
        config_write();
        break;
    case SDL_EVENT_SHOW_MOVE:
        CONFIG_SHOW_MOVE_YN = (CONFIG_SHOW_MOVE_YN == 'N' ? 'Y' : 'N');
        config_write();
        break;
    case SDL_EVENT_HUMAN_MOVE_PASS:
        human_move_select = MOVE_PASS;
        break;
    case SDL_EVENT_HUMAN_UNDO:
        INFO("setting request UNDO\n");
        game_request = GAME_REQUEST_UNDO;
        break;
    case SDL_EVENT_HUMAN_MOVE_SELECT ... SDL_EVENT_HUMAN_MOVE_SELECT+99:
        human_move_select = event->event_id - SDL_EVENT_HUMAN_MOVE_SELECT;
        break;

    // common events
    case SDL_EVENT_HELP:
        help_mode = true;
        break;
    }

    return rc;
}

// - - - - - - - - -  GAME CHOOSE PLAYER MODE  - - - - - - - - - - - 

static void render_game_choose_player_mode(pane_cx_t *pane_cx)
{
    int i;

    render_board(pane_cx);

    for (i = 0; i < MAX_AVAIL_PLAYERS; i++) {
        register_event(pane_cx, 
                       (i/2) * 2,            // row
                       (i%2) * (CTL_COLS/2), // col
                       SDL_EVENT_CHOOSE_PLAYER_SELECT + i,
                       "%s", avail_players[i]->name);
    }
}

static int event_game_choose_player_mode(pane_cx_t *pane_cx, sdl_event_t *event)
{
    int rc = PANE_HANDLER_RET_DISPLAY_REDRAW;

    switch (event->event_id) {

    // game mode, choose player events
    case SDL_EVENT_CHOOSE_PLAYER_SELECT ... SDL_EVENT_CHOOSE_PLAYER_SELECT+MAX_AVAIL_PLAYERS-1: {
        int idx = event->event_id - SDL_EVENT_CHOOSE_PLAYER_SELECT;
        if (choose_player_mode == BLACK) {
            sprintf(CONFIG_PLAYER_BLACK_IDX_STR, "%d", idx);
            config_write();
        } else if (choose_player_mode == WHITE) {
            sprintf(CONFIG_PLAYER_WHITE_IDX_STR, "%d", idx);
            config_write();
        }
        choose_player_mode = NONE;
        break; }
    }

    return rc;
}

// - - - - - - - - -  TOURNAMENT MODE - - - - - - - - - - - - - - - 

static void render_tournament_mode(pane_cx_t *pane_cx)
{
    int i;

    // xxx comment
    render_board(pane_cx);

    // xxx comment
    register_event(pane_cx, 0, -4, SDL_EVENT_HELP, "HELP");

    // register event to go back to game mode
    register_event(pane_cx, 0, 0, SDL_EVENT_SET_GAME_MODE, "TOURNAMENT");

    // display tournament mode status
    print(pane_cx, 1.5, 0, "%5s vs %s", player_black->name, player_white->name);

    // xxx comment
    for (i = 0; i < MAX_TOURNAMENT_PLAYERS; i++) {
        print(pane_cx, 3+i, 0, 
                  "%5s : %3.0f %%",
                  tournament_players[i]->name,
                  100. * tournament.games_won[i] / tournament.games_played[i]);
    }

    // print total games
    print(pane_cx, 4.5+i, 0, "GAMES = %d", tournament.total_games_played);
}

static int event_tournament_mode(pane_cx_t *pane_cx, sdl_event_t *event)
{
    int rc = PANE_HANDLER_RET_DISPLAY_REDRAW;

    switch (event->event_id) {

    // tournament mode events
    case SDL_EVENT_SET_GAME_MODE:
        tournament.enabled = false;
        game_request = GAME_REQUEST_RESET;
        break;

    // common events
    case SDL_EVENT_HELP:
        help_mode = true;
        break;
    }

    return rc;
}

// - - - - - - - - -  HELP MODE - - - - - - - - - - - - - - - - - - 

static void render_help_mode(pane_cx_t *pane_cx)
{
    // help mode ...
    // xxx display help screen
    // - mouse wheel scroll
    // - BACK event at lower right

    // register event to exit help mode
    register_event(pane_cx, -1, -4, SDL_EVENT_EXIT_HELP, "BACK");
}

static int event_help_mode(pane_cx_t *pane_cx, sdl_event_t *event)
{
    int rc = PANE_HANDLER_RET_DISPLAY_REDRAW;

    switch (event->event_id) {

    case SDL_EVENT_EXIT_HELP:
        help_mode = false;
        break;
    }

    return rc;
}

// - - - - - - - - -  DISPLAY COMMON ROUTINES - - - - - - - - - - - 

static void render_board(pane_cx_t *pane_cx)
{
    rect_t *pane = &pane_cx->pane;
    game_moves_t *gm = &game_moves[max_game_moves-1];
    int r, c, color;

    // draw the 64 playing squares
    for (r = 1; r <= 8; r++) {
        for (c = 1; c <= 8; c++) {
            color = (gm->highlight[r][c] == 0 ? GREEN : 
                     gm->highlight[r][c] == 1 ? LIGHT_BLUE : 
                                                BLUE);
            sdl_render_fill_rect(pane, rc_to_loc(r,c), color);
        }
    }

    // draw the black and white pieces 
    for (r = 1; r <= 8; r++) {
        for (c = 1; c <= 8; c++) {
            rect_t loc = *rc_to_loc(r,c);
            int offset = loc.w/2 - piece_circle_radius;
            if (gm->board.pos[r][c] == BLACK) {
                sdl_render_texture(pane, loc.x+offset, loc.y+offset, piece_black_circle);
            } else if (gm->board.pos[r][c] == WHITE) {
                sdl_render_texture(pane, loc.x+offset, loc.y+offset, piece_white_circle);
            }
        }
    }
}

static void register_event(pane_cx_t *pane_cx, double r, double c, int event, char *fmt, ...)
{
    int x, y;
    char str[100];
    va_list ap;
    rect_t * pane = &pane_cx->pane;

    // event must not be 0
    if (event == 0) {
        FATAL("event is 0\n");
    }

    // get x, y
    x = (c >= 0 ? CTL_X + COL2X(c,FONTSZ) : win_width  + COL2X(c,FONTSZ));
    y = (r >= 0 ? CTL_Y + ROW2Y(r,FONTSZ) : win_height + ROW2Y(r,FONTSZ));

    // make str
    va_start(ap,fmt);
    vsprintf(str, fmt, ap);
    va_end(ap);

    // register event
    sdl_render_text_and_register_event(
        pane, x, y, FONTSZ, str, LIGHT_BLUE, BLACK, 
        event, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);
}

static void print(pane_cx_t *pane_cx, double r, double c, char *fmt, ...)
{
    int x, y;
    char str[100];
    va_list ap;
    rect_t * pane = &pane_cx->pane;

    // get x, y
    x = (c >= 0 ? CTL_X + COL2X(c,FONTSZ) : win_width  + COL2X(c,FONTSZ));
    y = (r >= 0 ? CTL_Y + ROW2Y(r,FONTSZ) : win_height + ROW2Y(r,FONTSZ));

    // make str
    va_start(ap,fmt);
    vsprintf(str, fmt, ap);
    va_end(ap);

    // register event
    sdl_render_text( pane, x, y, FONTSZ, str, WHITE, BLACK);
}

static rect_t *rc_to_loc(int r_arg, int c_arg)
{
    static rect_t loc[10][10];
    static bool first_call = true;
#ifdef ANDROID
    int win_dim = win_width;
#else
    int win_dim = win_height;
#endif

    if (first_call) {
        int r, c, i, sq_beg[8], sq_end[8];
        double tmp;

        tmp = (win_dim - 2) / 8.;
        for (i = 0; i < 8; i++) {
            sq_beg[i] = rint(2 + i * tmp);
        }
        for (i = 0; i < 7; i++) {
            sq_end[i] = sq_beg[i+1] - 3;
        }
        sq_end[7] = win_dim - 3;

        for (r = 1; r <= 8; r++) {
            for (c = 1; c <= 8; c++) {
                loc[r][c].x = sq_beg[c-1];
                loc[r][c].y = sq_beg[r-1];
                loc[r][c].w = sq_end[c-1] - sq_beg[c-1] + 1;
                loc[r][c].h = sq_end[r-1] - sq_beg[r-1] + 1;
            }
        }

        first_call = false;
    }

    return &loc[r_arg][c_arg];
}
