#include "rvcc.h"

// 栈深度
static int Depth;
// 用于存储函数参数的寄存器
static char *ArgReg[] = {"a0", "a1", "a2", "a3", "a4", "a5"};
// 当前的函数
static Func *CurrentFn;

static void genExpr(Node *Nd);

static int Count(void)
{
    static int cnt = 0;
    return cnt++;
}

// 将 a0 压入栈
static void push(void)
{
    printf("  # 压栈，将 a0 的值存入栈顶\n");
    printf("  addi sp, sp, -8\n");
    // sd rd, rs1, 将寄存器 rd 中的值存储到 rsq 上
    printf("  sd a0, 0(sp)\n");
    Depth++;
}

// 从栈中取出元素并放到 Reg
static void pop(char *Reg)
{
    printf("  # 弹栈，将栈顶的值存入%s\n", Reg);
    // ld rd, rs1，从内存中加载一个 32 位或 64 位的 rs1(操作数) 到寄存器 rd 中
    printf("  ld %s, 0(sp)\n", Reg);
    // addi rd, rs1, imm 表示 rd = rs1 + imm
    printf("  addi sp, sp, 8\n");
    Depth--;
}

// 计算给定节点的绝对地址，如果报错，说明节点不在内存中
static void getAddr(Node *node)
{
    switch (node->kind)
    {
    case ND_VAR:
        printf("  # 获取变量%s，栈内地址为%d(fp)\n", node->Var->name,
               node->Var->offset);
        // 取出变量相对于 fp 的偏移量
        printf("  addi a0, fp, %d\n", node->Var->offset);
        return;
    case ND_DEREF:
        genExpr(node->LHS);
        return;
    default:
        errorTok(node->Tok, "not an lvalue");
        return;
    }
}

static void genExpr(Node *node)
{
    switch (node->kind)
    {
    case ND_VAR: // 是变量
        // 计算出变量的地址，然后存入 a0
        getAddr(node);
        // 访问 a0 地址中存储的数据，存入到 a0 当中
        printf("  # 读取 a0 中存放的地址，得到的值存入 a0\n");
        printf("  ld a0, 0(a0)\n");
        return;
    case ND_DEREF:
        genExpr(node->LHS);
        printf("  # 读取 a0 中存放的地址，得到的值存入 a0\n");
        printf("  ld a0, 0(a0)\n");
        return;
    case ND_ADDR:
        getAddr(node->LHS);
        return;
    case ND_NEG: // 是取反
        genExpr(node->LHS);
        printf("  # 对 a0 值进行取反\n");
        // neg a0, a0 是 sub a0, x0, a0 的别名，即 a0=0-a0
        printf("  neg a0, a0\n");
        return;
    case ND_ASSIGN:         // 是赋值
        getAddr(node->LHS); // 左边为被赋值的地址
        push();
        genExpr(node->RHS); // 右边为赋予的值
        pop("a1");
        printf("  # 将 a0 的值，写入到 a1 中存放的地址\n");
        printf("  sd a0, 0(a1)\n");
        return;
    case ND_NUM: // 是整型
        printf("  # 将%d加载到 a0 中\n", node->Val);
        // li 为 addi 别名指令，加载一个立即数到寄存器中
        printf("  li a0, %d\n", node->Val);
        return;
    case ND_FUNCALL:
    {
        int argsCnt = 0;
        // 计算所有参数的值，正向压栈
        for (Node *Arg = node->Args; Arg; Arg = Arg->next)
        {
            genExpr(Arg);
            push();
            argsCnt++;
        }
        // 反向弹栈，a0->参数 1，a1->参数 2……
        for (int i = argsCnt - 1; i >= 0; i--)
        {
            pop(ArgReg[i]);
        }

        printf("  # 调用%s函数\n", node->FuncName);
        printf("  call %s\n", node->FuncName); // 调用函数
        return;
    }
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
        printf("  # 判断是否 a0%sa1\n", node->kind == ND_EQ ? "=" : "≠");
        printf("  xor a0, a0, a1\n");
        // snez a, b 判断 b 是否不等于 0 并将结果放入 a
        printf("  snez a0, a0\n");
        return;
    case ND_LT:
        printf("  # 判断 a0<a1\n");
        // slt a, b, c，将 b < c 的结果放入 a
        printf("  slt a0, a0, a1\n");
        return;
    case ND_LE:
        printf("  # 判断是否 a0≤a1\n");
        printf("  slt a0, a1, a0\n");
        // xori a, b, (立即数)，将 b 异或 (立即数) 的结果放入 a
        printf("  xori a0, a0, 1\n");
        return;
    case ND_ADD:
        printf("  # a0+a1，结果写入 a0\n");
        printf("  add a0, a0, a1\n");
        return;
    case ND_SUB:
        printf("  # a0-a1，结果写入 a0\n");
        printf("  sub a0, a0, a1\n");
        return;
    case ND_MUL:
        printf("  # a0*a1，结果写入 a0\n");
        printf("  mul a0, a0,  a1\n");
        return;
    case ND_DIV:
        printf("  # a0/a1，结果写入a0\n");
        printf("  div a0, a0, a1\n");
        return;
    default:
        errorTok(node->Tok, "invalid expression");
        return;
    }
}

