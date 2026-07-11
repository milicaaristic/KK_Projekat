#include <stdio.h>

int sum(int n, int acc) {
    if (n == 0)
        return acc;
    return sum(n - 1, acc + n); 
}

int main() {
    printf("%d\n", sum(100, 0)); 
    return 0;
}