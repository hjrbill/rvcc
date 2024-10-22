#include "rvcc.h"

// expr = equality
// equality = relational ("==" relational | "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add = mul ("+" mul | "-" mul)*
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-" ) unary | primary
// primary = "(" add ")" | num
static Node *expr(Token **Rest, Token *Tok);
static Node *equality(Token **Rest, Token *Tok);
static Node *relational(Token **Rest, Token *Tok);
static Node *add(Token **Rest, Token *Tok);
static Node *mul(Token **Rest, Token *Tok);
static Node *unary(Token **Rest, Token *Tok);
static Node *primary(Token **Rest, Token *Tok);

static Node *newNode(NodeKind kind)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

// 创建二元运算节点
static Node *newBinary(NodeKind kind, Node *lhs, Node *rhs)
{
    Node *node = newNode(kind);
    node->LHS = lhs;
    node->RHS = rhs;
    return node;
}

// 创建一元运算节点
static Node *newUnary(NodeKind kind, Node *lhs)
{
    Node *node = newNode(kind);
    node->LHS = lhs;
    return node;
}

// 创建数字（叶子）节点
static Node *newNumNode(int Val)
{
    Node *node = newNode(ND_INT);
    node->Val = Val;
    return node;
}

// expr = equality
// @param Rest 用于向上传递仍需要解析的 Token 的首部
// @param Tok 当前正在解析的 Token
static Node *expr(Token **Rest, Token *Tok)
{
    return equality(Rest, Tok);
}

// equality = relational ("==" relational | "!=" relational)*
// @param Rest 用于向上传递仍需要解析的 Token 的首部
// @param Tok 当前正在解析的 Token
static Node *equality(Token **Rest, Token *Tok)
{
    Node *node = relational(&Tok, Tok);
    while (true)
    {
        if (equal(Tok, "=="))
        {
            node = newBinary(ND_EQ, node, relational(&Tok, Tok->next));
        }
        else if (equal(Tok, "!="))
        {
            node = newBinary(ND_NE, node, relational(&Tok, Tok->next));
        }
        else
        {
            *Rest = Tok;
            return node;
        }
    }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// @param Rest 用于向上传递仍需要解析的 Token 的首部
// @param Tok 当前正在解析的 Token
static Node *relational(Token **Rest, Token *Tok)
{
    Node *node = add(&Tok, Tok);
    while (true)
    {
        if (equal(Tok, "<"))
        {
            node = newBinary(ND_LT, node, add(&Tok, Tok->next));
        }
        else if (equal(Tok, ">"))
        {
            node = newBinary(ND_LT, add(&Tok, Tok->next), node); // a > b 等价于 b < a
        }
        else if (equal(Tok, "<="))
        {
            node = newBinary(ND_LE, node, add(&Tok, Tok->next));
        }
        else if (equal(Tok, ">="))
        {
            node = newBinary(ND_LE, add(&Tok, Tok->next), node); // a >= b 等价于 b <= a
        }
        else
        {
            *Rest = Tok;
            return node;
        }
    }
}

// add = mul ("+" mul | "-" mul)*
// @param Rest 用于向上传递仍需要解析的 Token 的首部
// @param Tok 当前正在解析的 Token
static Node *add(Token **Rest, Token *Tok)
{
    Node *node = mul(&Tok, Tok);
    while (true)
    {
        if (equal(Tok, "+"))
        {
            node = newBinary(ND_ADD, node, mul(&Tok, Tok->next));
        }
        else if (equal(Tok, "-"))
        {
            node = newBinary(ND_SUB, node, mul(&Tok, Tok->next));
        }
        else
        {
            *Rest = Tok;
            return node;
        }
    }
}

// mul = unary ("*" unary | "/" unary)*
// @param Rest 用于向上传递仍需要解析的 Token 的首部
// @param Tok 当前正在解析的 Token
static Node *mul(Token **Rest, Token *Tok)
{
    Node *node = unary(&Tok, Tok);
    while (true)
    {
        if (equal(Tok, "*"))
        {
            node = newBinary(ND_MUL, node, unary(&Tok, Tok->next));
        }
        else if (equal(Tok, "/"))
        {
            node = newBinary(ND_DIV, node, unary(&Tok, Tok->next));
        }
        else
        {
            *Rest = Tok;
            return node;
        }
    }
}

// unary = ("+" | "-" ) unary | primary
// @param Rest 用于向上传递仍需要解析的 Token 的首部
// @param Tok 当前正在解析的 Token
static Node *unary(Token **Rest, Token *Tok)
{
    if (equal(Tok, "+"))
    {
        return unary(Rest, Tok->next);
    }
    else if (equal(Tok, "-"))
    {
        return newUnary(ND_NEG, unary(Rest, Tok->next));
    }
    return primary(Rest, Tok); // Tok 未进行运算，需要解析首部仍为 Rest
}

// primary = "(" expr ")" | num
// @param Rest 用于向上传递仍需要解析的 Token 的首部
// @param Tok 当前正在解析的 Token
static Node *primary(Token **Rest, Token *Tok)
{
    if (equal(Tok, "("))
    {
        Node *node = expr(&Tok, Tok->next);
        *Rest = skip(Tok, ")");
        return node;
    }
    else if (Tok->kind == TK_NUM)
    {
        Node *node = newNumNode(Tok->Val);
        *Rest = Tok->next;
        return node;
    }
    else
    {
        errorTok(Tok, "expected an expression");
        return NULL;
    }
}

Node *parse(Token *Tok)
{
    Node *Nd = expr(&Tok, Tok);
    if (Tok->kind != TK_EOF)
        errorTok(Tok, "extra token");
    return Nd;
}