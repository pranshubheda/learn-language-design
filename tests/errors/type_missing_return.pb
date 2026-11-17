// expect: might not return a value on every path
fn sign(x: int) -> int {
    if x > 0 {
        return 1;
    }
}
fn main() {
    print(sign(3));
}
