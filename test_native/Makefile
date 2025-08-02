CC=gcc
CFLAGS=-Wall -Wextra -g
OBJS=test_hid.o badge_hid_drivers.o badge_hid_decoder.o badge_hid_tokenizer.o

test: $(OBJS)
	$(CC) -o $@ $(OBJS)

clean:
	rm -f *.o test
