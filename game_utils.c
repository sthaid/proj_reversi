#include <common.h>

//
// defines
//

//
// variables
//

//
// prototypes
//


// -----------------  GAME BOARD SUPPORT  -----------------------------------------

//                          0   1   2   3   4   5  6  7
static int r_incr_tbl[8] = {0, -1, -1, -1,  0,  1, 1, 1};
static int c_incr_tbl[8] = {1,  1,  0, -1, -1, -1, 0, 1};

void  apply_move(board_t *b, int move, unsigned char highlight[][10])
{
    int  r, c, i, j, my_color, other_color;
    int *my_color_cnt, *other_color_cnt;
    bool succ;

    if (move == MOVE_PASS) {
        b->whose_turn = OTHER_COLOR(b->whose_turn);
        return;
    }

    my_color = b->whose_turn;
    other_color = OTHER_COLOR(my_color);

    succ = false;
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

    if (!succ) {
        FATAL("invalid call to apply_move, move=%d\n", move);
    }

    if (highlight) {
        sleep(1);
        memset(highlight, 0, 100);
    }

    b->whose_turn = OTHER_COLOR(b->whose_turn);
}

void get_possible_moves(const board_t *b, possible_moves_t *pm)
{
    int r, c, i, my_color, other_color, move;

    my_color = b->whose_turn;
    other_color = OTHER_COLOR(my_color);

    pm->max = 0;
    pm->color = my_color;

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

bool is_corner_move_possible(const board_t *b, int which_corner)
{
    static struct {
        int r;
        int c;
        int r_incr_tbl[3];
        int c_incr_tbl[3];
    } tbl[4] = {         // which_corner: 
        { 1,1, {0, 1, 1}, { 1, 0, 1} },    // 0: top left
        { 1,8, {0, 1, 1}, {-1, 0,-1} },    // 1: top right
        { 8,8, {0,-1,-1}, {-1, 0,-1} },    // 2: bottom right
        { 8,1, {0,-1,-1}, { 1, 0, 1} },    // 3: bottom left
                    };

    int r, c, i, my_color, other_color;

    r = tbl[which_corner].r;
    c = tbl[which_corner].c;
    if (b->pos[r][c] != NONE) {
        return false;
    }

    my_color    = b->whose_turn;
    other_color = OTHER_COLOR(my_color);
    for (i = 0; i < 3; i++) {
        int r_incr = tbl[which_corner].r_incr_tbl[i];
        int c_incr = tbl[which_corner].c_incr_tbl[i];
        int r_next = r + r_incr;
        int c_next = c + c_incr;
        int cnt    = 0;

        while (b->pos[r_next][c_next] == other_color) {
            r_next += r_incr;
            c_next += c_incr;
            cnt++;
        }

        if (cnt > 0 && b->pos[r_next][c_next] == my_color) {
            return true;
        }
    }

    return false;
}

// -----------------  BOOK MOVE SUPPORT  ------------------------------------------

// Notes:
//
// The reversi.book file is an array of bm_t;
// except the first entry, bm_file[0], is the file hdr which contains 
// a magic number.
//
// MAX_BM_HASHTBL should be > 2*max_bm_file. This is a recommendation,
// and not an absolute requirement.
//
// MAX_BM_FILE is used only in bm gen mode, to provide a large array
// of bm_file entries that are added to by calls to bm_add_move().

#define BM_ASSETNAME       "reversi.book"
#define MAX_BM_HASHTBL     (1 << 16)   // must be pwr of 2
#define CRC_TO_HTIDX(crc)  ((crc) & (MAX_BM_HASHTBL-1))
#define MAGIC_BM_FILE      0x44434241
#define MAX_BM_FILE        100000

// must be invoked with a bm that is part of bm_file
#define ADD_BM_TO_HASHTBL(bm) \
    do { \
        int htidx = CRC_TO_HTIDX((bm)->crc); \
        (bm)->hashtbl_next = bm_hashtbl[htidx]; \
        bm_hashtbl[htidx] = (bm) - bm_file; \
    } while (0)

typedef struct {
    unsigned short data[9];
} bm_sig_t;

typedef struct {
    bm_sig_t     sig;
    short        pad;
    unsigned int crc;
    int          move;
    int          hashtbl_next;
} bm_t;

static bm_t *bm_file;
static int   max_bm_file;
static int   bm_hashtbl[MAX_BM_HASHTBL];
static bool  bm_gen_mode;

static void create_sig(const unsigned char pos[][10], int whose_turn, bm_sig_t *sig);
static void rotate(unsigned char pos[][10]);
static void flip(unsigned char pos[][10]);
static unsigned int crc32(const void *buf, size_t size);
static void bm_add_move_debug(const board_t *b, int move);

// --------- public  -----------------

void bm_init(bool bm_gen_mode_arg)
{
    size_t filesize;
    int i;

    // expect sizeof bm_t to be 32
    if (sizeof(bm_t) != 32) {
        FATAL("sizeof bm_t = %zd\n", sizeof(bm_t));
    }

    // save bm_gen_mode_arg in file global for later use
    bm_gen_mode = bm_gen_mode_arg;

    // debug print
    if (bm_gen_mode) {
        INFO("book move generator is ENABLED\n");
    }

    // if running book move generator and asset file does not exist then
    // create the asset file and write it's hdr
    if (bm_gen_mode && does_asset_file_exist(BM_ASSETNAME) == false) {
        static bm_t hdr;
        *(int*)&hdr = MAGIC_BM_FILE;
        INFO("creating empty %s\n", BM_ASSETNAME);
        create_asset_file(BM_ASSETNAME);
        write_asset_file(BM_ASSETNAME, &hdr, sizeof(hdr));
    }
            
    // read book move file
    bm_file = read_asset_file(BM_ASSETNAME, &filesize);
    if (bm_file == NULL) {
        FATAL("assetfile %s, failed read\n", BM_ASSETNAME);
    }

    // validate filesize, which must not be 0 becuase there should be at least
    // the header; and must be a multiple of sizeof(bm_t)
    if ((filesize == 0) || (filesize % sizeof(bm_t))) {   
        FATAL("assetfile %s, invalid size %zd\n", BM_ASSETNAME, filesize);
    }

    // if running the book move generator then realloc a large bm_file array,
    // to support the generator adding entries
    if (bm_gen_mode) {
        bm_file = realloc(bm_file, MAX_BM_FILE*sizeof(bm_t));
        if (bm_file == NULL) {
            FATAL("realloc failed\n");
        }
    }

    // the first bm_t entry in bm_file is actually a header, 
    // which just contains a magic number;
    // verify file magic number
    if (*(int*)bm_file != MAGIC_BM_FILE) {
        FATAL("assetfile %s, invalid magic 0x%x\n", BM_ASSETNAME, *(int*)bm_file);
    }

    // set max_bm_file with the number of entries in the file;
    // note - the header is not an entry, thus the '- 1'
    max_bm_file = filesize / sizeof(bm_t) - 1;
    INFO("max_bm_file=%d  MAX_BM_FILE=%d  MAX_BM_HASHTBL=%d\n", 
         max_bm_file, MAX_BM_FILE, MAX_BM_HASHTBL);

    // loop over all entries, starting at 1 to skip the header;
    // and add each entry to the bm_hashtbl
    for (i = 1; i <= max_bm_file; i++) {
        ADD_BM_TO_HASHTBL(&bm_file[i]);
    }
}

int bm_get_move(const board_t *b)
{
    unsigned char pos[10][10];
    unsigned char move[10][10];
    int r, c, i, whose_turn, htidx;
    bm_sig_t sig;
    bm_t *bm;

    // if no bm_file then return MOVE_NONE;
    // this supports case when bm_init has not been called
    if (bm_file == NULL) {
        return MOVE_NONE;
    }

    // init variables
    memcpy(pos, b->pos, sizeof(pos));
    whose_turn = b->whose_turn;

    memset(move, 0, sizeof(move));
    for (r = 1; r <= 8; r++) {
        for (c = 1; c <= 8; c++) {
            RC_TO_MOVE(r,c,move[r][c]);
        }
    }

    // there are 8 instances of boards that are rotated or mirror images;
    // loop over these 8 instances
    for (i = 0; i < 8; i++) {
        // create signature for this instance, and
        create_sig(pos, whose_turn, &sig);

        // Lookup this instance in the hasttbl.
        // If found in the hashtbl then return the move.
        // Since this instance is rotated/flipped the move that is returned
        //  is converted to be consistent with the board that was passed in
        //  to this routine.
        htidx = CRC_TO_HTIDX(crc32(&sig,sizeof(sig)));
        bm = (bm_hashtbl[htidx] == 0 ? NULL : &bm_file[bm_hashtbl[htidx]]);
        while (bm) {
            if (memcmp(&bm->sig, &sig, sizeof(bm_sig_t)) == 0) {
                MOVE_TO_RC(bm->move, r, c);
                return move[r][c];
            }
            bm = (bm->hashtbl_next == 0 ? NULL : &bm_file[bm->hashtbl_next]);
        }

        // rotate and flip to convert pos to the next instance
        rotate(pos);
        rotate(move);
        if (i == 3) {
            flip(pos);
            flip(move);
        }
    }

    // b was not found, so return MOVE_NONE
    return MOVE_NONE;
}

void bm_add_move(const board_t *b, int move)
{
    bm_t new_bm_ent;

    // bm_init must have been called to enable book move generator mode
    if (bm_gen_mode == false || bm_file == NULL) {
        FATAL("bm_gen_mode must be enabled\n");
    }

    // if book move file is full then fatal error
    if (max_bm_file+1 == MAX_BM_FILE) {
        FATAL("bookmark file is full, max_bm_file=%d\n", max_bm_file);
    }

    // move arg should never be MOVE_NONE
    if (move == MOVE_NONE) {
        FATAL("move arg must not be MOVE_NONE\n");
    }

    // if there is already a book move entry for 'b' then fatal error
    if (bm_get_move(b) != MOVE_NONE) {
        FATAL("entry already exists\n");
        return;
    }

    // create the bm_t
    create_sig(b->pos, b->whose_turn, &new_bm_ent.sig);
    new_bm_ent.crc = crc32(&new_bm_ent.sig, sizeof(bm_sig_t));
    new_bm_ent.move = move;
    new_bm_ent.hashtbl_next = 0;

    // add the bm_t to bm_file array, and to bm_hashtbl
    //  (note that the '+1' is to skip the header entry)
    bm_file[max_bm_file+1] = new_bm_ent;
    ADD_BM_TO_HASHTBL(&bm_file[max_bm_file+1]);
    max_bm_file++;

    // write the bm_t to the end of the book move asset file
    write_asset_file(BM_ASSETNAME, &new_bm_ent, sizeof(bm_t));

    // debug print the board/move that has been added
    bm_add_move_debug(b, move);
}

int get_max_bm_file(void)
{
    return max_bm_file;
}

// --------- private  -----------------

static void create_sig(const unsigned char pos[][10], int whose_turn, bm_sig_t *sig)
{
    int r;

    // set sig->data[0..7]
    for (r = 1; r <= 8; r++) {
        sig->data[r-1] = (pos[r][1] << 14) |
                         (pos[r][2] << 12) |
                         (pos[r][3] << 10) |
                         (pos[r][4] <<  8) |
                         (pos[r][5] <<  6) |
                         (pos[r][6] <<  4) |
                         (pos[r][7] <<  2) |
                         (pos[r][8] <<  0);
    }

    // set sig->data[8]
    sig->data[8] = whose_turn;
}

static void rotate(unsigned char pos[][10])
{
    int r, c;
    unsigned char A, B, C, D;

    for (r = 1; r <= 4; r++) {
        for (c = 1; c <= 4; c++) {
            A = pos[r][c];
            B = pos[c][9-r];
            C = pos[9-r][9-c];
            D = pos[9-c][r];

            pos[r][c]      = D;
            pos[c][9-r]    = A;
            pos[9-r][9-c]  = B;
            pos[9-c][r]    = C;
        }
    }
}

static void flip(unsigned char pos[][10])
{
    int r;

    for (r = 1; r <= 8; r++) {
        SWAP(pos[r][1], pos[r][8]);
        SWAP(pos[r][2], pos[r][7]);
        SWAP(pos[r][3], pos[r][6]);
        SWAP(pos[r][4], pos[r][5]);
    }
}

// https://web.mit.edu/freebsd/head/sys/libkern/crc32.c
static const uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static uint32_t crc32(const void *buf, size_t size)
{
    const uint8_t *p = buf;
    uint32_t crc;

    crc = ~0U;
    while (size--) {
        crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ ~0U;
}

static void bm_add_move_debug(const board_t *b, int move)
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

    INFO("-------------------------\n");
    INFO("adding move %02d, max_bm_file = %d\n", move, max_bm_file);
    for (i = 0; i < 8; i++) {
        INFO("  %s\n", line[i]);
    }
    INFO("-------------------------\n");
}

