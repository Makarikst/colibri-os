#ifndef CX_H
#define CX_H

#include "../utils/utils.h"

// Типы токенов
enum {
    TOK_EOF = 0, TOK_INT_LIT, TOK_STR_LIT, TOK_IDENT,
    TOK_KEYWORD, TOK_OP, TOK_PUNCT
};

// Ключевые слова (добавлены интерфейсные)
enum {
    KW_NONE = 0, KW_INT, KW_STR, KW_PRINT,
    KW_LABEL, KW_TEXTVIEW
};

// Операторы
enum { OP_NONE = 0, OP_PLUS, OP_MINUS, OP_STAR, OP_SLASH, OP_ASSIGN, OP_EQ, OP_LT, OP_GT };

// Пунктуация
enum { P_NONE = 0, P_LPAREN, P_RPAREN, P_LBRACE, P_RBRACE, P_SEMI };

typedef struct {
    int type;
    int kw;
    int op;
    int p;
    long int_val;
    char str_val[256];
} Token;

typedef struct {
    const char* src;
    int pos;
    int line;
} Lexer;

// ---- Модель приложения (разбор MANIFEST + screen) ----
#define CX_MAX_WIDGETS 24
#define CX_TEXT_LEN    96

typedef struct {
    int  x, y;
    char text[CX_TEXT_LEN];
} CxWidget;

typedef struct {
    char     label[32];
    char     icon[128];
    int      has_screen;
    int      widget_count;
    CxWidget widgets[CX_MAX_WIDGETS];
} CxApp;

// Разбирает исходник приложения. Возвращает 1, если найден screen.
int   cx_app_load(CxApp* app, const char* source);

void  cx_lexer_init(Lexer* lex, const char* src);
Token cx_lexer_next(Lexer* lex);
void  cx_run_code(const char* source);
void  cx_run_code_buf(const char* source, char* out_buf, int max_len);

#endif