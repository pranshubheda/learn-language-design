// expect: undefined variable 'inner'
fn main() {
    {
        let inner = 5;
    }
    print(inner);
}
