#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

#include <util_misc.h>

//
// defines
//

#define NONE   0
#define BLACK  1
#define WHITE  2

#define MOVE_PASS       -1
#define MOVE_GAME_OVER  -2
#define MOVE_NONE       -9

#define MOVE_TO_RC(m,r,c) \
    do { \
        (r) = (m) / 10; \
        (c) = (m) % 10; \
    } while (0)

#define RC_TO_MOVE(r,c,m) \
    do { \
        (m) = ((r) * 10) + (c); \
    } while (0)

#define OTHER_COLOR(c) ((c) == WHITE ? BLACK : \
                        (c) == BLACK ? WHITE : \
                                       ({FATAL("OTHER_COLOR c=%d\n", c);0;}))

#define MS 1000L

#define SWAP(a,b) \
    do { \
        typeof(a) temp;  \
        temp = (a);  \
        (a) = (b);  \
        (b) = temp;  \
    } while (0)

#define MEMBER_SIZE(type, member) sizeof(((type *)0)->member)

#define BOOK_MOVE_ENABLED  (!opt_book_move_disabled)
#define BOOK_MOVE_GEN_MODE (opt_book_move_gen_mode)

//
// typedefs
//

typedef struct {
    unsigned char pos[10][10];
    int black_cnt;
    int white_cnt;
    int whose_turn;
} board_t;

typedef struct {
    int move[64];
    int max;
    int color;
} possible_moves_t;

//
// variables
//

bool opt_fullscreen;
bool opt_book_move_disabled;
bool opt_book_move_gen_mode;

int  human_move_select;

extern char *version;

//
// prototypes
//

int human_get_move(int level, const board_t *b, char *eval_str);
int cpu_get_move(int level, const board_t *b, char *eval_str);
int old_get_move(int level, const board_t *b, char *eval_str);

bool move_cancelled(void);

void apply_move(board_t *b, int move, unsigned char highlight[][10]);
void get_possible_moves(const board_t *b, possible_moves_t *pm);
bool is_corner_move_possible(const board_t *b, int which_corner);

void bm_init(bool bm_gen_mode);
int bm_get_move(const board_t *b);
int get_max_bm_file(void);

// only for use by book_move_generator ...
void bm_add_move(const board_t *b, int move);

//
// inline procedures
//

static inline int min(int a, int b) {
    return a < b ? a : b;
}

static inline int max(int a, int b) {
    return a > b ? a : b;
}

static inline int32_t min32(int32_t a, int32_t b) {
    return a < b ? a : b;
}

static inline int32_t max32(int32_t a, int32_t b) {
    return a > b ? a : b;
}

static inline int64_t min64(int64_t a, int64_t b) {
    return a < b ? a : b;
}

static inline int64_t max64(int64_t a, int64_t b) {
    return a > b ? a : b;
}

static inline void setbit(uint8_t *bm, int idx) {
    bm[idx/8] |= (1 << (idx&7));
}

static inline bool getbit(uint8_t *bm, int idx) {
    return bm[idx/8] & (1 << (idx&7));
}

static inline char *bool2str(bool x) {
    return x ? "true" : "false";
}

#endif
