#include "rvcc.h"

// 存储当前解析种的变量
Obj *Locals;  // 局部变量 (局部函数/嵌套函数)
Obj *Globals; // 全局变量（全局函数）

static Obj *FindVarByName(Token *Tok)
{
    for (Obj *p = Locals; p; p = p->next)
    {
        if (strlen(p->name) == Tok->Len && !strncmp(p->name, Tok->Loc, Tok->Len))
        {
            return p;
        }
    }

    for (Obj *p = Globals; p; p = p->next)
    {
        if (strlen(p->name) == Tok->Len && !strncmp(p->name, Tok->Loc, Tok->Len))
        {
            return p;
        }
    }
    return NULL;
}

// 获取变量名
static char *getIdent(Token *Tok)
{
    if (Tok->kind != TK_IDENT)
    {
        errorTok(Tok, "expected an identifier");
    }

    return strndup(Tok->Loc, Tok->Len);
}

static int getNum(Token *Tok)
{
    if (Tok->kind != TK_NUM)
        errorTok(Tok, "expected a number");
    return Tok->Val;
}

static Obj *newVar(char *name, Type *type)
{
    Obj *var = calloc(1, sizeof(Obj));
    var->name = name;
    var->type = type;

    return var;
}

static Obj *newLocalVar(char *name, Type *type)
{
    Obj *var = newVar(name, type);
    var->isLocal = true;

    // 将新变量插入到 Locals 的头部
    var->next = Locals;
    Locals = var;

    return var;
}

static Obj *newGlobalVar(char *name, Type *type)
{
    Obj *var = newVar(name, type);

    // 将新变量插入到 Globals 的头部
    var->next = Globals;
    Globals = var;

    return var;
}

static void createParamVars(Type *Param)
{
    if (Param)
    {
        // 先将最底部的加入 Locals 中，之后逐个加入到顶部，保持顺序不变
        createParamVars(Param->next);
        // 添加到 Locals 中
        newLocalVar(getIdent(Param->name), Param);
    }
}

// program = (functionDefinition | globalVariable)*
// functionDefinition = declspec declarator "{" compoundStmt*
// globalVariable = declspec ( declarator ",")* ";"
// declspec = "int"
// declarator = "*"* ident typeSuffix
// typeSuffix = "(" funcParams | "[" num "]" typeSuffix | ε
// funcParams = (param ("," param)*)? ")"
// param = declspec declarator
// compoundStmt = (declaration | stmt)* "}"
// declaration = declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
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
// unary = ("+" | "-" | "*" | "&") unary | postfix
// postfix = primary ("[" expr "]")*
// primary = "(" expr ")" | "sizeof" unary | ident | Funcall | num
// Funcall = ident "(" (assign ("," assign)*)? ")"
static Token *function(Token *Tok, Type *declspec);
static Token *globalVariable(Token *Tok, Type *declspec);
static Type *declspec(Token **Rest, Token *Tok);
static Type *declarator(Token **Rest, Token *Tok, Type *Ty);
static Type *typeSuffix(Token **Rest, Token *Tok, Type *Ty);
static Type *funcParams(Token **Rest, Token *Tok, Type *Ty);
static Node *compoundStmt(Token **Rest, Token *Tok);
static Node *declaration(Token **Rest, Token *Tok);
static Node *stmt(Token **Rest, Token *Tok);
static Node *exprStmt(Token **Rest, Token *Tok);
static Node *expr(Token **Rest, Token *Tok);
static Node *assign(Token **Rest, Token *Tok);
static Node *equality(Token **Rest, Token *Tok);
static Node *relational(Token **Rest, Token *Tok);
static Node *add(Token **Rest, Token *Tok);
static Node *mul(Token **Rest, Token *Tok);
static Node *unary(Token **Rest, Token *Tok);
static Node *postfix(Token **Rest, Token *Tok);
static Node *primary(Token **Rest, Token *Tok);
static Node *Funcall(Token **Rest, Token *Tok);

