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
    TK_PUNCT, // 操作符
    TK_NUM,   // 数字
    TK_EOF,   // 终止符
} TokenKind;  // 终结符

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
    ND_EQ, // ==
    ND_NE, // !=
    ND_LT, // <
    ND_LE, // <=

    ND_ADD, // +
    ND_SUB, // -
    ND_MUL, // *
    ND_DIV, // /

    ND_NEG, // 负号

    ND_INT // 整形
} NodeKind;

typedef struct Node Node;
struct Node
{
    NodeKind kind;
    Node *LHS;
    Node *RHS;
    int Val;
};

// 语法解析入口函数
Node *parse(Token *Tok);

//
// 语义分析与代码生成
//

// 代码生成入口函数
void codegen(Node *node);