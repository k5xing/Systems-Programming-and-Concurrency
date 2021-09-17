CFLAGS?=-std=c99 -D_GNU_SOURCE -Wall -O2
CC=gcc

all: paster2

paster2: paster2.c zutil.c crc.c lab_png.h shm_stack.c
	$(CC) $(CFLAGS) -o paster2 paster2.c -lm -lz -lcurl -lrt -lpthread
	
clean:
	rm -f *.o paster2
