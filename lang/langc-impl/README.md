# Selfhosting??

The goal now is to only develop LANG in LANG.

```ts
// Hello World
main: () -> void = {
    println("Hello", "world!");
}
```

```bash
# "Bootstrapping"
./langc -o langself ./langc.lang

./langself < hello.lang | gcc -x assembler -o hello -

./hello # Hello world!
```

## Passing tests
Not matching expected output on error.

-    [x] `rule110.lang`
-    [x] `arr-2d.lang`
-    [x] `arr.lang`
-    [x] `boolean_op.lang`
-    [x] `break.lang`
-    [x] `call-type.lang`
-    [x] `cast.lang`
-    [x] `charlit.lang`
-    [x] `continue.lang`
-    [x] `edge.lang`
-    [x] `err.lang`
-    [x] `euler3.lang`
-    [x] `geqleq.lang`
-    [x] `hello.lang`
-    [x] `infer.lang`
-    [ ] `many-args.lang`
-    [x] `memory.lang`
-    [x] `new-syntax.lang`
-    [x] `opassign.lang`
-    [x] `operators.lang`
-    [x] `pointer.lang`
-    [ ] `real.lang`
-    [x] `rec.lang`
-    [x] `scope.lang`
-    [x] `sieve.lang`
-    [x] `simple-expr.lang`
-    [x] `simple-func.lang`
-    [x] `simple-if.lang`
-    [x] `simple-struct.lang`
-    [x] `simple-typedef.lang`
-    [x] `simple-var.lang`
-    [x] `simple-while.lang`
-    [x] `sorting.lang`
-    [x] `stack-arr.lang`
