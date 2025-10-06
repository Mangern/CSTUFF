# Selfhosting??

The current state of this program is that it is able to compile this:

```ts
// Hello World
main: () -> void = {
    println("Hello", "world!");
}
```

through stdin and stdout.

```bash
# "Bootstrapping"
./langc ./langc.lang

./a.out < hello.lang | gcc -x assembler -o hello -

./hello # Hello world!
```
