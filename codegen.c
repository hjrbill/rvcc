#include "rvcc.h"

// 栈深度
static int Depth;
// 用于存储函数参数的寄存器
static char *ArgReg[] = {"a0", "a1", "a2", "a3", "a4", "a5"};
// 当前的函数
static Obj *CurrentFn;

static void genExpr(Node *node);
static void genStmt(Node *node);

// 输出字符串并换行
static void println(char *Fmt, ...)
{
    va_list VA;
    va_start(VA, Fmt);
    vprintf(Fmt, VA);
    va_end(VA);
    printf("\n");
}

static int Count(void)
{
    static int cnt = 0;
    return cnt++;
}

// 将 a0 压入栈
static void push(void)
{
    println("  # 压栈，将 a0 的值存入栈顶\n");
    println("  addi sp, sp, -8\n");
    // sd rd, rs1, 将寄存器 rd 中的值存储到 rsq 上
    println("  sd a0, 0(sp)\n");
    Depth++;
}

// 从栈中取出元素并放到 Reg
static void pop(char *Reg)
{
    println("  # 弹栈，将栈顶的值存入%s\n", Reg);
    // ld rd, rs1，从内存中加载一个 32 位或 64 位的 rs1(操作数) 到寄存器 rd 中
    println("  ld %s, 0(sp)\n", Reg);
    // addi rd, rs1, imm 表示 rd = rs1 + imm
    println("  addi sp, sp, 8\n");
    Depth--;
}

// 加载 a0 指向的值
static void load(Type *Ty)
{
    if (Ty->kind == TY_ARRAY)
    {
        return;
    }

    println("  # 读取 a0 中存放的地址，得到的值存入 a0\n");
    if (Ty->size == 1)
    {
        println("  lb a0, 0(a0)\n");
    }
    else
    {
        println("  ld a0, 0(a0)\n");
    }
}

// 将栈顶值 (为一个地址) 存入 a0
static void store(Type *Ty)
{
    pop("a1");
    println("  # 将 a0 的值，写入到 a1 中存放的地址\n");
    if (Ty->size == 1)
    {
        println("  sb a0, 0(a1)\n"); // sb 代表 "store byte"，通常用于存储 1 个字节的数据
    }
    else
    {
        println("  sd a0, 0(a1)\n"); // sd 代表 "store doubleword"，通常用于存储 4 字节或 8 字节的数据
    }
};

