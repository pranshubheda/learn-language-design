// Recursion, while loops, typed parameters, and type inference.

fn fib(n: int) -> int {
    if n < 2 {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

fn main() {
    let i = 0;          // no annotation, so the type is inferred as int
    while i < 10 {
        print(fib(i));
        i = i + 1;
    }
}
