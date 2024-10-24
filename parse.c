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

// program = "{" compoundStmt
// compoundStmt = stmt* "}"
// stmt = "return" expr ";"
//        | "if" "(" expr ")" stmt ("else" stmt)?
//        | "for" "(" exprStmt expr? ";" expr? ")" stmt
//        | "while" "(" expr ")" stmt
//        | "{" compoundStmt
//        | exprStmt
// exprStmt = expr ";"
// expr = assign
// assign = equality ("=" assign)?
// equality = relational ("==" relational | "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// add = mul ("+" mul | "-" mul)*
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-" ) unary | primary
// primary = "(" expr ")" | ident | num
static Node *compoundStmt(Token **Rest, Token *Tok);
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

static Node *newNode(NodeKind kind, Token *Tok)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

// 创建二元运算节点
static Node *newBinary(NodeKind kind, Token *Tok, Node *lhs, Node *rhs)
{
    Node *node = newNode(kind, Tok);
    node->LHS = lhs;
    node->RHS = rhs;
    return node;
}

// 创建一元运算节点
static Node *newUnary(NodeKind kind, Token *Tok, Node *lhs)
{
    Node *node = newNode(kind, Tok);
    node->LHS = lhs;
    return node;
}

// 创建变量节点
static Node *newVarNode(Token *Tok, Obj *var)
{
    Node *node = newNode(ND_VAR, Tok);
    node->Var = var;
    return node;
}

// 创建数字（叶子）节点
static Node *newNumNode(Token *Tok, int Val)
{
    Node *node = newNode(ND_INT, Tok);
    node->Val = Val;
    return node;
}

// compoundStmt = stmt* "}"
// 解析复合语句
static Node *compoundStmt(Token **Rest, Token *T)
{
    Node Head = {};
    Node *Cur = &Head;

    while (!equal(T, "}"))
    {
        Cur->next = stmt(&T, T);
        Cur = Cur->next;
    }
    *Rest = skip(T, "}");

    Node *node = newNode(ND_BLOCK,T);
    node->Body = Head.next; // 代码块节点的 body 存储了该代码块的语句

    return node;
}

// stmt = "return" expr ";"
//        | "if" "(" expr ")" stmt ("else" stmt)?
//        | "for" "(" exprStmt expr? ";" expr? ")" stmt
//        | "while" "(" expr ")" stmt
//        | "{" compoundStmt
//        | exprStmt
static Node *stmt(Token **Rest, Token *T)
{
    if (equal(T, "if"))
    {
        Node *node = newNode(ND_IF,T);

        T = skip(T->next, "(");
        node->Cond = expr(&T, T);
        T = skip(T, ")");
        node->Then = stmt(&T, T);

        if (equal(T, "else"))
        {
            node->Else = stmt(&T, T->next);
        }

        *Rest = T;
        return node;
    }
    else if (equal(T, "for"))
    {
        T = T->next;
        Node *node = newNode(ND_FOR, T);

        T = skip(T, "(");
        node->Init = exprStmt(&T, T);
        if (!equal(T, ";"))
        {
            node->Cond = expr(&T, T);
        }
        T = skip(T, ";");
        if (!equal(T, ")"))
        {
            node->Inc = expr(&T, T);
        }
        T = skip(T, ")");

        node->Then = stmt(Rest, T);
        return node;
    }
    else if (equal(T, "while")) // 处理 while 语句，不与 for 共用是为了处理两种语法不同的报错情况
    {
        T = T->next;
        Node *node = newNode(ND_FOR, T);

        T = skip(T, "(");
        node->Cond = expr(&T, T);
        T = skip(T, ")");

        node->Then = stmt(Rest, T);
        return node;
    }
    else if (equal(T, "return")) // return expr;
    {
        T = T->next;
        Node *node = newUnary(ND_RETURN, T, expr(&T, T));
        *Rest = skip(T, ";");
        return node;
    }
    else if (equal(T, "{"))
    {
        return compoundStmt(Rest, T->next);
    }

    // expr;
    return exprStmt(Rest, T);
}

// exprStmt = expr? ";"
static Node *exprStmt(Token **Rest, Token *Tok)
{
    if (equal(Tok, ";")) // 处理空语句
    {
        *Rest = skip(Tok, ";");
        return newNode(ND_BLOCK, Tok);
    }

    Node *node = newUnary(ND_EXPR_STMT, Tok, expr(&Tok, Tok));
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
        node = newBinary(ND_ASSIGN, Tok, node, assign(&Tok, Tok->next));
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
            node = newBinary(ND_EQ, Tok, node, relational(&Tok, Tok->next));
        }
        else if (equal(Tok, "!="))
        {
            node = newBinary(ND_NE, Tok, node, relational(&Tok, Tok->next));
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
            node = newBinary(ND_LT, Tok, node, add(&Tok, Tok->next));
        }
        else if (equal(Tok, ">"))
        {
            node = newBinary(ND_LT, Tok, add(&Tok, Tok->next), node); // a > b 等价于 b < a
        }
        else if (equal(Tok, "<="))
        {
            node = newBinary(ND_LE, Tok, node, add(&Tok, Tok->next));
        }
        else if (equal(Tok, ">="))
        {
            node = newBinary(ND_LE, Tok, add(&Tok, Tok->next), node); // a >= b 等价于 b <= a
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
            node = newBinary(ND_ADD, Tok, node, mul(&Tok, Tok->next));
        }
        else if (equal(Tok, "-"))
        {
            node = newBinary(ND_SUB, Tok, node, mul(&Tok, Tok->next));
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
            node = newBinary(ND_MUL,Tok, node, unary(&Tok, Tok->next));
        }
        else if (equal(Tok, "/"))
        {
            node = newBinary(ND_DIV,Tok, node, unary(&Tok, Tok->next));
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
        return newUnary(ND_NEG, Tok, unary(Rest, Tok->next));
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
        Node *node = newVarNode(Tok, var);
        *Rest = Tok->next;
        return node;
    }
    else if (Tok->kind == TK_NUM)
    {
        Node *node = newNumNode(Tok, Tok->Val);
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
// program = "{" compoundStmt
Func *parse(Token *Tok)
{
    Tok = skip(Tok, "{"); // 跳过 " { "

    Func *fn = calloc(1, sizeof(Func));
    fn->body = compoundStmt(&Tok, Tok);
    fn->locals = Locals;

    return fn;
}