static void genStmt(Node *node)
{
    switch (node->kind)
    {
    case ND_EXPR_STMT:
        genExpr(node->LHS);
        return;
    case ND_IF:
    {
        int cnt = Count(); // 为每个 段语句（如：if,for）生成其编号，以处理 else 的跳转标记
        printf("\n# =====分支语句%d==============\n", cnt);

        printf("\n# Cond 表达式%d\n", cnt);
        genExpr(node->Cond);
        printf("  # 若 a0 为 0，则跳转到分支%d的.L.else.%d段\n", cnt, cnt);
        printf("  beqz a0, .L.else.%d\n", cnt); // 判断条件是否不成立（a0=0），条件不成立跳转到 .L.else. 标签
        printf("\n# Then 语句%d\n", cnt);
        genStmt(node->Then); // 条件成立，执行 then 语句
        printf("  # 跳转到分支%d的.L.end.%d段\n", cnt, cnt);
        printf("j .L.end.%d\n", cnt); // 执行完 then 语句后跳转到 .L.end. 标签

        printf("\n# Else 语句%d\n", cnt);
        printf("# 分支%d的.L.else.%d段标签\n", cnt, cnt);
        printf(".L.else.%d:\n", cnt); // else 标记
        if (node->Else)               // 存在 else 语句
        {
            genStmt(node->Else);
        }

        printf("\n# 分支%d的.L.end.%d段标签\n", cnt, cnt);
        printf(".L.end.%d:\n", cnt); // if 语句结束
        return;
    }
    case ND_FOR:
    {
        int cnt = Count();

        printf("\n# =====循环语句%d===============\n", cnt);
        if (node->Init)
        {
            printf("\n# Init 语句%d\n", cnt);
            genStmt(node->Init); // 初始化语句
        }

        printf("\n# 循环%d的.L.begin.%d段标签\n", cnt, cnt);
        printf(".L.begin.%d:\n", cnt);

        printf("# Cond 表达式%d\n", cnt);
        if (node->Cond) // 存在条件语句
        {
            genExpr(node->Cond);
            printf("  # 若 a0 为 0，则跳转到循环%d的.L.end.%d段\n", cnt, cnt);
            printf(" beqz a0, .L.end.%d\n", cnt);
        }

        printf("\n# Then 语句%d\n", cnt);
        genStmt(node->Then); // 执行循环体

        if (node->Inc) // 存在递增语句
        {
            printf("\n# Inc 语句%d\n", cnt);
            genExpr(node->Inc);
        }

        printf("  # 跳转到循环%d的.L.begin.%d段\n", cnt, cnt);
        printf("  j .L.begin.%d\n", cnt);
        printf("\n# 循环%d的.L.end.%d段标签\n", cnt, cnt);
        printf(".L.end.%d:\n", cnt);
        return;
    }
    case ND_RETURN:
        printf("# 返回语句\n");
        genExpr(node->LHS);
        printf("  # 跳转到.L.return.%s段\n", CurrentFn->name);
        // 无条件跳转语句，跳转到.L.return 段
        // j offset 是 jal x0, offset 的别名指令
        printf("  j .L.return.%s\n", CurrentFn->name);
        return;
    case ND_BLOCK:
        for (Node *n = node->Body; n; n = n->next)
        {
            genStmt(n);
        }
        return;
    default:
        break;
    }

    errorTok(node->Tok, "invalid statement");
}

