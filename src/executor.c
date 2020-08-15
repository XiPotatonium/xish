/**
 * 声明定义了执行ShellCmd链表的函数
 * 声明定义了内置指令的执行函数。
 */

#include "../inc/executor.h"
#include "../inc/builtin.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* executor.h声明的execute函数定义 */
int execute(ShellCmd *pcur, int inter) {
    int res = CMDR_OK;
    int pipefd[2];
    pid_t pid;

    while (pcur) {
        /* 检查之前的jobs有没有结束 */
        Job *cur_job = NULL;
        for (int i = 0; i < MAX_JOBS; ++i) {
            // 回收已经完成的running进程，防止僵尸进程
            if (g_joblist[i].state == JOB_RUNNING) {
                if (waitpid(g_joblist[i].pid, NULL, WNOHANG) != 0) {
                    g_joblist[i].state = JOB_NONE;
                    /* 批处理不显示进程结束信息 */
                    if (inter) {
                        fprintf(stdout, "Process %d done\n", g_joblist[i].pid);
                    }
                }
            }
            // 第一个空位设置为cur_job
            if (!cur_job && g_joblist[i].state == JOB_NONE) {
                cur_job = &g_joblist[i];
            }
        }
        if (pcur->next_op == OP_PIPE) {
            if (pipe(pipefd) < 0) {
                // exception
                exception("Fail to create pipe");
            }
        }
        /* 获得内建指令的函数指针 */
        int (*builtin)(char **);
        int process_running = 1;
        if (strcmp(pcur->argv[0], "bg") == 0) {
            builtin = exe_bg;
            process_running = 0;
        } else if (strcmp(pcur->argv[0], "cd") == 0) {
            builtin = exe_cd;
            process_running = 0;
        } else if (strcmp(pcur->argv[0], "clear") == 0) {
            builtin = exe_clear;
        } else if (strcmp(pcur->argv[0], "dir") == 0) {
            builtin = exe_dir;
        } else if (strcmp(pcur->argv[0], "echo") == 0) {
            builtin = exe_echo;
        } else if (strcmp(pcur->argv[0], "env") == 0) {
            builtin = exe_env;
        } else if (strcmp(pcur->argv[0], "exec") == 0) {
            builtin = exe_exec;
        } else if (strcmp(pcur->argv[0], "exit") == 0) {
            builtin = exe_exit;
            // process_running = 0;
        } else if (strcmp(pcur->argv[0], "fg") == 0) {
            builtin = exe_fg;
            process_running = 0;
        } else if (strcmp(pcur->argv[0], "help") == 0) {
            builtin = exe_help;
        } else if (strcmp(pcur->argv[0], "jobs") == 0) {
            builtin = exe_jobs;
        } else if (strcmp(pcur->argv[0], "pwd") == 0) {
            builtin = exe_pwd;
        } else if (strcmp(pcur->argv[0], "set") == 0) {
            builtin = exe_set;
            process_running = 0;
        } else if (strcmp(pcur->argv[0], "shift") == 0) {
            builtin = exe_shift;
            process_running = 0;
        } else if (strcmp(pcur->argv[0], "test") == 0) {
            builtin = exe_test;
            process_running = 0;
        } else if (strcmp(pcur->argv[0], "time") == 0) {
            builtin = exe_time;
        } else if (strcmp(pcur->argv[0], "umask") == 0) {
            if (pcur->argv[1]) {
                builtin = exe_umask_set;
                process_running = 0;
            } else {
                builtin = exe_umask_disp;
            }
        } else if (strcmp(pcur->argv[0], "unset") == 0) {
            builtin = exe_unset;
            process_running = 0;
        } else {
            builtin = NULL;
        }

        if (!process_running) {
            /* 某些内建命令不适合用子进程 */
            res = builtin(pcur->argv);
        } else {
            pid = fork();
            if (pid < 0) {
                exception("Fail to create pipe\n");
            } else if (pid == 0) {
                // 子进程
                // 解析管道命令
                // 将标准输出重定向至管道写端，这样标准输出会写入管道中 
                if (pcur->next_op == OP_PIPE) {
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                }

                // 解析重定向
                set_iofd(pcur);

                // 解析内部命令
                // 执行普通命令
                if (builtin) {
                    res = builtin(pcur->argv);
                } else {
                    setenv("parent", g_xish_path, 1);
                    if (execvp(pcur->argv[0], pcur->argv) < 0) {
                        fprintf(stderr, "%s not found\n", pcur->argv[0]);
                        res = CMDR_ERR;
                    }
                }
                exit(res);
            } else {
                /* 更新jobs队列 */
                strcpy(cur_job->cmd, pcur->argv[0]);
                cur_job->pid = pid;
                cur_job->state = JOB_RUNNING;
                if (pcur->next_op != OP_BG) {
                    g_curfgjob = cur_job;
                    /* foreground运行的进程需要wait */
                    waitpid(pid, &res, WUNTRACED);
                    res = WEXITSTATUS(res);
                    if (builtin == exe_exec) {
                        /* exec 指令直接退出 */
                        res = CMDR_EXIT;
                    } else if (!builtin) {
                        /* 外部指令执行时会返回不可预期的返回码 */
                        /* 假定返回0正常，返回其他代表error */
                        res = res == 0 ? CMDR_OK : CMDR_ERR;
                    }

                    if (g_curfgjob) {
                        g_curfgjob->state = JOB_NONE;
                        g_curfgjob = NULL;
                    }
                }
            }
        }

        /* 将管道读端重定向至标准输入，这样管道数据会写入写一条指令的标准输入中，实现pipe的功能 */
        if (pcur->next_op == OP_PIPE) {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
        } else {
            reset_iofd();
        }
        /* 如果是退出指令或者条件失败，那么退出执行 */
        if ((pcur->next_op == OP_OR && res != CMDR_ERR) || 
            (pcur->next_op == OP_AND && res == CMDR_ERR) ||
            res == CMDR_EXIT) {
            break;
        }
        pcur = pcur->next;
    }
    return res;
}
