#ifndef CX_H
#define CX_H

#include "../utils/utils.h"

/* Типы токенов */
enum {
    TOK_EOF = 0,
    TOK_INT_LIT,      /* 123 */
    TOK_STR_LIT,      /* "hello" */
    TOK_IDENT,        /* foo, bar */
    TOK_KEYWORD,      /* int, if, while, return, void, const, type, mass */
    TOK_OP,           /* + - * / = == != < > <= >= && || ! */
    TOK_PUNCT         /* ( ) { } [ ] , ; */
};

/* Ключевые слова */
enum {
    KW_NONE = 0,
    KW_INT, KW_NUM, KW_DOUBLE, KW_FLOAT, KW_STR, KW_VOID,
    KW_IF, KW_ELSE, KW_WHILE, KW_RETURN,
    KW_CONST, KW_TYPE, KW_MASS, KW_TRUE, KW_FALSE
};

/* Операторы */
enum {
    OP_NONE = 0,
    OP_PLUS, OP_MINUS, OP_STAR, OP_SLASH,
    OP_ASSIGN, OP_EQ, OP_NEQ,
    OP_LT, OP_GT, OP_LE, OP_GE,
    OP_AND, OP_OR, OP_NOT
};

/* Пунктуация */
enum {
    P_NONE = 0,
    P_LPAREN, P_RPAREN,
    P_LBRACE, P_RBRACE,
    P_LBRACKET, P_RBRACKET,
    P_COMMA, P_SEMI
};

typedef struct {
    int      type;       /* TOK_* */
    int      kw;         /* KW_* если KEYWORD */
    int      op;         /* OP_* если OP */
    int      p;          /* P_* если PUNCT */
    long     int_val;    /* если INT_LIT */
    char     str_val[256]; /* если STR_LIT или IDENT */
    int      line;       /* строка в исходнике */
} Token;

/* Лексер */
typedef struct {
    const char* src;
    int         pos;
    int         line;
} Lexer;

void  cx_lexer_init(Lexer* lex, const char* src);
Token cx_lexer_next(Lexer* lex);

/* Отладка */
const char* cx_token_name(Token* t);

#endif