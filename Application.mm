#include <cstdio>

int CYConsole(FILE *in, FILE *out, FILE *err);

int main() {
    return CYConsole(stdin, stdout, stderr);
}
