CFLAGS := -g -Wall -Wextra -I../da/
OBJS   := main.o parser.o tree.o da.o lex.o symbol.o symbol_table.o type.o fail.o tac.o gen.o

langc: $(OBJS)
	gcc $(CFLAGS) -o langc $(OBJS)

%.o: %.c
	gcc $(CFLAGS) -c $? -o $@

da.o: ../da/da.c
	gcc $(CFLAGS) -c $? -o $@


.PHONY: test
test: langc
	python3 test/runner.py

.PHONY: lexer-test
lexer-test: lexer_test.o lex.o da.o fail.o tree.o symbol.o symbol_table.o
	gcc $(CFLAGS) -o $@ $?
	./$@

.PHONY: parser-test
parser-test: parser_test.o lex.o da.o fail.o tree.o symbol.o symbol_table.o parser.o
	gcc $(CFLAGS) -o $@ $?
	./$@

.PHONY: symbol-type-test
symbol-type-test: symbol_type_test.o lex.o da.o fail.o tree.o symbol.o symbol_table.o parser.o type.o
	gcc $(CFLAGS) -o $@ $?
	./$@


.PHONY: clean
clean:
	rm -f *.o lexer-test parser-test symbol-type-test langc langls *.S *.out
