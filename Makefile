CC=gcc
CFLAGS=-Wall -Wextra

all: tarsau

tarsau: tarsau.c
	$(CC) $(CFLAGS) tarsau.c -o tarsau

clean:
	rm -f tarsau *.sau