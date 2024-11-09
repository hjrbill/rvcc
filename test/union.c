
#include "test.h"
int main()
{
    // [46] 支持 union
    ASSERT(8, ({ union { int a; char b[6]; } x; sizeof(x); }));
    ASSERT(3, ({ union { int a; char b[4]; } x; x.a = 515; x.b[0]; }));
    ASSERT(2, ({ union { int a; char b[4]; } x; x.a = 515; x.b[1]; }));
    ASSERT(0, ({ union { int a; char b[4]; } x; x.a = 515; x.b[2]; }));
    ASSERT(0, ({ union { int a; char b[4]; } x; x.a = 515; x.b[3]; }));

    // [47] 支持结构体赋值
    ASSERT(3, ({ struct {int a,b;} x,y; x.a=3; y=x; y.a; }));
    ASSERT(7, ({ struct t {int a,b;}; struct t x; x.a=7; struct t y; struct t *z=&y; *z=x; y.a; }));
    ASSERT(7, ({ struct t {int a,b;}; struct t x; x.a=7; struct t y, *p=&x, *q=&y; *q=*p; y.a; }));
    ASSERT(5, ({ struct t {char a, b;} x, y; x.a=5; y=x; y.a; }));

    printf("OK\n");
    return 0;
}