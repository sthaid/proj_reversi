    // NOTES - Black goes first


    // select players
    player1 = human;
    player2 = computer;

    // init board, and 
    // display board


    // loop until game is finished
    while (true) {
        // get player1 move
        move = player1->get_move(board);

        // apply move to board
        apply_move(move, board);

        // if game over then break
        if (game_over(board)) {
            break;
        }

        whose_turn = player2;
        update_display();


xxx
        // get player2 move
        // apply move to board
        // display board
        // if game over then break
    }

    // 



player methods
- get_move HUMAN and COMPUTER do it differently
- get_name
- get_color
- set_color




evaluate(board, color)
    if game over 
      return value
    endif

    if reached search depth
      return value based on number of corners - number of opponent cornders
                  AND
             number of possible moves
    endif

    get_best_move


public helper routines

- get_best_move(board, color)
     returns MOVE, PASS, GAME_OVER
- get_possible_moves(board, color, retlist)
- apply_move(board, color, move, new_board)

private helper routines
- is_game_over(board, color)

        
get_best_move  XXX this should be part of the computer player code:w
    get possible moves list

    loop over possible moves list
        apply the move -> gets a new board for opponents turn

        xxx[] = evaluate board from opponents perspective
    endloop

    find the lowest xxx, and return it

    //MISC IDEAS
    //  - first few moves are random, so games are different
    //  - if xxx is a tie then choose randomly




