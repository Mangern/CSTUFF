CFLAGS := -g -Wall -Wextra -I../da/
OBJS   := main.o parser.o tree.o da.o lex.o symbol.o symbol_table.o type.o fail.o tac.o gen.o

langc: $(OBJS)
	gcc $(CFLAGS) -o langc $(OBJS)

%.o: %.c
	gcc $(CFLAGS) -c $? -o $@

da.o: ../da/da.c
	gcc $(CFLAGS) -c $? -o $@


.PHONY: clean
clean:
	rm *.o
