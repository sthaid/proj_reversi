#include <common.h>

// XXX prevent dups when using multiple threads
//      but multiple threads may not be helping

#define MAX_THREAD 2

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int bm_added;
static int bm_already_exists;
static int active_thread_count;

static void *bm_gen_thread(void *cx);
static void generate_book_moves(board_t *b, int depth);
static void dbgpr(char *str, board_t *b, int move);

// XXX also explore book moves in normal game play

// XXX multi threaded

// -----------------------------------------------------

int main(int argc, char **argv)
{
    long i;
    pthread_t thread_id[MAX_THREAD];
    unsigned long start_us, end_us;

    bm_init(true);

    start_us = microsec_timer();

    for (i = 0; i < MAX_THREAD; i++) {
        __sync_fetch_and_add(&active_thread_count, 1);
        pthread_create(&thread_id[i], NULL, bm_gen_thread, (void*)(i));
    }

    // xxx term on ctrlc, or when all threads are done
    while (active_thread_count > 0) {
        sleep(1);
        INFO("max_bm_file=%d  bm_added=%d  bm_already_exists=%d\n", 
             bm_get_max_bm_file(), bm_added, bm_already_exists);
    }

    end_us = microsec_timer();
    INFO("duration %ld secs\n", (end_us - start_us) / 1000000);

    return 0;
}

bool move_cancelled(void)
{
    return false;
}

// -----------------------------------------------------

__thread int tid;

static void *bm_gen_thread(void *cx)
{
    board_t b;
    int i;

    tid = (int)(long)cx;
    INFO("thread %d starting\n", tid);


    memset(&b, 0, sizeof(board_t));
    b.pos[4][4]  = WHITE;
    b.pos[4][5]  = BLACK;
    b.pos[5][4]  = BLACK;
    b.pos[5][5]  = WHITE;
    b.black_cnt  = 2;
    b.white_cnt  = 2;
    b.whose_turn = BLACK;

    pthread_mutex_lock(&mutex);
#if 1
    // using depth 11 needs  max_bm_file = 3472624
    // using depth 10 needs  max_bm_file =  514083
    // using depth  8 needs  max_bm_file =     tbd
    // using depth  8 needs  max_bm_file =   12823
    for (i = 0; i < 10; i++) {
        INFO("%d: calling generate_book_moves for depth %d\n", tid, i);
        generate_book_moves(&b, i);
    }
#else
    generate_book_moves(&b, 3);
#endif
    pthread_mutex_unlock(&mutex);

    INFO("thread %d done\n", tid);
    __sync_fetch_and_sub(&active_thread_count, 1);

    return NULL;
}
    
// -----------------------------------------------------

int working_on_board[MAX_THREAD];

static void generate_book_moves(board_t *b, int depth)
{
    int i, move;
    possible_moves_t pm;
    board_t b_child;

    #define CHILD(mv) \
        ({ b_child = *b; \
           apply_move(&b_child, mv, NULL); \
           &b_child; })

    if (depth == 0) {
        working_on_board[tid]++;

        // if this 'b' already has a book move then return
        move = bm_get_move(b);
        if (move != MOVE_NONE) {
            //dbgpr("ALREADY IN BOOK", b, move);
            __sync_fetch_and_add(&bm_already_exists,1);
            return;
        }

        //if another thread is working on this one then return
        //indicate that this thread is working it
        //if any other thread is working on 
        bool skip = false;
        for (i = 0; i < MAX_THREAD; i++) {
            if (i == tid) continue;
            if (working_on_board[i] == working_on_board[tid]) {
                skip = true;
                break;
            }
        }
        if (skip) return;

        // xxx can't call this
        pthread_mutex_unlock(&mutex);
        move = cpu_book_move_generator(b);
        pthread_mutex_lock(&mutex);

        //dbgpr("ADDING BOOK MOVE", b, move);

        bm_add_move(b, move);

        __sync_fetch_and_add(&bm_added, 1);
        return;
    }

    get_possible_moves(b, &pm);
    for (i = 0; i < pm.max; i++) {
        generate_book_moves(CHILD(pm.move[i]), depth-1);
    }
}

static void dbgpr(char *str, board_t *b, int move)
{
    char line[8][50];
    int i, r, c;

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
