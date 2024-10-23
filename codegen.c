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

// 计算给定节点的绝对地址，如果报错，说明节点不在内存中
static void getAddr(Node *node)
{
    if (node->kind == ND_VAR)
    {
        // 取出变量相对于 fp 的偏移量
        printf("  addi a0, fp, %d\n", node->Var->offset);
        return;
    }
    error("not an lvalue");
}

static void genExpr(Node *node)
{
    switch (node->kind)
    {
    case ND_INT: // 是整型
        // li 为 addi 别名指令，加载一个立即数到寄存器中
        printf("  li a0, %d\n", node->Val);
        return;
    case ND_NEG: // 是取反
        genExpr(node->LHS);
        // neg a0, a0 是 sub a0, x0, a0 的别名，即 a0=0-a0
        printf("  neg a0, a0\n");
        return;
    case ND_VAR: // 是变量
        // 计算出变量的地址，然后存入 a0
        getAddr(node);
        // 访问 a0 地址中存储的数据，存入到 a0 当中
        printf("  ld a0, 0(a0)\n");
        return;
    case ND_ASSIGN:         // 是赋值
        getAddr(node->LHS); // 左边为被赋值的地址
        push();
        genExpr(node->RHS); // 右边为赋予的值
        pop("a1");
        printf("  sd a0, 0(a1)\n");
        return;
    default:
        break;
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

static void genStmt(Node *node)
{
    switch (node->kind)
    {
    case ND_RETURN:
        genExpr(node->LHS);
        // 无条件跳转语句，跳转到.L.return 段
        // j offset 是 jal x0, offset 的别名指令
        printf("  j .L.return\n");
        return;
    case ND_EXPR_STMT:
        genExpr(node->LHS);
        return;
    default:
        break;
    }

    error("invalid statement %d", node->kind);
}

// 将 N 对齐到 Align 的整数倍
static int alignTo(int N, int Align)
{
    return (N + Align - 1) / Align * Align;
}

static void assignLVarOffsets(Func *Prog)
{
    int offset = 0;
    for (Obj *var = Prog->locals; var; var = var->next)
    {
        offset += 8;
        var->offset = -offset;
    }
    Prog->stackSize = alignTo(offset, 16); // 将栈对齐到 16 字节（内存对齐），优化处理器访问
}

void codegen(Func *fn)
{
    assignLVarOffsets(fn);
    printf("  .globl main\n");
    printf("main:\n");

    // Prologue, 预处理
    // 将 fp 压入栈中，保存 fp 的值
    printf("  addi sp, sp, -8\n");
    printf("  sd fp, 0(sp)\n");
    // mv a, b. 将寄存器 b 中的值存储到寄存器 a 中
    printf("  mv fp, sp\n"); // 将 sp 写入 fp
    // 26 个字母*8 字节=208 字节，栈腾出 208 字节的空间
    printf("  addi sp, sp, %d\n", fn->stackSize);

    for (Node *Nd = fn->body; Nd; Nd = Nd->next)
    {
        genStmt(Nd);
        assert(Depth == 0);
    }

    // Epilogue，后处理
    printf(".L.return:\n"); // 输出 return 段标签

    printf("  mv sp, fp\n");
    printf("  ld fp, 0(sp)\n");   // 将栈顶元素（fp）弹出并存储到 fp
    printf("  addi sp, sp, 8\n"); // 移动 sp 到初始态，消除 fp 的影响

    printf("  ret\n");
}