static Node *newNode(NodeKind kind, Token *Tok)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

// 创建数字（叶子）节点
static Node *newNumNode(Token *Tok, int Val)
{
    Node *node = newNode(ND_NUM, Tok);
    node->Val = Val;
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

// 特殊处理加法运算节点，以处理各类型的转换问题
static Node *newAddBinary(Token *Tok, Node *LHS, Node *RHS)
{
    // 提前为左右部添加类型
    addType(LHS);
    addType(RHS);

    // 拒绝处理指针 + 指针
    if (LHS->type->base && RHS->type->base)
    {
        errorTok(Tok, "invalid operands");
    }

    // num + num
    if (isInteger(LHS->type) && isInteger(RHS->type))
    {
        return newBinary(ND_ADD, Tok, LHS, RHS);
    }

    // num+ptr
    if (isInteger(LHS->type) && RHS->type->base)
    {
        Node *Tmp = LHS;
        LHS = RHS;
        RHS = Tmp;
    }
    // ptr + num
    RHS = newBinary(ND_MUL, Tok, RHS, newNumNode(Tok, LHS->type->base->size)); // ptr + 1 == ptr + 1*sizeof(ptr->base)
    return newBinary(ND_ADD, Tok, LHS, RHS);
}

// 特殊处理减法运算节点，以处理各类型的转换问题
static Node *newSubBinary(Token *Tok, Node *LHS, Node *RHS)
{
    // 提前为左右部添加类型
    addType(LHS);
    addType(RHS);

    // 指针 - 指针
    if (LHS->type->base && RHS->type->base)
    {
        Node *node = newBinary(ND_SUB, Tok, LHS, RHS);
        node->type = TyInt;
        return newBinary(ND_DIV, Tok, node, newNumNode(Tok, LHS->type->size));
    }

    // num - num
    if (isInteger(LHS->type) && isInteger(RHS->type))
    {
        return newBinary(ND_SUB, Tok, LHS, RHS);
    }

    // ptr - num
    if (LHS->type->base && isInteger(RHS->type))
    {
        RHS = newBinary(ND_MUL, Tok, RHS, newNumNode(Tok, LHS->type->size));
        return newBinary(ND_SUB, Tok, LHS, RHS);
    }

    errorTok(Tok, "invalid operands");
    return NULL;
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

// 区分 函数还是全局变量
static bool isFunction(Token *Tok)
{
    if (equal(Tok, ";"))
    {
        return false;
    }

    // 虚设变量，用于调用 declarator 以确定是否为函数
    Type Dummy = {};
    Type *Ty = declarator(&Tok, Tok, &Dummy);
    return Ty->kind == TY_FUNC;
}

// 语法解析入口函数
// program = (functionDefinition | globalVariable)*
Obj *parse(Token *Tok)
{
    Globals = NULL;

    while (Tok->kind != TK_EOF)
    {
        Type *baseType = declspec(&Tok, Tok);

        if (isFunction(Tok))
        {
            Tok = function(Tok, baseType);
        }
        else{
            Tok = globalVariable(Tok, baseType);
        }
    }
    return Globals;
}

// globalVariable = declspec ( declarator ",")* ";"
static Token *globalVariable(Token *Tok, Type *declspec)
{
    bool isFirst = true;

    while (!consume(&Tok,Tok, ";"))
    {
        if (!isFirst)
        {
            Tok = skip(Tok, ",");
        }
        else
        {
            isFirst = false;
        }

        Type *Ty = declarator(&Tok, Tok, declspec);
        Obj *obj = newGlobalVar(getIdent(Ty->name), Ty);
    }
    return Tok;
}

// functionDefinition = declspec declarator "{" compoundStmt*
static Token *function(Token *Tok, Type *declspec)
{
    Type *Ty = declarator(&Tok, Tok, declspec);
    Obj *fn = newGlobalVar(getIdent(Ty->name), Ty); // 全局函数是一种特殊的全局变量
    fn->isFunction = true;

    // 清空上一个函数的 Locals
    Locals = NULL;
    // 函数参数
    createParamVars(Ty->Params);
    fn->Params = Locals;

    Tok = skip(Tok, "{");
    // 函数体存储语句的 AST，Locals 存储变量
    fn->body = compoundStmt(&Tok, Tok);
    fn->locals = Locals;

    return Tok;
}

// declspec = "int"
static Type *declspec(Token **Rest, Token *Tok)
{
    if (equal(Tok, "int"))
    {
        *Rest = Tok->next;
        return TyInt;
    }
    else
    {
        errorTok(Tok, "undefined type");
    }
}

// declarator = "*"* ident typeSuffix
static Type *declarator(Token **Rest, Token *Tok, Type *type)
{
    // "*"*
    while (consume(&Tok, Tok, "*"))
    {
        type = pointerTo(type);
    }

    if (Tok->kind != TK_IDENT)
    {
        errorTok(Tok, "expected a variable name");
    }

    type = typeSuffix(Rest, Tok->next, type);
    // ident
    type->name = Tok; // 变量名或函数名
    return type;
}

// typeSuffix = "(" funcParams | "[" num "]" typeSuffix | ε
static Type *typeSuffix(Token **Rest, Token *Tok, Type *type)
{
    // ("(" funcParams? ")")?
    if (equal(Tok, "("))
    {
        return funcParams(Rest, Tok->next, type);
    }
    else if (equal(Tok, "["))
    {
        int size = getNum(Tok->next);
        Tok = skip(Tok->next->next, "]");
        type = typeSuffix(Rest, Tok, type);
        return arrayOf(type, size);
    }
    *Rest = Tok;
    return type;
}

// funcParams = (param ("," param)*)? ")"
// param = declspec declarator
static Type *funcParams(Token **Rest, Token *Tok, Type *Ty)
{
    Type Head = {};
    Type *Cur = &Head;

    while (!equal(Tok, ")"))
    {
        // funcParams = param ("," param)*
        // param = declspec declarator
        if (Cur != &Head)
            Tok = skip(Tok, ",");
        Type *BaseTy = declspec(&Tok, Tok);
        Type *DeclarTy = declarator(&Tok, Tok, BaseTy);
        Cur->next = copyType(DeclarTy); // 将类型复制到形参链表一份
        Cur = Cur->next;
    }

    Ty = funcType(Ty);
    Ty->Params = Head.next; // 传递形参
    *Rest = Tok->next;
    return Ty;
}

// compoundStmt = (declaration | stmt)* "}"
// 解析复合语句
static Node *compoundStmt(Token **Rest, Token *T)
{
    Node Head = {};
    Node *Cur = &Head;

    while (!equal(T, "}"))
    {
        if (equal(T, "int")) // declaration
        {
            Cur->next = declaration(&T, T);
        }
        else // stmt
        {
            Cur->next = stmt(&T, T);
        }
        Cur = Cur->next;
        addType(Cur); // 为节点添加类型信息
    }
    *Rest = skip(T, "}");

    Node *node = newNode(ND_BLOCK, T);
    node->Body = Head.next; // 代码块节点的 body 存储了该代码块的语句

    return node;
}

// declaration = declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
static Node *declaration(Token **Rest, Token *Tok)
{
    // declspec
    Type *baseType = declspec(&Tok, Tok); // 声明的 基础类型

    Node head = {};
    Node *Cur = &head;

    int i = 0; // 处理连续变量申明

    // (declarator ("=" expr)? ("," declarator ("=" expr)?)*)?
    while (!equal(Tok, ";"))
    {
        // 第 1 个变量不必匹配 ","
        if (i++ > 0)
        {
            Tok = skip(Tok, ",");
        }

        Type *Ty = declarator(&Tok, Tok, baseType);
        Obj *Var = newLocalVar(getIdent(Ty->name), Ty);

        // 如果不存在"="则为变量声明，不需要生成节点，已经存储在 Locals 中了
        if (!equal(Tok, "="))
        {
            continue;
        }

        Node *LHS = newVarNode(Ty->name, Var);
        Node *RHS = assign(&Tok, Tok->next);
        Node *node = newBinary(ND_ASSIGN, Tok, LHS, RHS);

        Cur->next = newUnary(ND_EXPR_STMT, Tok, node);
        Cur = Cur->next;
    }

    // 将所有表达式语句，存放在代码块中
    Node *node = newNode(ND_BLOCK, Tok);
    node->Body = head.next;
    *Rest = Tok->next;
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
        Node *node = newNode(ND_IF, T);

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
            node = newAddBinary(Tok, node, mul(&Tok, Tok->next));
        }
        else if (equal(Tok, "-"))
        {
            node = newSubBinary(Tok, node, mul(&Tok, Tok->next));
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
            node = newBinary(ND_MUL, Tok, node, unary(&Tok, Tok->next));
        }
        else if (equal(Tok, "/"))
        {
            node = newBinary(ND_DIV, Tok, node, unary(&Tok, Tok->next));
        }
        else
        {
            *Rest = Tok;
            return node;
        }
    }
}

