CFLAGS?=-std=gnu99 -D_GNU_SOURCE -Wall -O2 -g
CC=gcc

all: findpng3

findpng3: findpng3.c linked_list.c function.c
	$(CC) $(CFLAGS) -o findpng3 findpng3.c -lm -lz -lcurl -lrt -I/usr/include/libxml2 -lxml2
	
clean:
	rm -f *.o findpng3