// 计算给定节点的绝对地址，如果报错，说明节点不在内存中
static void getAddr(Node *node)
{
    switch (node->kind)
    {
    case ND_VAR:
        if (node->Var->isLocal)
        {
            println("  # 获取局部变量%s的栈内地址为%d(fp)\n", node->Var->name,
                    node->Var->offset);
            println("  addi a0, fp, %d\n", node->Var->offset); // 取出变量相对于 fp 的偏移量
        }
        else
        {
            println("  # 获取全局变量%s的栈内地址\n", node->Var->name);
            println("  la a0, %s\n", node->Var->name);
        }
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
        load(node->type);
        return;
    case ND_DEREF:
        genExpr(node->LHS);
        load(node->type);
        return;
    case ND_ADDR:
        getAddr(node->LHS);
        return;
    case ND_NEG: // 是取反
        genExpr(node->LHS);
        println("  # 对 a0 值进行取反\n");
        // neg a0, a0 是 sub a0, x0, a0 的别名，即 a0=0-a0
        println("  neg a0, a0\n");
        return;
    case ND_ASSIGN:         // 是赋值
        getAddr(node->LHS); // 左边为被赋值的地址
        push();
        genExpr(node->RHS); // 右边为赋予的值
        store(node->type);
        return;
    case ND_NUM: // 是整型
        println("  # 将%d加载到 a0 中\n", node->Val);
        // li 为 addi 别名指令，加载一个立即数到寄存器中
        println("  li a0, %d\n", node->Val);
        return;
    case ND_STMT_EXPR:
    {
        for (Node *Nd = node->Body; Nd; Nd = Nd->next)
            genStmt(Nd);
        return;
    }
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

        println("  # 调用%s函数\n", node->FuncName);
        println("  call %s\n", node->FuncName); // 调用函数
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
        println("  xor a0, a0, a1\n");
        // seqz a, b 判断 b 是否等于 0 并将结果放入 a
        println("  seqz a0, a0\n");
        return;
    case ND_NE:
        // a0=a0^a1，异或指令
        // 异或后如果相同，a0=1，否则 a0=0
        println("  # 判断是否 a0%sa1\n", node->kind == ND_EQ ? "=" : "≠");
        println("  xor a0, a0, a1\n");
        // snez a, b 判断 b 是否不等于 0 并将结果放入 a
        println("  snez a0, a0\n");
        return;
    case ND_LT:
        println("  # 判断 a0<a1\n");
        // slt a, b, c，将 b < c 的结果放入 a
        println("  slt a0, a0, a1\n");
        return;
    case ND_LE:
        println("  # 判断是否 a0≤a1\n");
        println("  slt a0, a1, a0\n");
        // xori a, b, (立即数)，将 b 异或 (立即数) 的结果放入 a
        println("  xori a0, a0, 1\n");
        return;
    case ND_ADD:
        println("  # a0+a1，结果写入 a0\n");
        println("  add a0, a0, a1\n");
        return;
    case ND_SUB:
        println("  # a0-a1，结果写入 a0\n");
        println("  sub a0, a0, a1\n");
        return;
    case ND_MUL:
        println("  # a0*a1，结果写入 a0\n");
        println("  mul a0, a0,  a1\n");
        return;
    case ND_DIV:
        println("  # a0/a1，结果写入a0\n");
        println("  div a0, a0, a1\n");
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
        println("\n# =====分支语句%d==============\n", cnt);

        println("\n# Cond 表达式%d\n", cnt);
        genExpr(node->Cond);
        println("  # 若 a0 为 0，则跳转到分支%d的.L.else.%d段\n", cnt, cnt);
        println("  beqz a0, .L.else.%d\n", cnt); // 判断条件是否不成立（a0=0），条件不成立跳转到 .L.else. 标签
        println("\n# Then 语句%d\n", cnt);
        genStmt(node->Then); // 条件成立，执行 then 语句
        println("  # 跳转到分支%d的.L.end.%d段\n", cnt, cnt);
        println("j .L.end.%d\n", cnt); // 执行完 then 语句后跳转到 .L.end. 标签

        println("\n# Else 语句%d\n", cnt);
        println("# 分支%d的.L.else.%d段标签\n", cnt, cnt);
        println(".L.else.%d:\n", cnt); // else 标记
        if (node->Else)                // 存在 else 语句
        {
            genStmt(node->Else);
        }

        println("\n# 分支%d的.L.end.%d段标签\n", cnt, cnt);
        println(".L.end.%d:\n", cnt); // if 语句结束
        return;
    }
    case ND_FOR:
    {
        int cnt = Count();

        println("\n# =====循环语句%d===============\n", cnt);
        if (node->Init)
        {
            println("\n# Init 语句%d\n", cnt);
            genStmt(node->Init); // 初始化语句
        }

        println("\n# 循环%d的.L.begin.%d段标签\n", cnt, cnt);
        println(".L.begin.%d:\n", cnt);

        println("# Cond 表达式%d\n", cnt);
        if (node->Cond) // 存在条件语句
        {
            genExpr(node->Cond);
            println("  # 若 a0 为 0，则跳转到循环%d的.L.end.%d段\n", cnt, cnt);
            println(" beqz a0, .L.end.%d\n", cnt);
        }

        println("\n# Then 语句%d\n", cnt);
        genStmt(node->Then); // 执行循环体

        if (node->Inc) // 存在递增语句
        {
            println("\n# Inc 语句%d\n", cnt);
            genExpr(node->Inc);
        }

        println("  # 跳转到循环%d的.L.begin.%d段\n", cnt, cnt);
        println("  j .L.begin.%d\n", cnt);
        println("\n# 循环%d的.L.end.%d段标签\n", cnt, cnt);
        println(".L.end.%d:\n", cnt);
        return;
    }
    case ND_RETURN:
        println("# 返回语句\n");
        genExpr(node->LHS);
        println("  # 跳转到.L.return.%s段\n", CurrentFn->name);
        // 无条件跳转语句，跳转到.L.return 段
        // j offset 是 jal x0, offset 的别名指令
        println("  j .L.return.%s\n", CurrentFn->name);
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

static void assignLVarOffsets(Obj *fn)
{ // 为每个函数计算其变量所用的栈空间
    for (Obj *Fn = fn; Fn; Fn = Fn->next)
    {
        if (!Fn->isFunction)
        {
            continue; // 不是函数，跳过
        }

        int offset = 0;
        for (Obj *var = Fn->locals; var; var = var->next)
        {
            offset += var->type->size;
            var->offset = -offset;
        }
        Fn->stackSize = alignTo(offset, 16); // 将栈对齐到 16 字节（内存对齐），优化处理器访问
    }
}

// 生成数据段（数据段是存储程序数据的内存区域，包括全局变量、静态变量、常量和程序中分配的其他数据结构。）
static void emitData(Obj *Prog)
{
    for (Obj *Var = Prog; Var; Var = Var->next)
    {
        if (Var->isFunction)
        {
            continue;
        }

        println("  # 数据段标签\n");
        println("  .data\n"); // 指示汇编器接下来的代码属于数据段
        if (Var->InitData)
        {
            println("%s:\n", Var->name);
            for (int i = 0; i < Var->type->size; i++)
            {
                char ch = Var->InitData[i];
                if (isprint(ch)) // 判断是否为可打印字符
                {
                    println("  # 字符：%c\n", ch);
                    println("  .byte %d\n", ch);
                }
                else
                {
                    println("  .byte %d\n", ch);
                }
            }
        }
        else
        {
            println("  # 全局段%s\n", Var->name);
            println("  .globl %s\n", Var->name); // 指示汇编器 Var->name 指定的符号是全局的，可以在其他地方被访问
            println("  # 全局变量%s\n", Var->name);
            println("%s:\n", Var->name);
            println("  # 全局变量零填充%d位\n", Var->type->size);
            println("  .zero %d\n", Var->type->size); // 为全局变量 Var->Name 分配 Var->type->Size 字节的内存空间，并将其初始化为零
        }
    }
}

// 生成文本段（数据段）
void emitText(Obj *Prog)
{
    for (Obj *Fn = Prog; Fn; Fn = Fn->next)
    {
        if (!Fn->isFunction)
        {
            continue;
        }

        println("\n  # 定义全局%s段\n", Fn->name);
        println("  .globl %s\n", Fn->name); // 指示汇编器 Fn->name 指定的符号是全局的，可以在其他地方被访问
        println("  # 文本段标签\n");
        println("  .text\n"); // 指示汇编器接下来的代码属于程序的文本段
        println("# =====%s段开始===============\n", Fn->name);
        println("# %s段标签\n", Fn->name);
        println("%s:\n", Fn->name);
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
        println("  addi sp, sp, -16\n");
        println("  # 将 ra 寄存器压栈，保存 ra 的值\n");
        println("  sd ra, 8(sp)\n");
        println("  # 将 fp 压栈，fp 属于“被调用者保存”的寄存器，需要恢复原值\n");
        println("  sd fp, 0(sp)\n");
        // mv a, b. 将寄存器 b 中的值存储到寄存器 a 中
        println("  # 将 sp 的值写入 fp\n");
        println("  mv fp, sp\n"); // 将 sp 写入 fp
        // 26 个字母*8 字节=208 字节，栈腾出 208 字节的空间
        println("  # sp 腾出 StackSize 大小的栈空间\n");
        println("  addi sp, sp, -%d\n", Fn->stackSize);

        int cnt = 0;
        for (Obj *Var = Fn->Params; Var; Var = Var->next)
        {
            println("  # 将%s寄存器的值存入%s的栈地址\n", ArgReg[cnt], Var->name);
            if (Var->type->size == 1)
            {
                println("  sb %s, %d(fp)\n", ArgReg[cnt++], Var->offset);
            }
            else
            {
                println("  sd %s, %d(fp)\n", ArgReg[cnt++], Var->offset);
            }
        }

        // 生成语句链表的代码
        println("# =====%s段主体===============\n", Fn->name);
        genStmt(Fn->body);
        assert(Depth == 0);

        // Epilogue，后处理
        println("# =====%s段结束===============\n", Fn->name);
        println("# return 段标签\n");
        println(".L.return.%s:\n", Fn->name); // 输出 return 段标签

        println("  # 将 fp 的值写回 sp\n");
        println("  mv sp, fp\n");
        println("  # 将最早 fp 保存的值弹栈，恢复 fp 和 sp\n");
        println("  ld fp, 0(sp)\n"); // 将栈顶元素（fp）弹出并存储到 fp
        println("  # 将 ra 寄存器弹栈，恢复 ra 的值\n");
        println("  ld ra, 8(sp)\n");    // 将 ra 寄存器弹栈，恢复 ra 的值
        println("  addi sp, sp, 16\n"); // 移动 sp 到初始态，消除 fp 的影响

        println("  # 返回 a0 值给系统调用\n");
        println("  ret\n");
    }
}

void codegen(Obj *Prog)
{
    // 计算局部变量的偏移量
    assignLVarOffsets(Prog);
    // 生成数据段
    emitData(Prog);
    // 生成文本段
    emitText(Prog);
}