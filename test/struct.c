#include "test.h"
int main()
{
    // [41] 支持 struct
    ASSERT(1, ({ struct {int a; int b;} x; x.a=1; x.b=2; x.a; }));
    ASSERT(2, ({ struct {int a; int b;} x; x.a=1; x.b=2; x.b; }));
    ASSERT(1, ({ struct {char a; int b; char c;} x; x.a=1; x.b=2; x.c=3; x.a; }));
    ASSERT(2, ({ struct {char a; int b; char c;} x; x.b=1; x.b=2; x.c=3; x.b; }));
    ASSERT(3, ({ struct {char a; int b; char c;} x; x.a=1; x.b=2; x.c=3; x.c; }));
    ASSERT(0, ({ struct {char a; char b;} x[3]; char *p=x; p[0]=0; x[0].a; }));
    ASSERT(1, ({ struct {char a; char b;} x[3]; char *p=x; p[1]=1; x[0].b; }));
    ASSERT(2, ({ struct {char a; char b;} x[3]; char *p=x; p[2]=2; x[1].a; }));
    ASSERT(3, ({ struct {char a; char b;} x[3]; char *p=x; p[3]=3; x[1].b; }));
    ASSERT(6, ({ struct {char a[3]; char b[5];} x; char *p=&x; x.a[0]=6; p[0]; }));
    ASSERT(7, ({ struct {char a[3]; char b[5];} x; char *p=&x; x.b[0]=7; p[3]; }));
    ASSERT(6, ({ struct { struct { char b; } a; } x; x.a.b=6; x.a.b; }));
    ASSERT(4, ({ struct {int a;} x; sizeof(x); }));
    ASSERT(8, ({ struct {int a; int b;} x; sizeof(x); }));
    ASSERT(8, ({ struct {int a, b;} x; sizeof(x); }));
    ASSERT(12, ({ struct {int a[3];} x; sizeof(x); }));
    ASSERT(16, ({ struct {int a;} x[4]; sizeof(x); }));
    ASSERT(24, ({ struct {int a[3];} x[2]; sizeof(x); }));
    ASSERT(2, ({ struct {char a; char b;} x; sizeof(x); }));
    ASSERT(0, ({ struct {} x; sizeof(x); }));

    // [42] 对齐结构体成员变量
    ASSERT(8, ({ struct {char a; int b;} x; sizeof(x); }));
    ASSERT(8, ({ struct {int a; char b;} x; sizeof(x); }));

    // [44] 支持结构体标签
    ASSERT(8, ({ struct t {int a; int b;} x; struct t y; sizeof(y); }));
    ASSERT(8, ({ struct t {int a; int b;}; struct t y; sizeof(y); }));
    ASSERT(2, ({ struct t {char a[2];}; { struct t {char a[4];}; } struct t y; sizeof(y); }));
    ASSERT(3, ({ struct t {int x;}; int t=1; struct t y; y.x=2; t+y.x; }));

    // [45] 支持->操作符
    ASSERT(3, ({ struct t {char a;} x; struct t *y = &x; x.a=3; y->a; }));
    ASSERT(3, ({ struct t {char a;} x; struct t *y = &x; y->a=3; x.a; }));

    // [47] 支持结构体赋值
    ASSERT(3, ({ struct {int a,b;} x,y; x.a=3; y=x; y.a; }));
    ASSERT(7, ({ struct t {int a,b;}; struct t x; x.a=7; struct t y; struct t *z=&y; *z=x; y.a; }));
    ASSERT(7, ({ struct t {int a,b;}; struct t x; x.a=7; struct t y, *p=&x, *q=&y; *q=*p; y.a; }));
    ASSERT(5, ({ struct t {char a, b;} x, y; x.a=5; y=x; y.a; }));

    // [48] 将 int 的大小由 8 改为 4
    ASSERT(3, ({ struct {int a,b;} x,y; x.a=3; y=x; y.a; }));
    ASSERT(7, ({ struct t {int a,b;}; struct t x; x.a=7; struct t y; struct t *z=&y; *z=x; y.a; }));
    ASSERT(7, ({ struct t {int a,b;}; struct t x; x.a=7; struct t y, *p=&x, *q=&y; *q=*p; y.a; }));
    ASSERT(5, ({ struct t {char a, b;} x, y; x.a=5; y=x; y.a; }));
    ASSERT(8, ({ struct t {int a; int b;} x; struct t y; sizeof(y); }));
    ASSERT(8, ({ struct t {int a; int b;}; struct t y; sizeof(y); }));

    // [49] 支持 long 类型
    ASSERT(16, ({ struct {char a; long b;} x; sizeof(x); }));

    // [50] 支持 short 类型
    ASSERT(4, ({ struct {char a; short b;} x; sizeof(x); }));

    printf("OK\n");
    return 0;
}