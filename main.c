#include <common.h>

#include <util_misc.h>
#include <util_sdl.h>

//
// defines
//

#define DEFAULT_WIN_WIDTH    1500
#define DEFAULT_WIN_HEIGHT   1002   // xxx 125 * 8  + 2

#define MAX_AVAIL_PLAYERS    (sizeof(avail_players) / sizeof(void*))

#define GAME_STATE_NOT_STARTED   0
#define GAME_STATE_ACTIVE        1
#define GAME_STATE_COMPLETE      2
#define GAME_STATE_RESTART       3

//
// typedefs
//

typedef struct {
    int pos[10][10];
} possible_moves_t;

//
// variables
//

static int       win_width  = DEFAULT_WIN_WIDTH;
static int       win_height = DEFAULT_WIN_HEIGHT;

static int       game_state = GAME_STATE_NOT_STARTED;

static board_t   board;
static int       whose_turn;
static possible_moves_t possible_moves;  // xxx indent

static player_t *player_black;
static player_t *player_white;

static player_t *avail_players[] = { &human, &computer };

//
// prototypes
//

static void game_init(char *player_black_name, char *player_white_name);
static void *game_thread(void *cx);
static void set_game_state(int new_game_state);
static int get_game_state(void);

static void get_possible_moves(int my_color);
static void clear_possible_moves(void);

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
        if (strcmp(player_black_name, avail_players[i]->name) == 0) {
            player_black = avail_players[i];
        }
        if (strcmp(player_white_name, avail_players[i]->name) == 0) {
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

restart:
    // init board
    memset(board.pos, REVERSI_EMPTY, sizeof(board.pos));
    board.pos[4][4] = REVERSI_WHITE;
    board.pos[4][5] = REVERSI_BLACK;
    board.pos[5][4] = REVERSI_BLACK;
    board.pos[5][5] = REVERSI_WHITE;
    // xxx other fields like number of white,black

    // xxx
    whose_turn = 0;  // XXX
    set_game_state(GAME_STATE_ACTIVE);

    // loop until game is finished
    // XXX may need a mutex around display update
    // XXX may want to slow down processing when human not invovled
    while (true) {
        // XXX make this a loop
        whose_turn = REVERSI_BLACK;
        get_possible_moves(REVERSI_BLACK);
        move = player_black->get_move(&board, REVERSI_BLACK);
        clear_possible_moves();
        if (game_restart_requested()) goto restart;
        if (move == MOVE_GAME_OVER) break;
        apply_move(&board, REVERSI_BLACK, move);  // XXX check to ensure it succeeded

        whose_turn = REVERSI_WHITE;
        get_possible_moves(REVERSI_WHITE);
        move = player_white->get_move(&board, REVERSI_WHITE);
        clear_possible_moves();
        if (game_restart_requested()) goto restart;
        if (move == MOVE_GAME_OVER) break;
        apply_move(&board, REVERSI_WHITE, move);
    }

    // game is over
    whose_turn = 0; //XXX
    set_game_state(GAME_STATE_COMPLETE);

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
    bool succ;

    succ = false;
    other_color = (my_color == REVERSI_WHITE ? REVERSI_BLACK : REVERSI_WHITE);
    MOVE_TO_RC(move, r, c);

    if (b->pos[r][c] != REVERSI_EMPTY) {
        // AAA or return failure
        FATAL("pose[%d][%d] is not empty\n", r, c);
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
        }
    }

    if (succ) {
        b->pos[r][c] = my_color;
    }

    return succ;
}

// -----------------  GAME UTILS - PRIVATE XXX  -----------------------------------

static void get_possible_moves(int my_color)
{
    int r, c, i, other_color;

    clear_possible_moves();

    other_color = (my_color == REVERSI_WHITE ? REVERSI_BLACK : REVERSI_WHITE);

    for (r = 1; r <= 8; r++) {
        for (c = 1; c <= 8; c++) {
            if (board.pos[r][c] != REVERSI_EMPTY) {
                continue;
            }

            for (i = 0; i < 8; i++) {
                int r_incr = r_incr_tbl[i];
                int c_incr = c_incr_tbl[i];
                int r_next = r + r_incr;
                int c_next = c + c_incr;
                int cnt    = 0;

                while (board.pos[r_next][c_next] == other_color) {
                    r_next += r_incr;
                    c_next += c_incr;
                    cnt++;
                }

                if (cnt > 0 && board.pos[r_next][c_next] == my_color) {
                    possible_moves.pos[r][c] = my_color;
                    break;
                }
            }
        }
    }
}

static void clear_possible_moves(void)
{
    memset(&possible_moves, 0, sizeof(possible_moves));
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

    #define SDL_EVENT_GAME_RESTART   (SDL_EVENT_USER_DEFINED + 0)
    #define SDL_EVENT_SELECT_MOVE    (SDL_EVENT_USER_DEFINED + 10)   // length 64

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
        int r, c, move;
        rect_t loc;

        for (r = 1; r <= 8; r++) {
            for (c = 1; c <= 8; c++) {
                RC_TO_LOC(r,c,loc);
                sdl_render_fill_rect(pane, &loc, GREEN);

                if (board.pos[r][c] == REVERSI_BLACK) {
                    sdl_render_texture(pane, loc.x+12, loc.y+12, black_circle);
                } else if (board.pos[r][c] == REVERSI_WHITE) {
                    sdl_render_texture(pane, loc.x+12, loc.y+12, white_circle);
                } 

                if (possible_moves.pos[r][c] == REVERSI_BLACK) {
                    sdl_render_texture(pane, loc.x+52, loc.y+52, small_black_circle);
                } else if (possible_moves.pos[r][c] == REVERSI_WHITE) {
                    sdl_render_texture(pane, loc.x+52, loc.y+52, small_white_circle);
                } 

                RC_TO_MOVE(r,c,move);
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
            "%s %s %s",
            "BLACK", player_black->name, (whose_turn == REVERSI_BLACK ? "<=" : ""));

        sdl_render_printf(
            pane, 1030, ROW2Y(3,40), 40, WHITE, BLACK, 
            "%s %s %s",
            "WHITE", player_white->name, (whose_turn == REVERSI_WHITE ? "<=" : ""));
            
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
            INFO("move_select = %d\n", move_select);
            break; }
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
// XXX may want a mutex unless we can atomically flip the board and new_board
pane_hndlr
- button to start the game
#endif
