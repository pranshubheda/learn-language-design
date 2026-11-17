// Booleans, modulo, nested loops, and early return.

fn is_prime(n: int) -> bool {
    if n < 2 {
        return false;
    }
    let d: int = 2;
    while d * d <= n {
        if n % d == 0 {
            return false;
        }
        d = d + 1;
    }
    return true;
}

fn main() {
    let n = 0;
    let count = 0;
    while n < 30 {
        if is_prime(n) {
            print(n);
            count = count + 1;
        }
        n = n + 1;
    }
    print("primes found:");
    print(count);
}
