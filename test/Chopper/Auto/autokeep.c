// RUN: %llvmgcc %s -emit-llvm -O0 -c -o %t1.bc
// RUN: rm -rf %t2.klee-out %t3.klee-out
// RUN: %klee --libc=uclibc --skip-functions-not=f3r --autokeep=1 --output-dir=%t2.klee-out %t1.bc > %t2.out 2> %t2.out
// RUN: %klee --libc=uclibc --skip-functions-not=f3r --autokeep=0 --output-dir=%t3.klee-out %t1.bc > %t3.out 2> %t3.out
// RUN: FileCheck %s -input-file=%t2.out -check-prefix=CHECK-WITH1 --check-prefix=CHECK-WITH2
// RUN: FileCheck %s -input-file=%t3.out -check-prefix=CHECK-WITHOUT

// CHECK-WITH1: (recovery)
// CHECK-WITH2: completed paths = 1
// CHECK-WITHOUT: reached "unreachable" instruction

#include <klee/klee.h>
#include <assert.h>
int global, a3;

void f3r() {
    global = a3;
}

void f3w() {
    a3 = 1;
}

int main(int argc, char *argv[]) {
    f3w();
    f3r();

    return 0;
}
