// RUN: clang -c -g -emit-llvm main.c -o main.bc
// RUN: klee -skip-functions-not=main main.bc

#include <stdarg.h>
#include "assert.h"

int sum(int N, ...) {
	return 0;
}
int main() {
	assert(sum(0, 1) == 0);
}
