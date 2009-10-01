#include <cstdio>
#include <substrate.h>

int CYConsole(FILE *in, FILE *out, FILE *err);

@ /**/protocol a
- (void) a:(int)m;
@end

int main() {
    return CYConsole(stdin, stdout, stderr);
}
