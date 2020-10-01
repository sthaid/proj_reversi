TARGETS = reversi

CC = gcc
OUTPUT_OPTION=-MMD -MP -o $@
CFLAGS = -Wall -g -O2 -Iutil -I.

util/util_sdl.o: CFLAGS += $(shell sdl2-config --cflags)
#util/util_sdl.o: CFLAGS += -DENABLE_UTIL_SDL_BUTTON_SOUND

SRC_REVERSI = main.c \
              human.c \
              cpu.c \
              cpu_random.c \
              util/util_misc.c \
              util/util_sdl.c \
              util/util_jpeg.c \
              util/util_png.c

DEP = $(SRC_REVERSI:.c=.d) 

#
# build rules
#

all: $(TARGETS)

reversi: $(SRC_REVERSI:.c=.o)
	$(CC) -o $@ $(SRC_REVERSI:.c=.o) \
              -lpthread -lm -ljpeg -lpng -lSDL2 -lSDL2_ttf

-include $(DEP)

#
# clean rule
#

clean:
	rm -f $(TARGETS) $(DEP) $(DEP:.d=.o)
