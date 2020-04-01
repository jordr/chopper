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
