.DEFAULT_GOAL := grammar
OBJ := lex.o lr.o test.o ../da/da.o
EXE := grammar

%.o: %.c
	gcc -g -c $? -o $@

$(EXE): $(OBJ)
	gcc -g -o $@ $(OBJ)

clean:
	rm $(OBJ) $(EXE)
