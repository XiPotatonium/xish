/**
 * 模块：parser。文件：parser.h parser.c
 * 声明定义了各种符号
 * 声明定义了两个用于将字符串转换为ShellCmd结构体链表的函数
 */
 
#pragma once

#include "../inc/xish.h"

#define ISSPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n')
#define ISALPHA(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' || (c) <= 'z'))
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')

typedef enum TokenType {
    TOKEN_WORD,      // e.g. quit
    TOKEN_AND,       // &&
    TOKEN_OR,        // ||
    TOKEN_PIPE,      // |
    TOKEN_AMPER,     // &
    TOKEN_SIMICOLON, // ;
    TOKEN_RDIN,      // <
    TOKEN_RDOUT,     // >
    TOKEN_RDAPP,     // >>
    TOKEN_ABANDON
} TokenType;

typedef struct TokenNode {
    char *str;              /* 符号原文 */
    TokenType type;         /* 符号类型 */
    struct TokenNode *next;
} TokenNode;

/**
 * 函数parse：将符号链表转化为ShellCmd结构体链表
 * 参数：expr 符号链表
 * 返回：生成的ShellCmd结构体链表
 */
ShellCmd *parse(char *expr);
