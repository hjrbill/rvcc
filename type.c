#include "rvcc.h"

Type *TyInt = &(Type){TY_INT}; // 为 int 类型创建 Type 常（变）量

bool isInteger(Type *Ty)
{
    return Ty->kind == TY_INT;
}

// 创建一个基类为 base 的指针类型
Type *pointerTo(Type *base)
{
    Type *ptr = calloc(1, sizeof(Type));
    ptr->kind = TY_PTR;
    ptr->base = base;
    return ptr;
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
        addType(N);

    switch (node->kind)
    {
    // 将操作符节点的类型设为 节点左部的类型
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_NEG:
    case ND_ASSIGN:
        node->type = node->LHS->type;
        return;

    // 将特殊处理比较操作符节点，其类型设为 int
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_FUNCALL:
    case ND_NUM:
        node->type = TyInt;
        return;

    case ND_VAR:
        node->type = node->Var->type;
        return;

    // 将取址节点的类型设为 指针，其基类为其左部的类型
    case ND_ADDR:
        node->type = pointerTo(node->LHS->type);
        return;

    // 如果解引用指向的是指针，则为指针指向的类型；否则报错
    case ND_DEREF:
        if (node->LHS->type->kind != TY_PTR)
        {
            errorTok(node->Tok, "invalid pointer dereference");
        }
        node->type = node->LHS->type->base;
        return;
    default:
        break;
    }
}