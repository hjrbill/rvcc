// 使用 POSIX.1 标准 (引入需要的 strndup 函数)
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// 共用头文件，定义了多个文件间共同使用的函数和数据
//

//
// 词法分析
//
typedef enum
{
    TK_IDENT,   // 标识符，变量名，函数名等
    TK_PUNCT,   // 操作符
    TK_KEYWORD, // 关键字
    TK_NUM,     // 数字
    TK_EOF,     // 终止符
} TokenKind;    // 终结符

typedef struct Token Token;

struct Token
{
    TokenKind kind;
    Token *next;

    int Val;
    char *Loc; // 在字符串中的位置

    int Len; // 长度
};

// 错误信息提示函数
void error(char *Fmt, ...);
void errorAt(char *Loc, char *Fmt, ...);
void errorTok(Token *Tok, char *Fmt, ...);

// 判断 Token 与 Str 的关系
bool equal(Token *Tok, char *Str);
Token *skip(Token *Tok, char *Str);

// 词法分析入口函数
Token *tokenize(char *Input);

//
// 语法分析
//

// AST 中二叉树节点种类
typedef enum
{
    ND_RETURN, // 返回

    ND_EXPR_STMT, // 表达式语句
    ND_VAR,       // 变量

    ND_ASSIGN, // 赋值
    ND_NEG,    // 负号

    ND_EQ, // ==
    ND_NE, // !=
    ND_LT, // <
    ND_LE, // <=

    ND_ADD, // +
    ND_SUB, // -
    ND_MUL, // *
    ND_DIV, // /

    ND_INT // 整形
} NodeKind;

typedef struct Node Node;

typedef struct Obj Obj;
struct Obj
{
    Obj *next; // 下一个对象

    char *name; // 对象名
    int offset; // 相对于 fp 的偏移量
};

struct Node
{
    NodeKind kind;

    Node *next; // 指向下一语句

    Node *LHS;
    Node *RHS;

    Obj *Var; // ND_VAR 类型的变量名
    int Val;  // ND_INT 类型的值
};

typedef struct Func Func;
struct Func
{
    Node *body;    // 函数体
    Obj *locals;   // 函数的局部变量
    int stackSize; // 栈深度
};

// 语法解析入口函数
Func *parse(Token *Tok);

//
// 语义分析与代码生成
//

// 代码生成入口函数
void codegen(Func *node);