// unary = ("+" | "-" | "*" | "&") unary | postfix
// @param Rest 用于向上传递仍需要解析的 Token 的首部
// @param Tok 当前正在解析的 Token
static Node *unary(Token **Rest, Token *T)
{
    if (equal(T, "+"))
    {
        return unary(Rest, T->next);
    }
    else if (equal(T, "-"))
    {
        return newUnary(ND_NEG, T, unary(Rest, T->next));
    }
    else if (equal(T, "*"))
    {
        return newUnary(ND_DEREF, T, unary(Rest, T->next));
    }
    else if (equal(T, "&"))
    {
        return newUnary(ND_ADDR, T, unary(Rest, T->next));
    }
    return postfix(Rest, T); // Tok 未进行运算，需要解析首部仍为 Rest
}

// postfix = primary ("[" expr "]")*
static Node *postfix(Token **Rest, Token *Tok)
{
    Node *node = primary(&Tok, Tok);

    while (equal(Tok, "["))
    {
        Token *start = Tok->next;
        Node *Idx = expr(&Tok, Tok->next);
        Tok = skip(Tok, "]");
        node = newUnary(ND_DEREF, start, newAddBinary(start, node, Idx));
    }

    *Rest = Tok;
    return node;
}

// primary = "(" expr ")" | "sizeof" unary | ident | Funcall | num
// args = "(" ")"
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
    else if (equal(Tok, "sizeof"))
    {
        Node *node = unary(Rest, Tok->next);
        addType(node);
        return newNumNode(Tok, node->type->size);
    }
    else if (Tok->kind == TK_IDENT)
    {
        if (equal(Tok->next, "("))
        {
            return Funcall(Rest, Tok);
        }

        Obj *var = FindVarByName(Tok);
        if (var == NULL)
        {
            errorTok(Tok, "undefined variable");
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

// Funcall = ident "(" (assign ("," assign)*)? ")"
static Node *Funcall(Token **Rest, Token *Tok)
{
    Node *node = newNode(ND_FUNCALL, Tok);
    node->FuncName = strndup(Tok->Loc, Tok->Len);

    Tok = Tok->next->next;

    Node head = {};
    Node *Cur = &head;
    int i = 0;

    while (!equal(Tok, ")"))
    {
        if (i++ > 0)
        {
            Tok = skip(Tok, ",");
        }
        if (i > 6)
        {
            errorTok(Tok, "funcation has too many arguments");
        }
        Cur->next = assign(&Tok, Tok);
        Cur = Cur->next;
    }
    node->Args = head.next;

    *Rest = Tok->next;
    return node;
}
