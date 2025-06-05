# Toy compiler

Just a compiler, designed with one goal in mind: trying out compiler construction theory in practice.

## Hello World

```ts
main := () -> void {
    println("Hello World!");
}
```

## Something else

```ts
// Project Euler Problem 3: 
// - Find the largest prime factor of 600851475143
main := () -> void {
    int num := 600851475143;

    int i := 2;

    int ans := num;
    while (!(i * i > num)) {
        while (num % i == 0) {
            if (i == num) {
                break;
            }
            num := num / i;
        }
        i := i + 1;
    }

    println(num);
}
```

# Build and run

## Build
```bash
make
```

## Compile to assembly
```bash
./langc -a ./test-files/sieve.lang > out.S
```

## Compile the assembly

Use `as` and `ld`, or just simply use `gcc`:

```bash
gcc out.S
```
