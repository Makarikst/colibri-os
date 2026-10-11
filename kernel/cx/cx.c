#include "cx.h"

/* ============================================
   Вспомогательные
   ============================================ */
static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static int is_alnum(char c) { return is_alpha(c) || is_digit(c); }

static char peek(Lexer* lex) { return lex->src[lex->pos]; }
static char peek2(Lexer* lex) { return lex->src[lex->pos + 1]; }
static char advance(Lexer* lex) {
    char c = lex->src[lex->pos++];
    if (c == '\n') lex->line++;
    return c;
}

/* ============================================
   Ключевые слова
   ============================================ */
static int kw_lookup(const char* s, int len) {
    #define KW(name, id) \
        if (len == (int)sizeof(name)-1 && strncmp(s, name, len) == 0) return id;
    KW("int", KW_INT);
    KW("num", KW_NUM);
    KW("double", KW_DOUBLE);
    KW("float", KW_FLOAT);
    KW("str", KW_STR);
    KW("void", KW_VOID);
    KW("if", KW_IF);
    KW("else", KW_ELSE);
    KW("while", KW_WHILE);
    KW("return", KW_RETURN);
    KW("const", KW_CONST);
    KW("type", KW_TYPE);
    KW("mass", KW_MASS);
    KW("true", KW_TRUE);
    KW("false", KW_FALSE);
    #undef KW
    return KW_NONE;
}

/* ============================================
   Инициализация
   ============================================ */
void cx_lexer_init(Lexer* lex, const char* src) {
    lex->src = src;
    lex->pos = 0;
    lex->line = 1;
}

/* ============================================
   Пропуск пробелов и комментариев
   ============================================ */
static void skip_ws(Lexer* lex) {
    while (1) {
        char c = peek(lex);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lex);
            continue;
        }
        /* Комментарий // до конца строки */
        if (c == '/' && peek2(lex) == '/') {
            while (peek(lex) && peek(lex) != '\n') advance(lex);
            continue;
        }
        /* Комментарий /* ... * / */
        if (c == '/' && peek2(lex) == '*') {
            advance(lex); advance(lex);
            while (peek(lex) && !(peek(lex) == '*' && peek2(lex) == '/'))
                advance(lex);
            if (peek(lex)) { advance(lex); advance(lex); }
            continue;
        }
        break;
    }
}

/* ============================================
   Чтение идентификатора / ключевого слова
   ============================================ */
static Token read_ident(Lexer* lex) {
    Token t = {0};
    t.line = lex->line;
    int i = 0;
    while (is_alnum(peek(lex)) && i < 255) {
        t.str_val[i++] = advance(lex);
    }
    t.str_val[i] = 0;

    int kw = kw_lookup(t.str_val, i);
    if (kw != KW_NONE) {
        t.type = TOK_KEYWORD;
        t.kw = kw;
    } else {
        t.type = TOK_IDENT;
    }
    return t;
}

/* ============================================
   Чтение числа
   ============================================ */
static Token read_number(Lexer* lex) {
    Token t = {0};
    t.line = lex->line;
    t.type = TOK_INT_LIT;
    long val = 0;
    while (is_digit(peek(lex))) {
        val = val * 10 + (advance(lex) - '0');
    }
    t.int_val = val;
    return t;
}

/* ============================================
   Чтение строки "..." с экранированием
   ============================================ */
static Token read_string(Lexer* lex) {
    Token t = {0};
    t.line = lex->line;
    t.type = TOK_STR_LIT;
    advance(lex); /* открывающая " */
    int i = 0;
    while (peek(lex) && peek(lex) != '"' && i < 255) {
        char c = advance(lex);
        if (c == '\\' && peek(lex)) {
            char n = advance(lex);
            if (n == 'n') c = '\n';
            else if (n == 't') c = '\t';
            else if (n == '\\') c = '\\';
            else if (n == '"') c = '"';
            else c = n;
        }
        t.str_val[i++] = c;
    }
    if (peek(lex) == '"') advance(lex);
    t.str_val[i] = 0;
    return t;
}

/* ============================================
   Главная функция
   ============================================ */
Token cx_lexer_next(Lexer* lex) {
    skip_ws(lex);

    Token t = {0};
    t.line = lex->line;

    char c = peek(lex);
    if (c == 0) { t.type = TOK_EOF; return t; }

    /* Идентификатор / ключевое слово */
    if (is_alpha(c)) return read_ident(lex);

    /* Число */
    if (is_digit(c)) return read_number(lex);

    /* Строка */
    if (c == '"') return read_string(lex);

    /* Двухсимвольные операторы */
    char c2 = peek2(lex);
    if (c == '=' && c2 == '=') { advance(lex); advance(lex);
        t.type = TOK_OP; t.op = OP_EQ; return t; }
    if (c == '!' && c2 == '=') { advance(lex); advance(lex);
        t.type = TOK_OP; t.op = OP_NEQ; return t; }
    if (c == '<' && c2 == '=') { advance(lex); advance(lex);
        t.type = TOK_OP; t.op = OP_LE; return t; }
    if (c == '>' && c2 == '=') { advance(lex); advance(lex);
        t.type = TOK_OP; t.op = OP_GE; return t; }
    if (c == '&' && c2 == '&') { advance(lex); advance(lex);
        t.type = TOK_OP; t.op = OP_AND; return t; }
    if (c == '|' && c2 == '|') { advance(lex); advance(lex);
        t.type = TOK_OP; t.op = OP_OR; return t; }

    /* Односимвольные */
    advance(lex);
    switch (c) {
        case '+': t.type=TOK_OP; t.op=OP_PLUS; return t;
        case '-': t.type=TOK_OP; t.op=OP_MINUS; return t;
        case '*': t.type=TOK_OP; t.op=OP_STAR; return t;
        case '/': t.type=TOK_OP; t.op=OP_SLASH; return t;
        case '=': t.type=TOK_OP; t.op=OP_ASSIGN; return t;
        case '<': t.type=TOK_OP; t.op=OP_LT; return t;
        case '>': t.type=TOK_OP; t.op=OP_GT; return t;
        case '!': t.type=TOK_OP; t.op=OP_NOT; return t;

        case '(': t.type=TOK_PUNCT; t.p=P_LPAREN; return t;
        case ')': t.type=TOK_PUNCT; t.p=P_RPAREN; return t;
        case '{': t.type=TOK_PUNCT; t.p=P_LBRACE; return t;
        case '}': t.type=TOK_PUNCT; t.p=P_RBRACE; return t;
        case '[': t.type=TOK_PUNCT; t.p=P_LBRACKET; return t;
        case ']': t.type=TOK_PUNCT; t.p=P_RBRACKET; return t;
        case ',': t.type=TOK_PUNCT; t.p=P_COMMA; return t;
        case ';': t.type=TOK_PUNCT; t.p=P_SEMI; return t;
    }
    /* Неизвестный символ — вернуть как EOF (или можно добавить TOK_ERROR) */
    t.type = TOK_EOF;
    return t;
}

/* ============================================
   Отладочное имя токена
   ============================================ */
const char* cx_token_name(Token* t) {
    switch (t->type) {
        case TOK_EOF:     return "EOF";
        case TOK_INT_LIT: return "INT_LIT";
        case TOK_STR_LIT: return "STR_LIT";
        case TOK_IDENT:   return "IDENT";
        case TOK_KEYWORD: return "KEYWORD";
        case TOK_OP:      return "OP";
        case TOK_PUNCT:   return "PUNCT";
    }
    return "?";
}