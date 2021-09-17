CFLAGS?=-std=c99 -D_GNU_SOURCE -Wall -O2
CC=gcc

all: paster

paster: paster.c zutil.c crc.c lab_png.h
	$(CC) $(CFLAGS) -o paster paster.c -lm -lz -lcurl -lpthread
	
clean:
	rm -f *.o paster *.png
