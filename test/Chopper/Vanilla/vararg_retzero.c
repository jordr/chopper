// RUN: %llvmgcc %s -emit-llvm -O0 -c -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --skip-functions=sum --output-dir=%t.klee-out %t1.bc > %t2.out 2> %t2.out
// RUN: FileCheck %s -input-file=%t2.out
// RUN: test ! -f %t.klee-out/test000001.ptr.err

// CHECK-NOT: ASSERTION FAIL
// CHECK-NOT: recovery states = 0

#include <stdarg.h>
#include "assert.h"

int sum(int N, ...) {
	return 0;
}
int main() {
	assert(sum(0, 1) == 0);
}
