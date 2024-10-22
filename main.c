#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *Input; // 读入的内容

// 词法分析
typedef enum
{
  TK_PUNCT, // 操作符
  TK_NUM,   // 数字
  TK_EOF,   // 终止符
} TokenKind;

typedef struct Token Token;

struct Token
{
  TokenKind kind;
  Token *next;

  int Val;
  char *Loc; // 在字符串中的位置

  int Len; // 长度
};

static Token *newToken(TokenKind kind, char *start, char *end)
{
  Token *tok = calloc(1, sizeof(Token)); // 由于编译器的运行时间较短，可以不回收内存，而是等待程序结束后操作系统的回收，以提升效率
  tok->kind = kind;
  tok->Loc = start;
  tok->Len = end - start;
  return tok;
}

// 输出错误信息
static void error(char *Fmt, ...)
{
  va_list VA;
  va_start(VA, Fmt);         // VA 获取 Fmt 后面的所有参数
  vfprintf(stderr, Fmt, VA); // vfprintf 可以输出 va_list 类型的参数
  fprintf(stderr, "\n");
  // 清除 VA
  va_end(VA);
  exit(1);
}

// 输出错误出现的位置
static void verrorAt(char *Cur, char *Fmt, va_list VA)
{
  // 原文
  fprintf(stderr, "%s\n", Input);

  // 定位错误位置
  int Pos = Cur - Input;
  // 输出错误信息
  fprintf(stderr, "%*s", Pos, "");
  fprintf(stderr, "^ ");
  vfprintf(stderr, Fmt, VA);
  fprintf(stderr, "\n");
  va_end(VA);
}

// 字符解析错误
static void errorAt(char *Loc, char *Fmt, ...)
{
  va_list VA;
  va_start(VA, Fmt);
  verrorAt(Loc, Fmt, VA);
  exit(1);
}

// Tok 解析错误
static void errorTok(Token *T, char *Fmt, ...)
{
  va_list VA;
  va_start(VA, Fmt);
  verrorAt(T->Loc, Fmt, VA);
  exit(1);
}

static int getNum(Token *T)
{
  if (T->kind != TK_NUM)
  {
    errorTok(T, "expect a number");
  }
  return T->Val;
}

static bool equal(Token *T, char *ch)
{
  return memcmp(T->Loc, ch, T->Len) == 0 && ch[T->Len] == '\0';
}

// 跳过指定的字符
static Token *skip(Token *T, char *ch)
{
  if (!equal(T, ch))
  {
    errorTok(T, "expected: %s, got: %s", ch, T->Loc);
  }
  return T->next;
}

// 判断 Str 是否以 SubStr 开头
static bool startsWith(char *Str, char *SubStr)
{
  return strncmp(Str, SubStr, strlen(SubStr)) == 0; // 比较 Str 和 SubStr 的 N 个字符是否相等
}

static int isPunct(char *P)
{
  if (startsWith(P, "==") || startsWith(P, "!=") || startsWith(P, "<=") || startsWith(P, ">="))
  {
    return 2;
  }
  else if ispunct (*P)
  {
    return 1;
  }
  return 0;
}

static Token *lexical_analysis()
{
  char *P = Input;
  Token Head = {}; // 空头指针，避免处理边界问题
  Token *Cur = &Head;

  while (*P)
  {
    if (isspace(*P)) // 跳过不可视的空白字符
    {
      ++P;
      continue;
    }

    int length = isPunct(P);
    if (length) // 是标点符号
    {
      Cur->next = newToken(TK_PUNCT, P, P + length);
      Cur = Cur->next;
      P += length;
    }
    else if (isdigit(*P))
    {
      char *older = P;
      const int num = strtoul(P, &P, 10);

      Cur->next = newToken(TK_NUM, older, P);
      Cur = Cur->next;
      Cur->Val = num;
    }
    else
    {
      errorAt(P, "invalid token");
    }
  }
  Cur->next = newToken(TK_EOF, P, P); // 添加终止节点
  return Head.next;
}

//
// 语法解析，生成 AST（抽象语法树）
//

typedef enum
{
  ND_EQ, // ==
  ND_NE, // !=
  ND_LT, // <
  ND_LE, // <=

  ND_ADD, // +
  ND_SUB, // -
  ND_MUL, // *
  ND_DIV, // /

  ND_NEG, // 负号

  ND_INT // 整形
} NodeKind;

typedef struct Node Node;
struct Node
{
  NodeKind kind;
  Node *LHS;
  Node *RHS;
  int Val;
};

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
    Node *node = newNumNode(getNum(Tok));
    *Rest = Tok->next;
    return node;
  }
  else
  {
    errorTok(Tok, "expected an expression");
    return NULL;
  }
}

//
// 语义分析与代码生成
//

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

int main(int Argc, char **Argv)
{
  if (Argc != 2)
  {
    error("%s: invalid number of arguments", Argv[0]);
  }

  Input = Argv[1];
  // 生成终结符流
  Token *T = lexical_analysis();

  Node *Node = expr(&T, T);
  if (T->kind == TK_EOF) // 语法分析后，token 应走到终止节点
  {
    printf("  .globl main\n");
    printf("main:\n");
    genExpr(Node); // 遍历 AST 树生成汇编
    // ret 为 jalr x0, x1, 0 别名指令，用于返回子程序
    printf("  ret\n");

    return 0;
  }
  else
  {
    errorTok(T, "extra token");
  }
}
