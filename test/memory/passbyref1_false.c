//@expect fail
#include <assert.h>
#include <stddef.h>

void klee_make_symbolic(void *addr, size_t nbytes, const char *name);
int __VERIFIER_nondet_int(void);
void update(int* val);

int main(void)
{
    int loc = 0;
    int c = 0;

    klee_make_symbolic(&loc, sizeof(loc), "loc");

    for (int i = 0; i < 5; ++i) {
        update(&c);
        loc = loc + c;
    }

    assert(loc != 0);

    return 0;
}