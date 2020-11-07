TARGETS = reversi book_move_generator

CC = gcc
OUTPUT_OPTION=-MMD -MP -o $@
CFLAGS = -Wall -g -O2 -Iutil -I.

util/util_sdl.o: CFLAGS += $(shell sdl2-config --cflags)
#util/util_sdl.o: CFLAGS += -DENABLE_UTIL_SDL_BUTTON_SOUND

SRC_REVERSI = main.c \
              human.c \
              cpu.c \
              oldd.c \
              game_utils.c \
              util/util_misc.c \
              util/util_sdl.c \
              util/util_jpeg.c \
              util/util_png.c

SRC_BOOK_MOVE_GENERATOR = \
	      book_move_generator.c \
	      game_utils.c \
              cpu.c \
              util/util_misc.c 

DEP = $(SRC_REVERSI:.c=.d) \
      $(SRC_BOOK_MOVE_GENERATOR:.c=.d) 

#
# build rules
#

all: $(TARGETS)

reversi: $(SRC_REVERSI:.c=.o)
	echo "char *version = \"`git log -1 --format=%h`\";" > version.c
	$(CC) -o $@ $(SRC_REVERSI:.c=.o) version.c \
              -lpthread -lm -ljpeg -lpng -lSDL2 -lSDL2_ttf

book_move_generator: $(SRC_BOOK_MOVE_GENERATOR:.c=.o)
	$(CC) -o $@ $(SRC_BOOK_MOVE_GENERATOR:.c=.o) \
              -lpthread -lm

-include $(DEP)

#
# clean rule
#

clean:
	rm -f $(TARGETS) $(DEP) $(DEP:.d=.o) version.c
