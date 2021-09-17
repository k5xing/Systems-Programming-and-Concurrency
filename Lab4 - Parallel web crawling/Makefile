CFLAGS?=-std=gnu99 -D_GNU_SOURCE -Wall -O2 -g
CC=gcc

all: findpng2

findpng2: findpng2.c linked_list.c
	$(CC) $(CFLAGS) -o findpng2 findpng2.c -lm -lz -lcurl -lrt -lpthread -I/usr/include/libxml2 -lxml2
	
clean:
	rm -f *.o findpng2

# CC = gcc 
# CFLAGS_XML2 = $(shell xml2-config --cflags)
# CFLAGS_CURL = $(shell curl-config --cflags)
# CFLAGS = -Wall $(CFLAGS_XML2) $(CFLAGS_CURL) -std=gnu99 -g -DDEBUG1_
# LD = gcc
# LDFLAGS = -std=gnu99 -g 
# LDLIBS_XML2 = $(shell xml2-config --libs)
# LDLIBS_CURL = $(shell curl-config --libs)
# LDLIBS = $(LDLIBS_XML2) $(LDLIBS_CURL) 

# SRCS   = findpng2.c
# OBJS3  = findpng2.o
# TARGETS= findpng2

# all: ${TARGETS}

# findpng2: $(OBJS3) linked_list.c
# 	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

# %.o: %.c 
# 	$(CC) $(CFLAGS) -c $< 

# %.d: %.c
# 	gcc -MM -MF $@ $<

# -include $(SRCS:.c=.d)

# .PHONY: clean
# clean:
# 	rm -f *~ *.d *.o $(TARGETS) *.png *.html
