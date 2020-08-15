/**
 * 声明定义了诸多重要的全局变量，
 * 声明定义了指令结构体和运算符枚举类型，
 * 声明定义了与这些全局变量和指令结构体相关的函数
 */

#include "../inc/xish.h"

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define FORCE_STOP 3
#define FORCE_SUSP 26

char g_cdpath[PATH_SIZE];
char g_xish_path[PATH_SIZE];
char g_xish_dir[PATH_SIZE];
char g_line[MAX_LINE];
Job *g_curfgjob;
Job g_joblist[MAX_JOBS];
Parameter *g_parlist = NULL;

/* 文件描述备份 */
static int s_ifd_backup;
static int s_ofd_backup;
static int s_efd_backup;

/* 设置搜索路径 */
static void setpath(char *newpath) { setenv("PATH", newpath, 1); }

/* Ctrl-z事件处理函数 */
static void susp_handler(int sig) {
    if (g_curfgjob) {
        fprintf(stdout, "Process %d suspended\n", g_curfgjob->pid);
        signal(SIGTSTP, SIG_DFL);
        kill(g_curfgjob->pid, SIGTSTP);
        signal(SIGTSTP, susp_handler);
        g_curfgjob->state = JOB_SUSPENDED;
        g_curfgjob = NULL;
    }
}

/* Ctrl-c事件处理函数 */
static void terminate_handler(int sig) {
    if (g_curfgjob) {
        fprintf(stdout, "Process %d stopped\n", g_curfgjob->pid);
        signal(SIGINT, SIG_DFL);
        kill(g_curfgjob->pid, SIGINT);
        signal(SIGINT, terminate_handler);
        g_curfgjob->state = JOB_NONE;
        g_curfgjob = NULL;
    }
}

ShellCmd *ShellCmd_c0() {
    ShellCmd *ret = (ShellCmd *)malloc(sizeof(ShellCmd));
    ret->argv = NULL;
    ret->next = NULL;
    ret->next_op = OP_NOP;
    ret->rdin = ret->rdout = ret->rderr = NULL;
    ret->is_app = 0;
    return ret;
}

void free_list(ShellCmd *pcur) {
    ShellCmd *ptemp = NULL;
    while (pcur) {
        if (pcur->argv) {
            for (int i = 0; pcur->argv[i] != NULL; ++i) {
                free(pcur->argv[i]);
            }
            free(pcur->argv);
        }
        ptemp = pcur;
        pcur = pcur->next;
        free(ptemp);
    }
}

void init(int interactive) {
    /* 进行文件描述符备份 */
    s_ifd_backup = dup(STDIN_FILENO);
    s_ofd_backup = dup(STDOUT_FILENO);
    s_efd_backup = dup(STDERR_FILENO);

    /* 获得当前工作目录和xish程序目录 */
    if (getcwd(g_cdpath, PATH_SIZE) == NULL) {
        exception("Fail to get current work dir\n");
    }
    int rslt = readlink("/proc/self/exe", g_xish_path, PATH_SIZE - 1);
    if (rslt < 0 || (rslt >= PATH_SIZE - 1)) {
        exception("Fail to get xish dir\n");
    }
    g_xish_path[rslt] = '\0';
    strcpy(g_xish_dir, g_xish_path);
    // 从xish的文件路径获得xish目录路径
    for (int i = rslt; ; --i) {
        if (g_xish_dir[i] == '/') {
            g_xish_dir[i + 1] = 0;
            break;
        }
    }

    /* 设置shell系统变量 */
    setenv("shell", g_xish_path, 1);
    /* 设置程序搜索路径 */
    setpath("/bin:/usr/bin");
    /* 初始化job数组 */
    for (int i = 0; i < MAX_JOBS; ++i) {
        g_joblist[i].state = JOB_NONE;
    }
    g_curfgjob = NULL;
    /* 注册SIGINT和SIGTSTP事件的处理函数 */
    if (interactive) {
        signal(SIGINT, terminate_handler);
        signal(SIGTSTP, susp_handler);
    }
    fflush(stdout);
}

