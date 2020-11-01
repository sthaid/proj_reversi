#include <common.h>

// XXX comments
// using depth 11 needs  max_bm_file = 3472624
// using depth 10 needs  max_bm_file =  514083
// using depth  9 needs  max_bm_file =   80068
// using depth  8 needs  max_bm_file =   12823

//
// defines
//

#define MAX_THREAD         100
#define DEFAULT_MAX_THREAD 4

//#define DEBUG_BMG

//
// variables
//

static int             max_thread;
static int             bm_added;
static int             bm_already_exists;
static int             bm_being_processed_by_another_thread;
static int             active_thread_count;
static int             board_sequence_num[MAX_THREAD];
static bool            ctrl_c;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static __thread int    tid;

//
// prototypes
//

static void usage(void);
static void signal_handler(int sig);
static void *bm_gen_thread(void *cx);
static void generate_book_moves(board_t *b, int depth);
static void debug(char *str, board_t *b, int move);

// -----------------  MAIN  ------------------------------------------

int main(int argc, char **argv)
{
    long i;
    pthread_t thread_id;
    unsigned long prog_start_us;
    struct sigaction act;

    // get max_thread, in allowd range 1..MAX_THREAD
    max_thread = DEFAULT_MAX_THREAD;
    if (argc >= 2 && sscanf(argv[1], "%d", &max_thread) != 1) {
        usage();
    } 
    if (max_thread < 1 || max_thread > MAX_THREAD) {
        usage();
    }
    INFO("max_thread = %d\n", max_thread);

    // register ctrl-c handler
    memset(&act, 0, sizeof(act));
    act.sa_handler = signal_handler;
    sigaction(SIGINT, &act, NULL);

    // initialize book move utils, in bm_gen_mode
    bm_init(true);

    // start the threads
    prog_start_us = microsec_timer();
    active_thread_count = max_thread;
    for (i = 0; i < max_thread; i++) {
        pthread_create(&thread_id, NULL, bm_gen_thread, (void*)(i));
    }

    // wait for the threads to complete, or be interrupted by ctrl_c
    // XXX cleanup, and comments
    while (active_thread_count > 0 && ctrl_c == false) {
        int start_bm_added, intvl_bm_added;
        unsigned long start_us;
        double intvl_minutes, total_minutes;

        start_us = microsec_timer();
        start_bm_added = bm_added;
        while (true) {
            sleep(1);
            intvl_minutes = (microsec_timer() - start_us) / (60. * 1000000);
            if (intvl_minutes > 1 || active_thread_count == 0 || ctrl_c == true) {
                break;
            }
        }
        if (ctrl_c) {
            break;
        }
        intvl_bm_added = bm_added - start_bm_added;
        total_minutes = (microsec_timer() - prog_start_us) / 60000000.;

        INFO("max_bm_file=%d  added=%d  skipped=%d,%d  intvl_rate=%0.1f  overall_rate=%0.1f /min\n",
              bm_get_max_bm_file(), 
              bm_added, 
              bm_already_exists,
              bm_being_processed_by_another_thread,
              intvl_bm_added / intvl_minutes,
              bm_added / total_minutes);
    }

    // for caution exit with the mutex locked so that when exitting due to ctrl-c
    // the threads can not be writing the reversi.book file
    pthread_mutex_lock(&mutex);

    // print reason for exit
    INFO("exitting: %s\n",
         (ctrl_c ? "CTRL_C" : active_thread_count == 0 ? "THREADS_COMPLETED" : "????"));

    // print duration and rate, in this format:
    //   duration: x.x days  OR  hh:mm:ss
    //   rate:     x.x moves added per minute
    int hours, minutes, seconds, total_secs;
    total_secs = (microsec_timer() - prog_start_us) / 1000000;
    if (total_secs >= 86400) {
        INFO("duration: %0.1f days\n", total_secs/86400.);
    } else {
        seconds  = total_secs;
        hours    = seconds/3600;
        seconds -= hours*3600;
        minutes  = seconds/60;
        seconds -= minutes*60;
        INFO("duration: %02d:%02d:%02d\n", hours, minutes, seconds);
    }
    INFO("rate:     %0.1f moves added per minute\n", bm_added / (total_secs/60.));

    // exit
    return 0;
}

