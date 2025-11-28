CFLAGS := -g -O2 -Wall -Wextra -I../da/ -I../lang/
LANG_OBJS := parser.o lex.o tree.o fail.o symbol.o symbol_table.o type.o tree_transform.o tac.o
OBJS   := main.o json.o json_rpc.o da.o log.o semantic_tokens.o util.o $(LANG_OBJS)

langls: $(OBJS)
	gcc $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	gcc $(CFLAGS) -c $? -o $@

da.o: ../da/da.c
	gcc $(CFLAGS) -c $? -o $@

$(LANG_OBJS): %.o: ../lang/%.c
	gcc $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.o langls
