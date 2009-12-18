CFLAGS=-Wall -O2
LDFLAGS=-lm

c64koala2ppm: c64koala2ppm.o
c64koala2ppm.o: c64koala2ppm.c

clean:
	rm -f *.o c64koala2ppm
