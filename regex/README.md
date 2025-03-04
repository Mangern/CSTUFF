# Regex

Regular expressions.

```c
char* regex = "(int|float|double) +[a-zA-Z_]\\w* *= *(-|\\+)*\\d+(\\.\\d*f?)?;";
dfa_t* dfa = dfa_from_regex(regex, strlen(regex));
printf("Will it match? %d\n", dfa_accepts(dfa, "int x = 3;", strlen("int x = 3;"))); // 1
printf("Will it match? %d\n", dfa_accepts(dfa, "float    f =3.0f;", strlen("float    f =3.0f;"))); // 1
printf("Will it match? %d\n", dfa_accepts(dfa, "int x = 3.0.0;", strlen("int x = 3.0.0;"))); // 0
printf("Will it match? %d\n", dfa_accepts(dfa, "double x3 = 3.0;", strlen("double x3 = 3.0;"))); // 1
printf("Will it match? %d\n", dfa_accepts(dfa, "double 3x = 3.0;", strlen("double 3x = 3.0;"))); // 0
```
