# Toy compiler

Just a compiler, designed with one goal in mind: trying out compiler construction theory in practice.

## Hello World

```ts
main: () -> void = {
    println("Hello World!");
}
```

## Something else

```ts
// Project Euler Problem 3: 
// - Find the largest prime factor of 600851475143
main: () -> void = {
    num := 600851475143;

    i := 2;
    while (i * i <= num) {
        while (num % i == 0) {
            if (i == num) {
                break;
            }
            num /= i;
        }
        i += 1;
    }

    println(num); // 6857
}
```

# Build and run

## Build

Please be on x86_64 linux and make sure you have `gcc` and `make`.

```bash
make

# Test
make test
```

## Usage (currently requires gcc in `$PATH`)

I use gcc as the assembler+linker.

```bash
./langc ./example-files/rule110.lang
```

## Run the compiled program

```bash
./a.out
```
