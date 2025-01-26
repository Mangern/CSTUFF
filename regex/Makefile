

.PHONY: build
build:
	cmake -B build && cmake --build build

preprocess-test: build
	./build/preprocess_test

nfa-test: build
	./build/nfa_test

dfa-test: build
	./build/dfa_test

.PHONY: test
test: nfa-test dfa-test
