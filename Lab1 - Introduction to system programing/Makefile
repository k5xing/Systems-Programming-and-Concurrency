CFLAGS?=-std=c99 -D_GNU_SOURCE -Wall -O2
CC=gcc

all: pnginfo findpng catpng

pnginfo: pnginfo.c lab_png.h crc.c
	$(CC) $(CFLAGS) -o pnginfo pnginfo.c -lm

findpng: findpng.c lab_png.h crc.c linked_list.c
	$(CC) $(CFLAGS) -o findpng findpng.c

catpng: catpng.c zutil.c crc.c lab_png.h
	$(CC) $(CFLAGS) -o catpng catpng.c -lm -lz
	
clean:
	rm -f *.o pnginfo findpng catpng
