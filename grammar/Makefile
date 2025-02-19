.DEFAULT_GOAL := grammar
OBJ := lex.o lr.o test.o ../da/da.o
EXE := grammar

$(EXE): $(OBJ)
	gcc -o $@ $(OBJ)

clean:
	rm $(OBJ) $(EXE)
