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
bool consume(Token **Rest, Token *Tok, char *Str);

// 词法分析入口函数
Token *tokenize(char *Input);

//
// 语法分析
//

typedef struct Node Node;
typedef struct Type Type;

// 语法分析 (类型系统)

// 类型种类
typedef enum
{
    TY_INT,  // int 整型
    TY_PTR,  // 指针
    TY_FUNC, // 函数
} TypeKind;

struct Type
{
    TypeKind kind;
    Type *base;
    Token *name; // 其类型对应的名称，如：变量名、函数名

    // 函数类型
    Type *ReturnTy; // 函数返回的类型
    Type *Params;   // 形参
    Type *next;     // 下一类型
};

// 声明一个全局变量，定义在 type.c 中。
extern Type *TyInt;
// 判断是否是整形
bool isInteger(Type *Ty);
// 构建一个指针类型，并指向基类
Type *pointerTo(Type *Base);
// 构建函数类型
Type *funcType(Type *ReturnTy);
// 复制类型
Type *copyType(Type *Ty);
// 为所有节点赋予类型
void addType(Node *node);

// 语法分析 (抽象语法树构建)

// AST 中二叉树节点种类
typedef enum
{
    ND_RETURN, // 返回

    ND_BLOCK,   // { ... }，代码块
    ND_FUNCALL, // 函数调用

    ND_VAR, // 变量

    ND_IF,        // if 语句
    ND_FOR,       // for | while 语句 (while 是 for 的一种特殊情况)
    ND_EXPR_STMT, // 表达式语句

    ND_ASSIGN, // 赋值
    ND_NEG,    // 负号

    ND_ADDR,  // 取地址 &
    ND_DEREF, // 解引用 *

    ND_EQ, // ==
    ND_NE, // !=
    ND_LT, // <
    ND_LE, // <=

    ND_ADD, // +
    ND_SUB, // -
    ND_MUL, // *
    ND_DIV, // /

    ND_NUM // 整形
} NodeKind;

typedef struct Obj Obj;
struct Obj
{
    Obj *next; // 下一个对象

    Type *type; // 变量类型
    char *name; // 对象名
    int offset; // 相对于 fp 的偏移量
};

struct Node
{
    NodeKind kind;
    Token *Tok;

    Node *next; // 指向下一语句

    Node *LHS;
    Node *RHS;

    // if 语句或 for 语句
    Node *Cond; // 条件语句
    Node *Then; // true 走向的语句
    Node *Else; // false 走向的语句
    Node *Init; // 初始化语句
    Node *Inc;  // 递增语句

    Node *Body; // 代码块

    char *FuncName; // 函数名
    Node *Args;     // 函数参数

    Obj *Var;   // ND_VAR 类型的变量名
    Type *type; // 节点中的数据的类型
    int Val;    // ND_NUM 类型的值
};

typedef struct Func Func;
struct Func
{
    Func *next; // 下一个函数

    char *name;  // 函数名
    
    Node *body;    // 函数体

    Obj *Params;   // 形参
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