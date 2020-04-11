// RUN: %llvmgcc %s -emit-llvm -O0 -c -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --skip-functions-not=f3r --output-dir=%t.klee-out %t1.bc > %t2.out 2> %t2.out
// RUN: FileCheck %s -input-file=%t2.out
// RUN: test -f %t.klee-out/test000001.assert.err

// CHECK: ASSERTION FAIL: !a3
// CHECK: recovery states = 1

#include <klee/klee.h>
#include <assert.h>
int a3;

void f3r() {
    assert(!a3);
}

void f3w() {
    a3 = 1;
}

void f1() {
    f2();
}

int f2() {
    f3r();
    return 0;
}

int main(int argc, char *argv[]) {
    a3 = 0;
    f3w();
    f1();
    return 0;
}
