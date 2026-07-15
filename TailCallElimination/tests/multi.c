#include <stdio.h>
int f(int n) {
    if (n > 0) return f(n - 1);
    if (n < 0) return f(n + 1);
    return 0;
}
int main() { 
    printf("%d\n", f(5)); 
    return 0; 
} 