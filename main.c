#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
  TK_PUNCT, // 操作符
  TK_NUM,   // 数字
  TK_EOF,   // 终止符
} TokenKind;

static char *input;

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
  Token *tok = malloc(sizeof(Token)); // 由于编译器的运行时间较短，可以不回收内存，而是等待程序结束后操作系统的回收，以提升效率
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
  fprintf(stderr, "%s\n", input);

  // 定位错误位置
  int Pos = Cur - input;
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

static Token *parse()
{
  char *P = input;
  Token Head = {}; // 空头指针，避免处理边界问题
  Token *Cur = &Head;

  while (*P)
  {
    if (isspace(*P)) // 跳过不可视的空白字符
    {
      ++P;
      continue;
    }

    if (isdigit(*P))
    {
      char *older = P;
      const int num = strtoul(P, &P, 10);

      Cur->next = newToken(TK_NUM, older, P);
      Cur = Cur->next;
      Cur->Val = num;
    }
    else if (*P == '+' || *P == '-')
    {
      Cur->next = newToken(TK_PUNCT, P, P + 1);
      Cur = Cur->next;
      ++P;
    }
    else
    {
      errorAt(P, "invalid token");
    }
  }
  Cur->next = newToken(TK_EOF, P, P); // 添加终止节点
  return Head.next;
}

int main(int Argc, char **Argv)
{
  if (Argc != 2)
  {
    error("%s: invalid number of arguments", Argv[0]);
  }

  input = Argv[1];
  // 假设 Argv[1] 是运算表达式
  Token *T = parse();

  printf("  .globl main\n");
  printf("main:\n");
  // li 为 addi 别名指令，加载一个立即数到寄存器中
  printf("  li a0, %d\n", getNum(T)); // 假设算式为 num (op num) (op num)...的形式
  T = T->next;
  while (T->kind != TK_EOF)
  {
    if (equal(T, "+"))
    {
      T = T->next;
      // addi rd, rs1, imm 表示 rd = rs1 + imm
      printf("  addi a0, a0, %d\n", getNum(T));
      T = T->next;
    }
    else if (equal(T, "-"))
    {
      T = T->next;
      // addi 中 imm 为有符号立即数，所以减法表示为 rd = rs1 + (-imm)
      printf("  addi a0, a0, -%d\n", getNum(T));
      T = T->next;
    }
  }
  // ret 为 jalr x0, x1, 0 别名指令，用于返回子程序
  printf("  ret\n");

  return 0;
}
