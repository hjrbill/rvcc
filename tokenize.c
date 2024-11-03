#include "rvcc.h"

static char *Input;           // 读入的内容
static char *CurrentFilename; // 输入的文件名

// 输出错误信息
void error(char *Fmt, ...)
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
void verrorAt(char *Cur, char *Fmt, va_list VA)
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
void errorAt(char *Loc, char *Fmt, ...)
{
    va_list VA;
    va_start(VA, Fmt);
    verrorAt(Loc, Fmt, VA);
    exit(1);
}

// Tok 解析错误
void errorTok(Token *T, char *Fmt, ...)
{
    va_list VA;
    va_start(VA, Fmt);
    verrorAt(T->Loc, Fmt, VA);
    exit(1);
}

bool equal(Token *T, char *ch)
{
    return memcmp(T->Loc, ch, T->Len) == 0 && ch[T->Len] == '\0';
}

// 跳过指定的字符，如果与指定的字符不同，则报错
Token *skip(Token *T, char *ch)
{
    if (!equal(T, ch))
    {
        errorTok(T, "expected: %s, got: %s", ch, T->Loc);
    }
    return T->next;
}

// 消耗掉指定字符的 Token，如果不匹配只会返回 false
bool consume(Token **Rest, Token *Tok, char *str)
{
    if (equal(Tok, str))
    {
        *Rest = Tok->next;
        return true;
    }
    *Rest = Tok;
    return false;
}