static void usage(void)
{
    INFO("usage: book_move_generator [<max_thread>]\n");
    INFO("          max_thread: 1..%d\n", MAX_THREAD);
    exit(1);
}

static void signal_handler(int sig)
{
    ctrl_c = true;
}

bool move_cancelled(void)
{
    return false;
}

// -----------------  BOOK MOVE GENERATOR THREAD  --------------------

static void *bm_gen_thread(void *cx)
{
    board_t b;
    int i;

    // save thread id (tid), this variable is declared with thread local storage
    tid = (int)(long)cx;
    INFO("thread %d starting\n", tid);

    // init the board_t 'b'
    memset(&b, 0, sizeof(board_t));
    b.pos[4][4]  = WHITE;
    b.pos[4][5]  = BLACK;
    b.pos[5][4]  = BLACK;
    b.pos[5][5]  = WHITE;
    b.black_cnt  = 2;
    b.white_cnt  = 2;
    b.whose_turn = BLACK;

    // generate book moves for all possible board configurations
    // that can occur in the first 10 moves, this is has been measured
    // to generate a reversi.book file with 514083 entries
    pthread_mutex_lock(&mutex);
    for (i = 0; i < 10; i++) {
        DEBUG("%d: calling generate_book_moves for depth %d\n", tid, i);
        generate_book_moves(&b, i);
    }
    pthread_mutex_unlock(&mutex);

    // thread is terminating
    INFO("thread %d done\n", tid);
    __sync_fetch_and_sub(&active_thread_count, 1);
    return NULL;
}
    
static void generate_book_moves(board_t *b, int depth)
{
    int i, move;
    possible_moves_t pm;
    board_t b_child;

    #define CHILD(mv) \
        ({ b_child = *b; \
           apply_move(&b_child, mv, NULL); \
           &b_child; })

    // when depth is 0 then the board will be considered for addition 
    // to the book moves file (reversi.book)
    // - if already in the file then skip
    // - if another thread is in the process of processing this same board then skip
    // - otherwise call cpu_book_move_generate to get the move, and call
    //   bm_add_move to add the move to the reversi.book file
    if (depth == 0) {
        // maintain a sequence number for each 'b';
        // this is used below to prevent multiple threads from
        //  simultaneously processing the same 'b'
        board_sequence_num[tid]++;

        // if this 'b' already has a book move then return
        move = bm_get_move(b);
        if (move != MOVE_NONE) {
            debug("ALREADY IN BOOK", b, move);
            bm_already_exists++;
            return;
        }

        // if another thread is processing this 'b' then return
        for (i = 0; i < max_thread; i++) {
            if (i == tid) continue;
            if (board_sequence_num[i] == board_sequence_num[tid]) {
                bm_being_processed_by_another_thread++;
                return;
            }
        }

        // determine the book move for 'b'
        pthread_mutex_unlock(&mutex);
        move = cpu_book_move_generator(b);
        pthread_mutex_lock(&mutex);

        // add the book move to the file
        debug("ADDING BOOK MOVE", b, move);
        bm_add_move(b, move);
        bm_added++;

        // return
        return;
    }

    // depth is not 0, so make recursive call to generate_book_moves
    get_possible_moves(b, &pm);
    for (i = 0; i < pm.max; i++) {
        generate_book_moves(CHILD(pm.move[i]), depth-1);
    }
}

static void debug(char *str, board_t *b, int move)
{
    char line[8][50];
    int i, r, c;

#ifndef DEBUG_BMG
    return;
#endif

    for (i = 0; i < 8; i++) {
        strcpy(line[i], ". . . . . . . .");
    }

    for (r = 1; r <= 8; r++) {
        for (c = 1; c <= 8; c++) {
            if (b->pos[r][c] == WHITE) {
                line[r-1][(c-1)*2] = 'w';
            } else if (b->pos[r][c] == BLACK) {
                line[r-1][(c-1)*2] = 'b';
            }
        }
    }

    MOVE_TO_RC(move,r,c);
    if (b->whose_turn == WHITE) {
        line[r-1][(c-1)*2] = 'W';
    } else if (b->whose_turn == BLACK) {
        line[r-1][(c-1)*2] = 'B';
    } else {
        FATAL("whose_turn = %d\n", b->whose_turn);
    }

    INFO("%d %s\n", tid, str);
    for (i = 0; i < 8; i++) {
        INFO("  %s\n", line[i]);
    }
}
