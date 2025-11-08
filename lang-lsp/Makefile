CFLAGS := -g -O2 -Wall -Wextra -I../da/
OBJS   := main.o json.o da.o log.o

langls: $(OBJS)
	gcc $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	gcc $(CFLAGS) -c $? -o $@

da.o: ../da/da.c
	gcc $(CFLAGS) -c $? -o $@

