CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -O2
TARGETS = parent child1 child2

all: $(TARGETS)

parent: parent.c
	$(CC) $(CFLAGS) -o parent parent.c

child1: child1.c
	$(CC) $(CFLAGS) -o child1 child1.c

child2: child2.c
	$(CC) $(CFLAGS) -o child2 child2.c

clean:
	rm -f $(TARGETS) child1.txt child2.txt
