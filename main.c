#include <stdio.h>
#include <stdlib.h>

int main(int Argc, char **Argv) {
  if (Argc != 2) {
    fprintf(stderr, "%s: invalid number of arguments\n", Argv[0]);
    return 1;
  }

  printf("  .globl main\n");
  printf("main:\n");
  // li 为 addi 别名指令，加载一个立即数到寄存器中
  // 在 RISC-V 架构中，a0 寄存器通常用于返回值
  printf("  li a0, %d\n", atoi(Argv[1]));
  // ret 为 jalr x0, x1, 0 别名指令，用于返回子程序
  printf("  ret\n");

  return 0;
}
