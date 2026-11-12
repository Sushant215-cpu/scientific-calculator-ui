# Simple Scientific Calculator

A clean, light-blue and white Windows scientific calculator app written in C.

## Features

- Standard and scientific calculator modes
- Degrees and radians
- Arithmetic, powers, factorials, and parentheses
- Trigonometry, logarithms, roots, absolute value, floor, and ceiling
- Calculator memory with `M+`, `M-`, `MR`, and `MC`
- Persistent calculation history
- Rounded, compact display with a simple modern interface

## Run the app

Download and open `scientific_calculator_app.exe` on Windows.

## Build from source

With MinGW GCC:

```powershell
gcc -std=c11 -Wall -Wextra -pedantic scientific_calculator_app.c -o scientific_calculator_app.exe -mwindows -lm
```

## Expression examples

```text
2 + 3 * 4
sin(30)
sqrt(81)
log(100)
2^8
5!
```
