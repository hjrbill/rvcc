#include "rvcc.h"

static char *Input; // 读入的内容

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
    static char *keywordList[] = {"return", "if", "else", "for", "while", "int"};

    for (int i = 0; i < sizeof(keywordList) / sizeof(*keywordList); i++)
    {
        if (equal(T, keywordList[i]))
        {
            return true;
        }
    }
    return false;
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

Token *tokenize(char *P)
{
    Input = P;
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
        else if (isIdentHead(*P)) // 解析标记符或关键字
        {
            char *start = P;
            do
            {
                ++P;
            } while (isIdentBody(*P));
            Cur->next = newToken(TK_IDENT, start, P);
            Cur = Cur->next;
        }
        else if (isdigit(*P))
        {
            char *start = P;
            const int num = strtoul(P, &P, 10);

            Cur->next = newToken(TK_NUM, start, P);
            Cur = Cur->next;
            Cur->Val = num;
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