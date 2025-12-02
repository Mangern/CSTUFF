# Selfhosting??

The current state of this program is that it is able to compile more then this:

```ts
// Hello World
main: () -> void = {
    println("Hello", "world!");
}
```

through stdin and stdout.

```bash
# "Bootstrapping"
./langc -o langself ./langc.lang

./langself < hello.lang | gcc -x assembler -o hello -

./hello # Hello world!
```

## Passing tests
Not matching expected output on error.

-    [ ] `arr-2d.lang`
-    [x] `arr.lang`
-    [x] `boolean_op.lang`
-    [x] `break.lang`
-    [ ] `call-type.lang`
-    [ ] `cast.lang`
-    [ ] `charlit.lang`
-    [x] `continue.lang`
-    [ ] `edge.lang`
-    [x] `err.lang`
-    [x] `euler3.lang`
-    [x] `geqleq.lang`
-    [x] `hello.lang`
-    [ ] `infer.lang`
-    [ ] `libc.lang`
-    [ ] `many-args.lang`
-    [ ] `memory.lang`
-    [x] `new-syntax.lang`
-    [ ] `opassign.lang`
-    [x] `operators.lang`
-    [ ] `pointer.lang`
-    [ ] `real.lang`
-    [x] `rec.lang`
-    [x] `scope.lang`
-    [ ] `sieve.lang`
-    [x] `simple-expr.lang`
-    [x] `simple-func.lang`
-    [x] `simple-if.lang`
-    [ ] `simple-struct.lang`
-    [ ] `simple-typedef.lang`
-    [x] `simple-var.lang`
-    [x] `simple-while.lang`
-    [x] `sorting.lang`
-    [x] `stack-arr.lang`
