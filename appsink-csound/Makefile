C=gcc
CFLAGS=-Wall `pkg-config --cflags --libs gstreamer-1.0 ` -lgstapp-1.0 -lgstaudio-1.0 -DUSE_DOUBLE -lcsound64
OBJ = chan.o
channels: main.c
	gcc -o app main.c $(CFLAGS)
