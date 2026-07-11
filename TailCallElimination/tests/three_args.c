#include <stdio.h>

int f(int n, int a, int b) {
    if (n == 0)
        return a + b;
    return f(n - 1, b, a);
}

int main() {
    printf("%d\n", f(10, 3, 7));   // ocekivano: 10
    return 0;
}