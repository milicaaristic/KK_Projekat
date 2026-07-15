#include <stdio.h>
int f(int n) {
    int x;
    if (n > 0)
        x = f(n - 1);
    else
        x = 0;
    return x + 1;
}
int main() { printf("%d\n", f(5)); return 0; }   // 6