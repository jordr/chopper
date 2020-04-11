// RUN: %llvmgcc %s -emit-llvm -O0 -c -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --skip-functions-not=f3r --keep=f3w --output-dir=%t.klee-out %t1.bc > %t2.out 2> %t2.out
// RUN: FileCheck %s -input-file=%t2.out
// RUN: test ! -f %t.klee-out/test000001.ptr.err

// CHECK: completed paths = 1
// CHECK: recovery states = 0

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
