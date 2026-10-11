#ifndef CX_AST_H
#define CX_AST_H

#include "cx.h"

/* Типы узлов AST */
enum {
    AST_PROGRAM = 1,
    AST_VAR_DECL,
    AST_FUNC_DECL,
    AST_BLOCK,
    AST_RETURN,
    AST_IF,
    AST_WHILE,
    AST_BINOP,
    AST_UNOP,
    AST_INT_LIT,
    AST_STR_LIT,
    AST_IDENT,
    AST_CALL
};

/* Типы в Cx */
enum {
    CX_TYPE_INT,
    CX_TYPE_STR,
    CX_TYPE_VOID,
    CX_TYPE_BOOL
};

struct AstNode;
typedef struct AstNode AstNode;

struct AstNode {
    int type;         /* AST_* */

    /* Для всех */
    int line;

    /* Для VAR_DECL / FUNC_DECL */
    int cx_type;      /* CX_TYPE_* */
    int is_const;     /* для const */
    char name[64];

    /* Для FUNC_DECL */
    AstNode* params;   /* список параметров (как блок деклараций) */
    int      param_count;
    AstNode* body;     /* тело функции */

    /* Для BLOCK */
    AstNode** stmts;
    int       stmt_count;

    /* Для RETURN / IF / WHILE */
    AstNode* expr;     /* return expr;  /  if (expr)  /  while (expr) */
    AstNode* then_branch;
    AstNode* else_branch;

    /* Для BINOP / UNOP */
    int      op;        /* OP_* */
    AstNode* left;
    AstNode* right;

    /* Для INT_LIT / STR_LIT / IDENT */
    long int_val;
    char str_val[256];

    /* Для CALL */
    AstNode** args;
    int       arg_count;

    /* Для VAR_DECL */
    AstNode* init;
};

/* Парсер */
AstNode* cx_parse(const char* src);
void     cx_ast_dump(AstNode* node, int depth);

#endif