// Scopes and shadowing, else-if chains, and short-circuit evaluation.

// Every path returns, so the "might not return" check passes.
fn classify(n: int) -> str {
    if n < 0 {
        return "negative";
    } else if n == 0 {
        return "zero";
    } else {
        return "positive";
    }
}

// A function with a side effect, so we can see whether it gets called.
fn loud(b: bool) -> bool {
    print("  (loud was called)");
    return b;
}

fn main() {
    // --- scopes ---
    let x = 1;
    {
        let x = 2;      // a NEW variable that shadows the outer x inside this block
        x = x + 10;     // assigns to the inner x
        print(x);       // 12
    }
    print(x);           // 1 (the outer x was never touched)

    // --- else-if ---
    print(classify(-5));
    print(classify(0));
    print(classify(7));

    // --- short circuit ---
    print("false && loud(true):");
    print(false && loud(true));     // loud is NOT called
    print("true || loud(false):");
    print(true || loud(false));     // loud is NOT called
    print("true && loud(true):");
    print(true && loud(true));      // loud IS called
}
