CFLAGS := -g -Wall -Wextra -I../da/
OBJS   := main.o parser.o tree.o da.o lex.o symbol.o symbol_table.o type.o fail.o tac.o gen.o

langc: $(OBJS)
	gcc $(CFLAGS) -o langc $(OBJS)

%.o: %.c
	gcc $(CFLAGS) -c $? -o $@

da.o: ../da/da.c
	gcc $(CFLAGS) -c $? -o $@


.PHONY: lexer-test
lexer-test: lexer_test.o lex.o da.o fail.o tree.o symbol.o symbol_table.o
	gcc $(CFLAGS) -o lexer-test $?
	./lexer-test

.PHONY: clean
clean:
	rm *.o
