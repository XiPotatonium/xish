/**
 * 声明定义了诸多重要的全局变量，
 * 声明定义了指令结构体和运算符枚举类型，
 * 声明定义了与这些全局变量和指令结构体相关的函数
 */

#pragma once

#ifndef __USE_POSIX
#define __USE_POSIX /* 为了使用signal.h中的kill() */
#endif

#include <stdio.h>

#define MAX_LINE 1000      /* 一行最多字符数 */
#define PATH_SIZE 256      /* 路径长度 */
#define MAX_CMD_IN_LINE 20 /* 一行最多包括指令数 */
#define MAX_JOBS 100       /* 最多同时进行的job */
#define MAX_CMD_LEN 20     /* 指令（不包括参数）长度 */

typedef enum OpType {
    OP_AND,  /* && */
    OP_OR,   /* || */
    OP_PIPE, /* | */
    OP_BG,   /* & */
    OP_SEQ,  /* ; */
    OP_NOP
} OpType;

typedef struct ShellCmd {
    char **argv;    /* 参数列表，包括指令本身，最后一个是NULL */
    OpType next_op; /* 指令之后的操作符号 */
    char *rdin;     /* 输入重定向 */
    char *rdout;    /* 输出重定向 */
    char *rderr;    /* 错误输出重定向 */
    int is_app;     /* 是否为追加 */
    struct ShellCmd *next;
} ShellCmd;

typedef enum JobState {
    JOB_NONE,      /* job不存在 */
    JOB_SUSPENDED, /* job被暂停 */
    JOB_RUNNING    /* job在运行中 */
} JobState;

typedef struct Job {
    int pid;               /* 进程pid */
    JobState state;        /* job的状态 */
    char cmd[MAX_CMD_LEN]; /* job的指令内容（不包括参数） */
} Job;

typedef struct Parameter {
    char *val; /* 变量内容 */
    struct Parameter *next;
} Parameter;

extern char g_cdpath[PATH_SIZE];    /* 当前工作目录 */
extern char g_xish_path[PATH_SIZE]; /* xish可执行文件路径 */
extern char g_xish_dir[PATH_SIZE];  /* xish可执行文件路径 */
extern char g_line[MAX_LINE];       /* 命令读入缓冲区 */
extern Job *g_curfgjob;             /* 当前前台job指针 */
extern Job g_joblist[MAX_JOBS];     /* job数组 */
extern Parameter *g_parlist;        /* xish参数链表 */

/* 默认构造函数，返回带默认值的ShellCmd结构体 */
ShellCmd *ShellCmd_c0();
/* 清除以head为头的ShellCmd链表，进行内存回收 */
void free_list(ShellCmd *head);
/* 初始化，参数interactive为1表明初始化交互式xish，否则为0 */
void init(int interactive);
/* 程序结束后恢复 */
void gbc();
/* 根据cmd提供的信息设置io文件描述符，以实现重定向 */
int set_iofd(ShellCmd *cmd);
/* 重置io文件描述符 */
void reset_iofd();
/* 让进程继续进行，同时修改joblist的内容 */
int pcont(int pid);
/* 解析字符串，如果是xish变量，那么替换成变量对应的字符串 */
char *parse_para(char *expr);
/* 异常处理 */
void exception(const char *err);