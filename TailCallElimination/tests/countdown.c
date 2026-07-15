#include <stdio.h>

int countdown(int n) {
    if (n == 0)
        return 0;
    return countdown(n - 1);
}

int main() {
    printf("%d\n", countdown(1000));
    return 0;
}