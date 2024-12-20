#include "test.h"
/*
 * This is a block comment.
 */
int main()
{
    // [10] 支持{...}
    ASSERT(3, ({ 1; {2;} 3; }));

    // [11] 支持空语句
    ASSERT(5, ({ ;;; 5; }));
    ASSERT(10, ({ int i=0; while(i<10) i=i+1; i; }));
    ASSERT(55, ({ int i=0; int j=0; while(i<=10) {j=i+j; i=i+1;} j; }));

    // [12] 支持 if 语句
    ASSERT(3, ({ int x; if (0) x=2; else x=3; x; }));
    ASSERT(3, ({ int x; if (1-1) x=2; else x=3; x; }));
    ASSERT(2, ({ int x; if (1) x=2; else x=3; x; }));
    ASSERT(2, ({ int x; if (2-1) x=2; else x=3; x; }));

    // [13] 支持 for 语句
    ASSERT(55, ({ int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; j; }));
    
    // [14] 支持 while 语句
    ASSERT(10, ({ int i=0; while(i<10) i=i+1; i; }));

    // [40] 支持 , 运算符
    ASSERT(3, (1, 2, 3));
    ASSERT(5, ({ int i=2, j=3; (i=5,j)=6; i; }));
    ASSERT(6, ({ int i=2, j=3; (i=5,j)=6; j; }));

    printf("OK\n");
    return 0;
}