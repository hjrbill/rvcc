#include "rvcc.h"

Obj *Locals; // 存储当前解析的函数的局部变量

static Obj *FindVarByName(Token *Tok)
{
    for (Obj *p = Locals; p; p = p->next)
    {
        if (strlen(p->name) == Tok->Len && !strncmp(p->name, Tok->Loc, Tok->Len))
        {
            return p;
        }
    }
    return NULL;
}

static Obj *newVar(char *name)
{
    Obj *var = calloc(1, sizeof(Obj));
    var->name = name;

    // 将新变量插入到 Locals 的头部
    var->next = Locals;
    Locals = var;

    return var;
}

// program = stmt*
// stmt = exprStmt | "return" expr ";"
// exprStmt = expr ";"
// expr = assign
// assign = equality ("=" assign)?
// equality = relational ("==" relational | "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add = mul ("+" mul | "-" mul)*
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-" ) unary | primary
// primary = "(" add ")" | num
static Node *stmt(Token **Rest, Token *Tok);
static Node *exprStmt(Token **Rest, Token *Tok);
static Node *expr(Token **Rest, Token *Tok);
static Node *assign(Token **Rest, Token *Tok);
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

// 创建变量节点
static Node *newVarNode(Obj *var)
{
    Node *node = newNode(ND_VAR);
    node->Var = var;
    return node;
}

// 创建数字（叶子）节点
static Node *newNumNode(int Val)
{
    Node *node = newNode(ND_INT);
    node->Val = Val;
    return node;
}

// stmt = exprStmt | "return" expr ";"
static Node *stmt(Token **Rest, Token *Tok)
{
    // return expr;
    if (equal(Tok, "return"))
    {
        Node *node = newUnary(ND_RETURN, expr(&Tok, Tok->next));
        *Rest = skip(Tok, ";");
        return node;
    }

    // expr;
    return exprStmt(Rest, Tok);
}

// exprStmt = expr ";"
static Node *exprStmt(Token **Rest, Token *Tok)
{
    Node *node = newUnary(ND_EXPR_STMT, expr(&Tok, Tok));
    *Rest = skip(Tok, ";");
    return node;
}

// expr = assign
// @param Rest 用于向上传递仍需要解析的 Token 的首部
// @param Tok 当前正在解析的 Token
static Node *expr(Token **Rest, Token *Tok)
{
    return assign(Rest, Tok);
}

// assign = equality ("=" assign)?
// @param Rest 用于向上传递仍需要解析的 Token 的首部
// @param Tok 当前正在解析的 Token
static Node *assign(Token **Rest, Token *Tok)
{
    Node *node = equality(&Tok, Tok);
    if (equal(Tok, "=")) // 处理递归赋值，如："a=b=1;"
    {
        node = newBinary(ND_ASSIGN, node, assign(&Tok, Tok->next));
    }
    *Rest = Tok;
    return node;
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

// primary = "(" expr ")" | num | ident
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
    else if (Tok->kind == TK_IDENT)
    {
        Obj *var = FindVarByName(Tok);
        if (var == NULL)
        {
            var = newVar(strndup(Tok->Loc, Tok->Len)); // strndup() 复制字符串的指定长度
        }
        Node *node = newVarNode(var);
        *Rest = Tok->next;
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

// 语法解析入口函数
// program = stmt*
Func *parse(Token *Tok)
{
    Node head = {};
    Node *cur = &head;
    while (Tok->kind != TK_EOF)
    {
        cur->next = stmt(&Tok, Tok);
        cur = cur->next;
    }

    Func *fn = calloc(1, sizeof(Func));
    fn->locals = Locals;
    fn->body = head.next;

    return fn;
}