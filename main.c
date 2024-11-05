#include "rvcc.h"

//
// 语义分析与代码生成
//

// 目标文件的路径
static char *OptO;
// 输入文件的路径
static char *InputPath;

// 输出程序的使用说明
static void usage(int Status)
{
  fprintf(stderr, "rvcc [ -o <path> ] <file>\n");

  exit(Status);
}

// 解析传入程序的参数
static void parseArgs(int Argc, char **Argv)
{
  // 遍历所有传入程序的参数
  for (int i = 1; i < Argc; i++)
  {
    // 如果存在 help，则直接显示用法说明
    if (!strcmp(Argv[i], "--help"))
      usage(0);
    // 解析-o XXX 的参数
    if (!strcmp(Argv[i], "-o"))
    {
      // 不存在目标文件则报错
      if (!Argv[++i])
      {
        usage(1);
      }

      // 目标文件的路径
      OptO = Argv[i];
      continue;
    }

    // 解析-oXXX 的参数
    if (!strncmp(Argv[i], "-o", 2))
    {
      // 目标文件的路径
      OptO = Argv[i] + 2;
      continue;
    }

    // 解析为 - 的参数
    if (Argv[i][0] == '-' && Argv[i][1] != '\0')
    {
      error("unknown option: %s", Argv[i]);
    }

    // 其他情况则匹配为输入文件
    InputPath = Argv[i];
  }

  // 不存在输入文件时报错
  if (!InputPath)
  {
    error("no input files");
  }
}

// 打开需要写入的文件
static FILE *openFile(char *Path)
{
  if (!Path || strcmp(Path, "-") == 0) // 如果没有指定输出文件，那么就使用标准输出
  {
    return stdout;
  }

  // 以写入模式打开文件
  FILE *Out = fopen(Path, "w");

  if (!Out)
  {
    error("cannot open output file: %s: %s", Path, strerror(errno));
  }
  return Out;
}

int main(int Argc, char **Argv)
{
  // 解析传入程序的参数
  parseArgs(Argc, Argv);

  // 解析文件，生成终结符流
  Token *Tok = tokenizeFile(InputPath);

  // 解析终结符流
  Obj *Prog = parse(Tok);

  // 生成代码
  FILE *Out = openFile(OptO);
  // .file 文件编号 文件名，设置文件的编号和名称，供后续 .loc 指令引用。
  fprintf(Out, ".file 1 \"%s\"\n", InputPath);

  codegen(Prog, Out);

  return 0;
}
