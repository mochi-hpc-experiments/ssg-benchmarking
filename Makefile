CC=mpicc
CFLAGS=-Wall -g $(shell pkg-config --libs ssg)

all: ssg-observe-group ssg-launch-group

clean:
	rm -f ssg-observe-group ssg-launch-group *.o
