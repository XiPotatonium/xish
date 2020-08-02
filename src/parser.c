/**
 * 声明定义了各种符号
 * 声明定义了两个用于将字符串转换为ShellCmd结构体链表的函数
 */

#include "../inc/parser.h"

#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 1000 /* 符号缓冲区大小，用于tokenize */

static ShellCmd *_parse(TokenNode *tokens) {
    ShellCmd *ret = NULL, *pcur = NULL, *ptemp = NULL;
    TokenNode *temp_node, *delete_node;
    while (tokens) {
        if (tokens->type == TOKEN_WORD) {
            ptemp = ShellCmd_c0();
            /* 删除从temp_node开始 */
            temp_node = tokens;
            /* 解析这条命令的参数 */
            TokenNode *arg = tokens->next;
            TokenNode *prev = NULL;
            int argc = 1;
            while (arg) {
                if (arg->type == TOKEN_WORD) {
                    /* 是普通参数 */
                    argc++;
                } else if (arg->type == TOKEN_RDAPP || arg->type == TOKEN_RDOUT) {
                    /* 是输出重定向 */
                    ptemp->is_app = arg->type == TOKEN_RDAPP ? 1 : 0;
                    if (!arg->next || arg->next->type != TOKEN_WORD) {
                        fprintf(stderr, "parse: Syntax error\n");
                        return NULL;
                    }
                    /* 如果之前有1，则是将标准输出重定向，如果是2，则是将标准错误输出重定向 */
                    if (prev) {
                        if (strcmp(prev->str, "1") == 0) {
                            ptemp->rdout = arg->next->str;
                            prev->type = TOKEN_ABANDON;
                            argc--;
                        } else if (strcmp(prev->str, "2") == 0) {
                            ptemp->rderr = arg->next->str;
                            prev->type = TOKEN_ABANDON;
                            argc--;
                        }
                    }
                    ptemp->rdout = arg->next->str;
                    arg->type = TOKEN_ABANDON;
                    arg->next->type = TOKEN_ABANDON;
                    arg = arg->next;
                } else if (arg->type == TOKEN_RDIN) {
                    /* 是输入重定向 */
                    if (!arg->next || arg->next->type != TOKEN_WORD) {
                        fprintf(stderr, "parse: Syntax error\n");
                        return NULL;
                    }
                    ptemp->rdin = arg->next->str;
                    arg->type = TOKEN_ABANDON;
                    arg->next->type = TOKEN_ABANDON;
                    arg = arg->next;
                } else {
                    /* 遇到运算符，一条指令结束 */
                    break;
                }
                prev = arg;
                arg = arg->next;
            }
            /* 将参数全部转移到ShellCmd结构体中 */
            ptemp->argv = (char **)malloc((argc + 1) * sizeof(char *));
            for (int i = 0; tokens != arg; ++i) {
                if (tokens->type != TOKEN_ABANDON) {
                    ptemp->argv[i] = tokens->str;
                }
                tokens = tokens->next;
            }
            ptemp->argv[argc] = NULL;
            tokens = arg;

            /* 解析运算符 */
            if (tokens) {
                if (tokens->type == TOKEN_AND) {
                    ptemp->next_op = OP_AND;
                } else if (tokens->type == TOKEN_OR) {
                    ptemp->next_op = OP_OR;
                } else if (tokens->type == TOKEN_PIPE) {
                    ptemp->next_op = OP_PIPE;
                } else if (tokens->type == TOKEN_SIMICOLON) {
                    ptemp->next_op = OP_SEQ;
                } else if (tokens->type == TOKEN_AMPER) {
                    ptemp->next_op = OP_BG;
                }
                tokens = tokens->next;
            }
            /* 释放已解析完的指令的tokens的内存 */
            while (temp_node != tokens) {
                delete_node = temp_node;
                temp_node = temp_node->next;
                free(delete_node);
            }
        } else {
            fprintf(stderr, "parse: Syntax error\n");
            return NULL;
        }

        if (!ret) {
            ret = pcur = ptemp;
        } else {
            pcur->next = ptemp;
            pcur = pcur->next;
        }
    }
    /* 如果末尾有一个二元运算符，那么就去掉它 */
    if (pcur && (pcur->next_op == OP_AND || pcur->next_op == OP_OR || pcur->next_op == OP_PIPE)) {
        pcur->next_op = OP_NOP;
    }
    return ret;
}

static TokenNode *_tokenize(char *expr) {
    TokenNode *head_node = NULL, *cur_node = NULL, *temp_node = NULL;
    int word_cnt = 0;
    char temp_line[MAX_LINE];
    strcpy(temp_line, expr);
    char *pcur = temp_line;
    char *tail = NULL;
    /* 相当于splitter，对输入进行分词，同时考虑了双引号 */
    while (1) {
        if (*pcur == '\0') {
            tail = pcur;
            break;
        } else if (ISSPACE(*pcur)) {
            *pcur = '\0';
            ++pcur;
        } else if (*pcur == '"') {
            *pcur = '\0';
            ++pcur;
            while (*pcur && *pcur != '"') {
                ++pcur;
            }
            *pcur = '\0';
            ++pcur;
        } else {
            ++pcur;
        }
    }
    pcur = temp_line;
    /* 读取每一个分词，并解析成对应符号 */
    while (1) {
        while (pcur != tail && *pcur == '\0') {
            ++pcur;
        }
        if (pcur == tail) {
            break;
        }
        /* 构造一个TokenNode结构体 */
        temp_node = (TokenNode *)malloc(sizeof(TokenNode));
        temp_node->next = NULL;
        temp_node->str = NULL;
        /* 判断符号类型，运算符或者普通字符串 */
        if (strcmp(pcur, "||") == 0) {
            temp_node->type = TOKEN_OR;
            pcur = pcur + 2;
        } else if (strcmp(pcur, "|") == 0) {
            temp_node->type = TOKEN_PIPE;
            pcur++;
        } else if (strcmp(pcur, "&&") == 0) {
            temp_node->type = TOKEN_AND;
            pcur = pcur + 2;
        } else if (strcmp(pcur, "&") == 0) {
            temp_node->type = TOKEN_AMPER;
            pcur++;
        } else if (strcmp(pcur, "<") == 0) {
            temp_node->type = TOKEN_RDIN;
            ++pcur;
        } else if (strcmp(pcur, ">>") == 0) {
            temp_node->type = TOKEN_RDAPP;
            pcur = pcur + 2;
        } else if (strcmp(pcur, ">") == 0) {
            temp_node->type = TOKEN_RDOUT;
            pcur++;
        } else if (strcmp(pcur, ";") == 0) {
            temp_node->type = TOKEN_SIMICOLON;
            ++pcur;
        } else {
            /* 不是运算符，这里不判断到底是指令还是什么，直接全部读入 */
            int len = strlen(pcur);
            temp_node->str = (char *)malloc((len + 1) * sizeof(char));
            strcpy(temp_node->str, pcur);
            temp_node->type = TOKEN_WORD;
            pcur += len;
        }

        if (!head_node) {
            head_node = cur_node = temp_node;
        } else {
            cur_node->next = temp_node;
            cur_node = temp_node;
        }
        temp_node = NULL;
    }

    return head_node;
}

ShellCmd *parse(char *expr) {
    return _parse(_tokenize(expr));
}
