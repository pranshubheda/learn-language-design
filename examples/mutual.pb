// is_even calls is_odd before is_odd is defined. That works because the
// type checker collects every function signature in a first pass.

fn is_even(n: int) -> bool {
    if n == 0 {
        return true;
    }
    return is_odd(n - 1);
}

fn is_odd(n: int) -> bool {
    if n == 0 {
        return false;
    }
    return is_even(n - 1);
}

fn main() {
    print(is_even(10));
    print(is_odd(7));
    print(is_even(3) || is_odd(3));
    print(!is_even(4) && true);
}
