# Makefile for CPE464 program 3

CC= g++
CFLAGS= -g -D_GNU_SOURCE -Wall -pedantic -ansi 

# The  -lsocket -lnsl are needed for the sockets.
# The -L/usr/ucblib -lucb gives location for the Berkeley library needed for
# the bcopy, bzero, and bcmp.  The -R/usr/ucblib tells where to load
# the runtime library.

# The next line is needed on Sun boxes (so uncomment it if your on a
# sun box)
# LIBS =  -lsocket -lnsl

# For Linux/Mac boxes uncomment the next line - the socket and nsl
# libraries are already in the link path.
LIBS = -lstdc++ -lrt libcpe464.2.12.a

all:   rcopy server

rcopy: rcopy.cpp rcopy.h networks.h  networks.c cpe464.h
	$(CC) $(CFLAGS) -o rcopy rcopy.cpp networks.c $(LIBS)

server: server.cpp networks.h networks.c cpe464.h
	$(CC) $(CFLAGS) -o server server.cpp networks.c $(LIBS)

clean:
	rm -f *.o server rcopy
 
