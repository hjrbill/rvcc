#include "rvcc.h"

//
// 语义分析与代码生成
//

int main(int Argc, char **Argv)
{
  if (Argc != 2)
  {
    error("%s: invalid number of arguments", Argv[0]);
  }

  // 生成终结符流
  Token *Tok = tokenizeFile(Argv[1]);

  Obj *fn = parse(Tok);

  codegen(fn);

  return 0;
}
