// RUN: %llvmgcc %s -emit-llvm -O0 -c -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --skip-functions=foo --output-dir=%t.klee-out %t1.bc > %t2.out 2> %t2.out
// RUN: FileCheck %s -input-file=%t2.out
// RUN: test ! -f %t.klee-out/test000001.ptr.err

// CHECK: 1 (good!)
// CHECK: recovery states = 2
// CHECK-NOT: (bad!)

#include <stdio.h>

int off = 0;
char* file = "12";

void foo(char *x) {
   x[0] = file[off++];
}

void bar(char* x) {
   x[0] = file[off++];
}

int main(int argc, char** argv) {
    char a, b;
    foo(&a);
    bar(&b);

    if (a == '1')
      printf("1 (good!)\n");
    else printf("%d (bad!)\n", a);
}
