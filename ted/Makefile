CFLAGS := -g -O2 -Wall -Wextra -I../da/
OBJS   := gap_buffer.o

ted: $(OBJS) main.o
	gcc $(CFLAGS) -o ted $(OBJS) test.o

%.o: %.c
	gcc $(CFLAGS) -c $? -o $@

test: $(OBJS) test.o
	gcc $(CFLAGS) -o test $(OBJS) test.o && ./test

.PHONY: clean
clean:
	rm -f *.o test ted