static Token *newToken(TokenKind kind, char *start, char *end)
{
    Token *tok = calloc(1, sizeof(Token)); // 由于编译器的运行时间较短，可以不回收内存，而是等待程序结束后操作系统的回收，以提升效率
    tok->kind = kind;
    tok->Loc = start;
    tok->Len = end - start;
    return tok;
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

// 判断是否符号标识符首字母
static bool isIdentHead(char c)
{
    if ('a' <= c && c <= 'z' || c == '_')
    {
        return true;
    }
    return false;
}

// 判断是否符号标识符的非首字母部分
static bool isIdentBody(char c)
{
    return isIdentHead(c) || ('0' <= c && c <= '9');
}

static bool isKeyword(Token *T)
{
    static char *keywordList[] = {"return", "if", "else", "for", "while", "int", "sizeof", "char"};

    for (int i = 0; i < sizeof(keywordList) / sizeof(*keywordList); i++)
    {
        if (equal(T, keywordList[i]))
        {
            return true;
        }
    }
    return false;
}

// 返回一位十六进制的十进制（hexDigit = [0-9a-fA-F]）
static int fromHex(char C)
{
    if ('0' <= C && C <= '9')
        return C - '0';
    if ('a' <= C && C <= 'f')
        return C - 'a' + 10;
    return C - 'A' + 10;
}

// 处理转义字符
static int readEscapedChar(char **NewPos, char *P)
{
    if ('0' <= *P && *P <= '7')
    {
        // 读取一个不长于三位的八进制数字，
        // \abc = (a*8+b)*8+c
        int C = *P++ - '0';
        if ('0' <= *P && *P <= '7')
        {
            C = (C << 3) + (*P++ - '0');
            if ('0' <= *P && *P <= '7')
                C = (C << 3) + (*P++ - '0');
        }
        *NewPos = P;
        return C;
    }
    else if (*P == 'x')
    {
        P++;
        // 判断是否为十六进制数字
        if (!isxdigit(*P))
            errorAt(P, "invalid hex escape sequence");
        int C = 0;
        // \xWXYZ = ((W*16+X)*16+Y)*16+Z
        for (; isxdigit(*P); P++)
            C = (C << 4) + fromHex(*P);
        *NewPos = P;
        return C;
    }

    *NewPos = P + 1; // 跳到转义字符后面的字符
    switch (*P)
    {
    case 'a': // 响铃（警报）
        return '\a';
    case 'b': // 退格
        return '\b';
    case 't': // 水平制表符，tab
        return '\t';
    case 'n': // 换行
        return '\n';
    case 'v': // 垂直制表符
        return '\v';
    case 'f': // 换页
        return '\f';
    case 'r': // 回车
        return '\r';
    // GNU C 拓展
    case 'e': // 转义符
        return 27;
    default: // 默认将原字符返回
        return *P;
    }
}

// 读取到字符串字面量尾部（'"'）
static char *stringLiteralEnd(char *P)
{
    char *start = P;
    while (*P != '"')
    {
        if (*P == '\n' || *P == '\0') // 遇到换行符和'\0'则报错
        {
            errorAt(start, "unclosed string literal");
        }
        else if (*P == '\\')
        {
            P++;
        }
        P++;
    }
    return P;
}

static Token *readStringLiteral(char *Start)
{
    // 读取到字符串字面量的右引号
    char *End = stringLiteralEnd(Start + 1);
    // 定义一个与字符串字面量内字符数 +1 的 Buf，用来存储最大位数的字符串字面量
    char *Buf = calloc(1, End - Start);
    // 实际的字符位数，一个转义字符为 1 位
    int Len = 0;

    // 将读取后的结果写入 Buf
    for (char *P = Start + 1; P < End;)
    {
        if (*P == '\\')
        {
            Buf[Len++] = readEscapedChar(&P, P + 1);
        }
        else
        {
            Buf[Len++] = *P++;
        }
    }

    // Token 这里需要包含带双引号的字符串字面量
    Token *Tok = newToken(TK_STR, Start, End + 1);
    Tok->type = arrayOf(TyChar, Len + 1); // 为\0增加一位
    Tok->Str = Buf;
    return Tok;
}

// 将为关键字的 TK_IDENT 节点转换为 TK_KEYWORD 节点
static void convertKeywords(Token *Tok)
{
    for (Token *t = Tok; t; t = t->next)
    {
        if (equal(t, "return"))
        {
            t->kind = TK_KEYWORD;
        }
    }
}

// 终结符解析
Token *tokenize(char *Filename, char *P)
{
    CurrentFilename = Filename;
    Input = P;
    Token Head = {}; // 空头指针，避免处理边界问题
    Token *Cur = &Head;

    while (*P)
    {

        if (startsWith(P, "//")) // 跳过行注释
        {
            P += 2;
            while (*P != '\n')
                P++;
            continue;
        }
        else if (startsWith(P, "/*")) // 跳过块注释
        {
            // 查找第一个"*/"的位置
            char *Q = strstr(P + 2, "*/");
            if (!Q)
                errorAt(P, "unclosed block comment");
            P = Q + 2;
            continue;
        }

        if (isspace(*P)) // 跳过不可视的空白字符
        {
            ++P;
            continue;
        }
        else if (isdigit(*P))
        {
            char *start = P;
            const int num = strtoul(P, &P, 10);

            Cur->next = newToken(TK_NUM, start, P);
            Cur = Cur->next;
            Cur->Val = num;
            continue;
        }
        else if (*P == '"') // 解析字符串字面量
        {
            Cur->next = readStringLiteral(P);
            Cur = Cur->next;
            P += Cur->Len;
            continue;
        }
        else if (isIdentHead(*P)) // 解析标记符或关键字
        {
            char *start = P;
            do
            {
                ++P;
            } while (isIdentBody(*P));
            Cur->next = newToken(TK_IDENT, start, P);
            Cur = Cur->next;
            continue;
        }

        int length = isPunct(P);
        if (length) // 是标点符号
        {
            Cur->next = newToken(TK_PUNCT, P, P + length);
            Cur = Cur->next;
            P += length;
        }
        else
        {
            errorAt(P, "invalid token");
        }
    }

    Cur->next = newToken(TK_EOF, P, P); // 添加终止节点

    convertKeywords(Cur);
    return Head.next;
}

// 读取指定文件
static char *readFile(char *Path)
{
    FILE *FP;
    if (strcmp(Path, "-") == 0)
    {
        // 如果文件名是"-"，那么就从输入中读取
        FP = stdin;
    }
    else
    {
        FP = fopen(Path, "r");
        if (!FP)
            // strerror 以字符串的形式输出错误代码，errno 为系统最后一次的错误代码
            error("cannot open %s: %s", Path, strerror(errno));
    }

    // 要返回的字符串
    char *Buf;
    size_t BufLen;
    FILE *Out = open_memstream(&Buf, &BufLen);

    while (true)
    {
        char Buf2[4096];
        // fread 从文件流中读取数据到数组中
        int N = fread(Buf2, 1, sizeof(Buf2), FP);
        if (N == 0)
            break;
        fwrite(Buf2, 1, N, Out);
    }

    // 如果来源是文件，关闭它
    if (FP != stdin)
    {
        fclose(FP);
    }

    // 刷新流的输出缓冲区，确保内容都被输出到流中
    fflush(Out);
    // 确保最后一行以'\n'结尾
    if (BufLen == 0 || Buf[BufLen - 1] != '\n')
        // 将字符输出到流中
        fputc('\n', Out);
    fputc('\0', Out);
    fclose(Out);

    return Buf;
}

// 对文件进行词法分析
Token *tokenizeFile(char *Path)
{
    return tokenize(Path, readFile(Path));
}