void restore() {
    /* 重设处理函数 */
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
}

/**
 * 注：dup2(fd1, fd2)会将fd1的文件信息复制到fd2中，
 * 例如dup2(fd, STDIN_FILENO)会将fd的文件描述信息复制到STDIN_FILENO中，
 * 这样，使用STDIN_FILENO等同于使用fd，也就是说输入被重定向至fd对应的文件
 * 本函数采用这样的方法实现重定向，executor模块中管道的实现方式也大致如此
 */
int set_iofd(ShellCmd *cmd) {
    int fd;
    if (cmd->rdin) {
        /* 输入重定向 */
        fd = open(cmd->rdin, O_RDONLY);
        if (fd < 0) {
            return -1;
        } else {
            dup2(fd, STDIN_FILENO);
        }
    }
    if (cmd->rdout) {
        /* 输出重定向 */
        if (strcmp(cmd->rdout, "&2") == 0) {
            dup2(STDERR_FILENO, STDOUT_FILENO);
        } else {
            if (cmd->is_app) {
                fd = open(cmd->rdout, O_WRONLY | O_CREAT | O_APPEND);
            } else {
                fd = open(cmd->rdout, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
            }
            if (fd < 0) {
                return -1;
            } else {
                dup2(fd, STDOUT_FILENO);
            }
        }
    }
    if (cmd->rderr) {
        /* 错误输出重定向 */
        if (strcmp(cmd->rderr, "&1") == 0) {
            dup2(STDOUT_FILENO, STDERR_FILENO);
        } else {
            if (cmd->is_app) {
                fd = open(cmd->rderr, O_WRONLY | O_CREAT | O_APPEND);
            } else {
                fd = open(cmd->rdout, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
            }
            if (fd < 0) {
                return -1;
            } else {
                dup2(fd, STDERR_FILENO);
            }
        }
    }
    return 0;
}

void reset_iofd() {
    dup2(s_ifd_backup, STDIN_FILENO);
    dup2(s_ofd_backup, STDOUT_FILENO);
    dup2(s_efd_backup, STDERR_FILENO);
}

int pcont(int pid) {
    int i = 0;
    for (i = 0; i < MAX_JOBS; ++i) {
        if (g_joblist[i].state != JOB_NONE && g_joblist[i].pid == pid) {
            /* 找到pid对应的job，发送信号让进程继续，修改job数组 */
            kill(pid, SIGCONT);
            g_joblist[i].state = JOB_RUNNING;
            g_curfgjob = &g_joblist[i];
            break;
        }
    }
    if (i == MAX_JOBS) {
        fprintf(stderr, "Process %d doesn't exist\n", pid);
    }
    return 0;
}

char *parse_para(char *expr) {
    char *ret = expr;
    if (expr && expr[0] == '$') {
        ++expr;
        if (*expr == '0') {
            /* $0，程序的名字 */
            ret = "-xish";
        } else if ((*expr >= '1' && *expr <= '9') || *expr == '{') {
            /* $1-$9, ${xxx}，命令行参数 */
            int index;
            if (*expr == '{') {
                ++expr;
                char *end = expr;
                while (*end != '\0' && *end != '}') {
                    ++end;
                }
                char ch = *end;
                *end = '\0';
                index = atoi(expr) - 1;
                *end = ch;
            } else {
                index = *expr - '1';
            }
            Parameter *p = g_parlist;
            for (int j = 0; j < index; ++j) {
                if (!p) {
                    break;
                }
                p = p->next;
            }
            if (p) {
                ret = p->val;
            }
        }
    }
    return ret;
}

void exception(const char *err) {
    reset_iofd();
    fprintf(stderr, "%s", err);
    restore();
    exit(0);
}
