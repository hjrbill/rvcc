#include "rvcc.h"

static int Depth; // 栈深度

// 将 a0 压入栈
static void push(void)
{
    printf("  addi sp, sp, -8\n");
    // sd rd, rs1, 将寄存器 rd 中的值存储到 rsq 上
    printf("  sd a0, 0(sp)\n");
    Depth++;
}

// 从栈中取出元素并放到 Reg
static void pop(char *Reg)
{
    // ld rd, rs1，从内存中加载一个 32 位或 64 位的 rs1(操作数) 到寄存器 rd 中
    printf("  ld %s, 0(sp)\n", Reg);
    // addi rd, rs1, imm 表示 rd = rs1 + imm
    printf("  addi sp, sp, 8\n");
    Depth--;
}

static void genExpr(Node *node)
{
    if (node->kind == ND_INT) // 是叶子节点
    {
        // li 为 addi 别名指令，加载一个立即数到寄存器中
        printf("  li a0, %d\n", node->Val);
        return;
    }

    if (node->kind == ND_NEG)
    {
        genExpr(node->LHS);
        // neg a0, a0 是 sub a0, x0, a0 的别名，即 a0=0-a0
        printf("  neg a0, a0\n");
        return;
    }

    // 由于优先级问题，先遍历右子树
    genExpr(node->RHS);
    push();
    genExpr(node->LHS);
    pop("a1"); // 取回右子树结果

    switch (node->kind)
    {
    case ND_EQ:
        // xor a, b, c，将 b 异或 c 的结果放入 a
        // 如果相同，异或后，a0=0，否则 a0=1
        printf("  xor a0, a0, a1\n");
        // seqz a, b 判断 b 是否等于 0 并将结果放入 a
        printf("  seqz a0, a0\n");
        return;
    case ND_NE:
        // a0=a0^a1，异或指令
        // 异或后如果相同，a0=1，否则 a0=0
        printf("  xor a0, a0, a1\n");
        // snez a, b 判断 b 是否不等于 0 并将结果放入 a
        printf("  snez a0, a0\n");
        return;
    case ND_LT:
        // slt a, b, c，将 b < c 的结果放入 a
        printf("  slt a0, a0, a1\n");
        return;
    case ND_LE:
        printf("  slt a0, a1, a0\n");
        // xori a, b, (立即数)，将 b 异或 (立即数) 的结果放入 a
        printf("  xori a0, a0, 1\n");
        return;
    case ND_ADD:
        printf("  add a0, a0, a1\n");
        return;
    case ND_SUB:
        printf("  sub a0, a0, a1\n");
        return;
    case ND_MUL:
        printf("  mul a0, a0,  a1\n");
        return;
    case ND_DIV:
        printf("  div a0, a0, a1\n");
        return;
    default:
        error("invalid expression %d", node->kind);
        return;
    }
}

void codegen(Node *Nd)
{
    printf("  .globl main\n");
    printf("main:\n");
    genExpr(Nd);
    printf("  ret\n");
    assert(Depth == 0);
}