// 将 N 对齐到 Align 的整数倍
static int alignTo(int N, int Align)
{
    return (N + Align - 1) / Align * Align;
}

static void assignLVarOffsets(Func *fn)
{ // 为每个函数计算其变量所用的栈空间
    for (Func *Fn = fn; Fn; Fn = Fn->next)
    {
        int offset = 0;
        for (Obj *var = Fn->locals; var; var = var->next)
        {
            offset += 8;
            var->offset = -offset;
        }
        Fn->stackSize = alignTo(offset, 16); // 将栈对齐到 16 字节（内存对齐），优化处理器访问
    }
}

void codegen(Func *fn)
{
    assignLVarOffsets(fn);
    // 为每个函数单独生成代码
    for (Func *Fn = fn; Fn; Fn = Fn->next)
    {
        printf("\n  # 定义全局%s段\n", Fn->name);
        printf("  .globl %s\n", Fn->name);
        printf("# =====%s段开始===============\n", Fn->name);
        printf("# %s段标签\n", Fn->name);
        printf("%s:\n", Fn->name);
        CurrentFn = Fn;

        // 栈布局
        //-------------------------------// sp(原)
        //              ra
        //-------------------------------// ra = sp(原)-8
        //              fp
        //-------------------------------// fp = sp(原)-16
        //             变量
        //-------------------------------// sp = sp(原)-16-StackSize
        //           表达式计算
        //-------------------------------//

        // Prologue, 预处理
        // 将 fp 压入栈中，保存 fp 的值
        printf("  addi sp, sp, -16\n");
        printf("  # 将 ra 寄存器压栈，保存 ra 的值\n");
        printf("  sd ra, 8(sp)\n");
        printf("  # 将 fp 压栈，fp 属于“被调用者保存”的寄存器，需要恢复原值\n");
        printf("  sd fp, 0(sp)\n");
        // mv a, b. 将寄存器 b 中的值存储到寄存器 a 中
        printf("  # 将 sp 的值写入 fp\n");
        printf("  mv fp, sp\n"); // 将 sp 写入 fp
        // 26 个字母*8 字节=208 字节，栈腾出 208 字节的空间
        printf("  # sp 腾出 StackSize 大小的栈空间\n");
        printf("  addi sp, sp, -%d\n", Fn->stackSize);

        int cnt = 0;
        for (Obj *Var = Fn->Params; Var; Var = Var->next)
        {
            printf("  # 将%s寄存器的值存入%s的栈地址\n", ArgReg[cnt], Var->name);
            printf("  sd %s, %d(fp)\n", ArgReg[cnt++], Var->offset);
        }

        // 生成语句链表的代码
        printf("# =====%s段主体===============\n", Fn->name);
        genStmt(Fn->body);
        assert(Depth == 0);

        // Epilogue，后处理
        printf("# =====%s段结束===============\n", Fn->name);
        printf("# return 段标签\n");
        printf(".L.return.%s:\n", Fn->name); // 输出 return 段标签

        printf("  # 将 fp 的值写回 sp\n");
        printf("  mv sp, fp\n");
        printf("  # 将最早 fp 保存的值弹栈，恢复 fp 和 sp\n");
        printf("  ld fp, 0(sp)\n"); // 将栈顶元素（fp）弹出并存储到 fp
        printf("  # 将 ra 寄存器弹栈，恢复 ra 的值\n");
        printf("  ld ra, 8(sp)\n");    // 将 ra 寄存器弹栈，恢复 ra 的值
        printf("  addi sp, sp, 16\n"); // 移动 sp 到初始态，消除 fp 的影响

        printf("  # 返回 a0 值给系统调用\n");
        printf("  ret\n");
    }
}