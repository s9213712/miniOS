// Host-side compile test for miniOS workflow.
//
// Build this with `make host-programs` and run the produced binary
// at build/host-programs/minios_host_compile_test_c.

#include <stdio.h>

int main(void) {
    const int a = 12;
    const int b = 5;
    const int c = a + b * 2;
    const int d = c - a;
    const int e = d / b;

    printf("compile test: a=%d b=%d\n", a, b);
    printf("expressions: (a+b*2)=%d, (c-a)=%d, ((c-a)/b)=%d\n", c, d, e);

    if (e == 2) {
        puts("expectation: e should be 2");
    } else {
        puts("unexpected result");
    }

    return 0;
}
