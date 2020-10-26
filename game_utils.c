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


// -----------------  XXXXXXXXXXXXXX  ---------------------------------------------

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
        memset(highlight, 0, 100);  // XXX sizeof
    }

    b->whose_turn = OTHER_COLOR(b->whose_turn);
}

void get_possible_moves(board_t *b, possible_moves_t *pm)
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

// -----------------  XXXXXXXXXXXXXX  ---------------------------------------------

#if 0
// --------------------------------------------------------------------------
// -----------------  BOOK MOVE XXXXXXXXXXXX  -------------------------------
// --------------------------------------------------------------------------

book_move_file_read
{
    open file   ALSO create
    read records,    validate MAGIC
    add entries to hash tbl
    close file
}

book_move_get(board_t *b)
{
    also rotate and flip

    compress board

    hash_idx =func(compressed_board)

    search across for matching b

    if not found return -1

    return the move
}    

board_rotate

board_flip




http://home.thep.lu.se/~bjorn/crc/crc32_simple.c
/* Simple public domain implementation of the standard CRC32 checksum.
 * Outputs the checksum for each file given as a command line argument.
 * Invalid file names and files that cause errors are silently skipped.
 * The program reads from stdin if it is called with no arguments. */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

uint32_t crc32_for_byte(uint32_t r) {
  for(int j = 0; j < 8; ++j)
    r = (r & 1? 0: (uint32_t)0xEDB88320L) ^ r >> 1;
  return r ^ (uint32_t)0xFF000000L;
}

void crc32(const void *data, size_t n_bytes, uint32_t* crc) {
  static uint32_t table[0x100];
  if(!*table)
    for(size_t i = 0; i < 0x100; ++i)
      table[i] = crc32_for_byte(i);
  for(size_t i = 0; i < n_bytes; ++i)
    *crc = table[(uint8_t)*crc ^ ((uint8_t*)data)[i]] ^ *crc >> 8;
}

int main(int ac, char** av) {
  FILE *fp;
  char buf[1L << 15];
  for(int i = ac > 1; i < ac; ++i)
    if((fp = i? fopen(av[i], "rb"): stdin)) { 
      uint32_t crc = 0;
      while(!feof(fp) && !ferror(fp))
	crc32(buf, fread(buf, 1, sizeof(buf), fp), &crc);
      if(!ferror(fp))
	printf("%08x%s%s\n", crc, ac > 2? "\t": "", ac > 2? av[i]: "");
      if(i)
	fclose(fp);
    }
  return 0;
}

#endif
