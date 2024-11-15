#include "rvcc.h"

// (Type){...}构造了一个复合字面量，相当于 Type 的匿名变量
Type *TyChar = &(Type){TY_CHAR, 1, 1};
Type *TyShort = &(Type){TY_SHORT, 2, 2};
Type *TyInt = &(Type){TY_INT, 4, 4};
Type *TyLong = &(Type){TY_LONG, 8, 8};

static Type *newType(TypeKind kind, int size, int align)
{
    Type *type = calloc(1, sizeof(Type));
    type->kind = kind;
    type->size = size;
    type->align = align;
    return type;
}

bool isInteger(Type *Ty)
{
    TypeKind K = Ty->kind;
    return K == TY_CHAR || K == TY_SHORT || K == TY_INT || K == TY_LONG;
}

// 创建一个基类为 base 的指针类型
Type *pointerTo(Type *base)
{
    Type *ptr = newType(TY_PTR, 8, 8);
    ptr->base = base;
    return ptr;
}

// 创建一个返回类型为 ReturnTy 的函数类型
Type *funcType(Type *ReturnTy)
{
    Type *Ty = calloc(1, sizeof(Type));
    Ty->kind = TY_FUNC;
    Ty->ReturnTy = ReturnTy;
    return Ty;
}

// 创建一个数组类型
Type *arrayOf(Type *Base, int Len)
{
    Type *Ty = newType(TY_ARRAY, Base->size * Len, Base->align);
    Ty->base = Base;
    Ty->ArrayLen = Len;
    return Ty;
}

// 复制类型
Type *copyType(Type *Ty)
{
    Type *Ret = calloc(1, sizeof(Type));
    *Ret = *Ty;
    return Ret;
}

// 为节点内的所有节点添加类型
void addType(Node *node)
{
    if (!node || node->type)
    {
        return;
    }

    // 递归访问所有节点以增加类型
    addType(node->LHS);
    addType(node->RHS);
    addType(node->Cond);
    addType(node->Then);
    addType(node->Else);
    addType(node->Init);
    addType(node->Inc);
    // 访问链表内的所有节点以增加类型
    for (Node *N = node->Body; N; N = N->next)
    {
        addType(N);
    }
    // 访问链表内的所有参数节点以增加类型
    for (Node *N = node->Args; N; N = N->next)
    {
        addType(N);
    }

    switch (node->kind)
    {
    // 将操作符节点的类型设为 节点左部的类型
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_NEG:
        node->type = node->LHS->type;
        return;
    case ND_ASSIGN:
        if (node->LHS->type->kind == TY_ARRAY) // 赋值操作符左部必须不是数组
        {
            errorTok(node->LHS->Tok, "not an lvalue");
        }
        node->type = node->LHS->type;
        return;
    // 将节点类型设为其指向成员的类型
    case ND_MEMBER:
        node->type = node->Mem->type;
        return;
    // 将特殊处理比较操作符节点，其类型设为 long
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_FUNCALL:
    case ND_NUM:
        node->type = TyLong;
        return;

    case ND_VAR:
        node->type = node->Var->type;
        return;

    // 将取址节点的类型设为 指针，其基类为其左部的类型
    case ND_ADDR:
        Type *Ty = node->LHS->type;
        // 左部如果是数组，则为指向数组基类的指针
        if (Ty->kind == TY_ARRAY)
            node->type = pointerTo(Ty->base);
        else
            node->type = pointerTo(Ty);
        return;

    // 如果解引用指向的是指针，则为指针指向的类型；否则报错
    case ND_DEREF:
        if (!node->LHS->type->base) // 如果不存在基类，则不可能能解引用
        {
            errorTok(node->Tok, "invalid pointer dereference");
        }
        node->type = node->LHS->type->base;
        return;
        // 节点类型为 最后的表达式语句的类型
    case ND_STMT_EXPR:
        if (node->Body)
        {
            Node *stmt = node->Body;
            while (stmt->next)
            {
                stmt = stmt->next;
            }
            if (stmt->kind == ND_EXPR_STMT)
            {
                node->type = stmt->LHS->type;
                return;
            }
        }
        errorTok(node->Tok, "statement expression returning void is not supported");
        return;
        // 将节点类型设为 右部的类型
    case ND_COMMA:
        node->type = node->RHS->type;
        return;
    default:
        break;
    }
}