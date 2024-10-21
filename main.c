#include <stdio.h>
#include <stdlib.h>

int main(int Argc, char **Argv)
{
  if (Argc != 2)
  {
    fprintf(stderr, "%s: invalid number of arguments\n", Argv[0]);
    return 1;
  }

  // 假设 Argv[1] 是运算表达式
  char *P = Argv[1];

  printf("  .globl main\n");
  printf("main:\n");
  // li 为 addi 别名指令，加载一个立即数到寄存器中
  // 将算式分解为 num (op num) (op num)...的形式
  printf("  li a0, %ld\n", strtol(P, &P, 10));
  while (*P)
  {
    if (*P == '+')
    {
      ++P; // 跳过符号
      // addi rd, rs1, imm 表示 rd = rs1 + imm
      printf("addi a0, a0, %ld\n", strtol(P, &P, 10));
    }
    else if (*P == '-')
    {
      ++P;
      // addi 中 imm 为有符号立即数，所以减法表示为 rd = rs1 + (-imm)
      printf("addi a0, a0, -%ld\n", strtol(P, &P, 10));
    }
    else
    {
      fprintf(stderr, "invalid char in %s: '%c'\n", Argv[1], *P);
      return 1;
    }
  }
  // ret 为 jalr x0, x1, 0 别名指令，用于返回子程序
  printf("  ret\n");

  return 0;
}
