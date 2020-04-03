// RUN: %llvmgcc %s -emit-llvm -O0 -c -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee -libc=uclibc -posix-runtime --skip-functions=foo --output-dir=%t.klee-out %t1.bc > %t2.out 2> %t2.out
// RUN: FileCheck %s -input-file=%t2.out
// RUN: test ! -f %t.klee-out/test000001.ptr.err

// CHECK: 1 (good!)
// CHECK-NOT (bad!)
// CHECK-NOT: recovery states = 0

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

int fd;

void foo(char *x) {
   read(fd, x, 1);
}

void bar(char* x) {
   read(fd, x, 1);
}

int main(int argc, char** argv) {
   fd = open("a.txt", O_RDONLY);
   assert(fd != -1);
   char a, b;
   foo(&a);
   bar(&b);

   if (a == '1')
     printf("1 (good!)\n");
   else printf("%d (bad!)\n", a);
}
