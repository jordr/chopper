// a.txt contains "12"
// run with klee -libc=uclibc -posix-runtime -split-search -skip-functions-not=bar,ioctl,printf posix.bc

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
