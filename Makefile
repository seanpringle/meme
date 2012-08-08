GTKINC=$(shell pkg-config --cflags gtk+-2.0 webkit-1.0)
GTKLIB=$(shell pkg-config --libs gtk+-2.0 webkit-1.0)

INCS = -I. -I/usr/include ${GTKINC}
LIBS = -L/usr/lib -lc ${GTKLIB} -lgthread-2.0 -ljavascriptcoregtk-1.0

CFLAGS = -Wall -Os
LDFLAGS = -g

CC = cc

all:
	${CC} ${CFLAGS} ${INCS} ${LDFLAGS} ${LIBS} -o meme meme.c
