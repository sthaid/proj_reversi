#include <common.h>

#include <util_misc.h>

int main(int argc, char **argv)
{
    bm_init(true);

    return 0;
}



#if 0
// --------------------------------------------------------------------------
// -----------------  BOOK MOVE GENERATOR  ----------------------------------
// --------------------------------------------------------------------------

XXX add whose_move to board

main
{
    book_move_file_read

    pthread_create

    wait for terminate
}

thread
{
    for (i = 0; i < 10; i++) {
        generate_book_moves(&b, i);
    }
}

book_move_generator(board_t *b, int depth)
{
    if (depth == 0) {
        book_move = get_book_move(b);
        if (book_move != -1) {
            return;
        }

        if another thread is working on this one then return
        indicate that this thread is working it

        drop mutex
        get_move
        acquire mutes

        write to file   NEED A ROUTINE
        return
    }

    get possible moves
    for (i = 0; i < pm.max) {
        create child board
        book_move_generator(child, depth-1);
    }
}



book_move_file_append_new_entry
{
    open for append
    write
    close
}

http://home.thep.lu.se/~bjorn/crc/crc32_simple.c
#endif

