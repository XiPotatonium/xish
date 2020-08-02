/**
 * 声明定义了执行ShellCmd链表的函数
 * 声明定义了内置指令的执行函数。
 */

#include "../inc/executor.h"

#include <dirent.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* 内建指令对应的函数 */
static int exe_bg(char **argv) {
    if (!argv[1]) {
        fprintf(stderr, "Please specify pid\n");
        return CMDR_ERR;
    }
    int pid = atoi(argv[1]);
    pcont(pid);
    g_curfgjob = NULL;
    return CMDR_OK;
}

static int exe_cd(char **argv) {
    char * dest = argv[1];
    if (argv[1] == NULL || strcmp(argv[1], "~") == 0) {
        // cd回HOME
        dest = getenv("HOME");
    }

    if (chdir(dest) != 0) {
        fprintf(stderr, "Fail to change directory\n");
        return CMDR_ERR;
    } else if (getcwd(g_cdpath, PATH_SIZE) == NULL) {
        fprintf(stderr, "Fail to get PWD\n");
        return CMDR_ERR;
    }
    setenv("PWD", g_cdpath, 1);
    return CMDR_OK;
}

static int exe_clear(char **argv) {
    fprintf(stdout, "\033[1H\033[2J\033[0m");
    return CMDR_OK;
}

static int exe_dir(char **argv) {
    char *path = argv[1] ? argv[1] : g_cdpath;
    DIR *dir = opendir(path);
    int ret = CMDR_OK;
    if (dir == NULL) {
        fprintf(stderr, "fail to open %s\n", path);
        return CMDR_ERR;
    } else {
        struct dirent *ptr = NULL;
        while ((ptr = readdir(dir)) != NULL) {
            if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0) {
                continue;
            } else {
                fprintf(stdout, "%s ", ptr->d_name);
            }
        }
        fprintf(stdout, "\n");
    }
    return ret;
}

static int exe_echo(char **argv) {
    for (int i = 1; argv[i] != NULL; ++i) {
        fprintf(stdout, "%s ", parse_para(argv[i]));
    }
    fprintf(stdout, "\n");
    return CMDR_OK;
}

static int exe_env(char **argv) {
    extern char **environ;
    char **envs = environ;
    while (*envs) {
        fprintf(stdout, "%s\n", *envs++);
    }
    return CMDR_OK;
}

static int exe_exec(char **argv) {
    if (execvp(argv[1], &argv[1]) < 0) {
        fprintf(stderr, "%s not found\n", argv[1]);
        return CMDR_ERR;
    }
    return CMDR_ERR;
}

static int exe_exit(char **argv) { return CMDR_EXIT; }

static int exe_help(char **no_argv) {
    int ret;
    char *argv[3];
    argv[0] = (char *)malloc(sizeof(char) * 5);
    argv[2] = NULL;
    strcpy(argv[0], "less");
    argv[1] = (char *)malloc(sizeof(char) * (PATH_SIZE + 1));
    strcpy(argv[1], g_xish_dir);
    strcat(argv[1], "manual");

    if (access(argv[1], F_OK & R_OK) == 0) {
        if (execvp("less", argv) < 0) {
            fprintf(stderr, "Fail in executing less.\n");
            ret = CMDR_ERR;
        }
        ret = CMDR_OK;
    } else {
        fprintf(stderr, "manual doesn't exist\n");
        ret = CMDR_ERR;
    }
    return ret;
}

static int exe_fg(char **argv) {
    int ret;
    if (!argv[1]) {
        fprintf(stderr, "Please specify pid\n");
        return CMDR_ERR;
    }
    int pid = atoi(argv[1]);
    pcont(pid);
    fprintf(stdout, "Running %s\n", g_curfgjob->cmd);
    pid = g_curfgjob->pid;
    waitpid(pid, NULL, WUNTRACED);
    return CMDR_OK;
}

static int exe_jobs(char **argv) {
    for (int i = 0; i < MAX_JOBS; ++i) {
        if (g_joblist[i].state != JOB_NONE) {
            int j = 0;
            fprintf(stdout, "%d %s %s\n", g_joblist[i].pid,
                    g_joblist[i].state == JOB_RUNNING ? "[RUNNING]" : "[SUSPENDED]", g_joblist[i].cmd);
        }
    }
}

static int exe_pwd(char **argv) {
    fprintf(stdout, "%s\n", getenv("PWD"));
    return CMDR_OK;
}

static int exe_set(char **argv) {
    Parameter *p = g_parlist;
    for (int i = 1; argv[i]; ++i) {
        if (!g_parlist) {
            g_parlist = p = (Parameter *)malloc(sizeof(Parameter));
            p->val = argv[i];
            p->next = NULL;
        } else if (p->next) {
            p = p->next;
            if (p->val) {
                free(p->val);
            }
            p->val = argv[i];
        } else {
            p->next = (Parameter *)malloc(sizeof(Parameter));
            p = p->next;
            p->val = argv[i];
            p->next = NULL;
        }
        argv[i] = NULL;
    }
    Parameter *prev = p;
    p = p->next;
    prev->next = NULL;
    while (p) {
        prev = p;
        p = p->next;
        if (prev->val) {
            free(prev->val);
        }
        free(prev);
    }
    return CMDR_OK;
}

static int exe_shift(char **argv) {
    if (g_parlist) {
        Parameter *p = g_parlist;
        g_parlist = g_parlist->next;
        if (p->val) {
            free(p->val);
        }
        free(p);
    }
    return CMDR_OK;
}

static int exe_test(char **argv) {
    int res = -1;
    int neg;
    char *v1;
    char *v2;
    argv++;
    while (*argv) {
        /* 如果不是第一次，那么这里是一个条件运算符 */
        if (res != -1) {
            if (strcmp(*argv, "-a") == 0) {
                if (res == 0) {
                    break;
                }
            } else if (strcmp(*argv, "-o") == 0) {
                if (res == 1) {
                    break;
                }
            } else {
                return CMDR_ERR;
            }
            ++argv;
        }
        /* 取反运算符 */
        neg = 0;
        if (*argv && **argv == '!') {
            neg = 1;
            ++argv;
        }
        /* 表达式 */
        if (*argv && (*argv)[0] == '-') {
            /* 一元判断表达式 */
            if (!*(argv + 1)) {
                return CMDR_ERR;
            }
            v1 = parse_para(*(argv + 1));
            if (strcmp(*argv, "-e") == 0) {
                /* -e 判断文件是否存在 */
                res = access(v1, F_OK) == 0;
            } else if (strcmp(*argv, "-r") == 0) {
                /* -x 判断文件是否可读 */
                res = access(v1, R_OK) == 0;
            } else if (strcmp(*argv, "-w") == 0) {
                /* -x 判断文件是否可写 */
                res = access(v1, W_OK) == 0;
            } else if (strcmp(*argv, "-x") == 0) {
                /* -x 判断文件是否可执行 */
                res = access(v1, X_OK) == 0;
            } else if (strcmp(*argv, "-n") == 0) {
                /* -n 判断字符串长度是否大于0 */
                res = strlen(v1) > 0;
            } else if (strcmp(*argv, "-z") == 0) {
                /* -z 判断字符串长度是否等于0 */
                res = strlen(v1) == 0;
            } else {
                /* 文件属性判断 */
                struct stat s;
                if (stat(v1, &s) != 0) {
                    return CMDR_ERR;
                }
                if (strcmp(*argv, "-d") == 0) {
                    /* -d 判断是否为文件夹 */
                    res = S_ISDIR(s.st_mode);
                } else if (strcmp(*argv, "-f") == 0) {
                    /* -d 判断是否为普通文件 */
                    res = S_ISREG(s.st_mode);
                } else if (strcmp(*argv, "-b") == 0) {
                    /* -d 判断是否为块特殊文件 */
                    res = S_ISBLK(s.st_mode);
                } else if (strcmp(*argv, "-c") == 0) {
                    /* -d 判断是否为字符特殊文件 */
                    res = S_ISCHR(s.st_mode);
                } else if (strcmp(*argv, "-L") == 0) {
                    /* -L 判断是否为符号链接文件 */
                    res = S_ISLNK(s.st_mode);
                } else {
                    return CMDR_ERR;
                }
            }
            argv += 2;
        } else {
            /* 二元判断表达式 */
            v1 = parse_para(*argv);
            ++argv;
            if (!*argv || !*(argv + 1)) {
                return CMDR_ERR;
            }
            v2 = parse_para(*(argv + 1));
            if (strcmp(*argv, "-eq") == 0) {
                /* 判断整数等于 */
                res = atoi(v1) == atoi(v2);
            } else if (strcmp(*argv, "-ge") == 0) {
                /* 判断整数大于等于 */
                res = atoi(v1) >= atoi(v2);
            } else if (strcmp(*argv, "-gt") == 0) {
                /* 判断整数大于 */
                res = atoi(v1) > atoi(v2);
            } else if (strcmp(*argv, "-le") == 0) {
                /* 判断整数小于等于 */
                res = atoi(v1) <= atoi(v2);
            } else if (strcmp(*argv, "-lt") == 0) {
                /* 判断整数小于 */
                res = atoi(v1) < atoi(v2);
            } else if (strcmp(*argv, "-ne") == 0) {
                /* 判断整数不等于 */
                res = atoi(v1) != atoi(v2);
            } else if (strcmp(*argv, "=") == 0) {
                /* 判断字符串等于 */
                res = strcmp(v1, v2) == 0;
            } else if (strcmp(*argv, "!=") == 0) {
                /* 判断字符串不等于 */
                res = strcmp(v1, v2) != 0;
            } else {
                return CMDR_ERR;
            }
            argv += 2;
        }
        res = neg ? !res : res;
    }
    return res ? CMDR_OK : CMDR_ERR;
}

static int exe_time(char **argv) {
    time_t timer = time(NULL);
    char buffer[100];
    strftime(buffer, 100, "Now it's %I:%M%p.", localtime(&timer));
    fprintf(stdout, "%s\n", buffer);
    return CMDR_OK;
}

static int exe_umask_disp(char **argv) {
    mode_t a = 0000;
    mode_t old_umask = umask(a);
    fprintf(stdout, "%04o\n", old_umask);
    umask(old_umask);
    return CMDR_OK;
}

static int exe_umask_set(char **argv) {
    mode_t new_mask;
    sscanf(argv[1], "%o", &new_mask);
    umask(new_mask);
    return CMDR_OK;
}

static int exe_unset(char **argv) {
    argv++;
    while (*argv) {
        unsetenv(*argv);
        argv++;
    }
    return CMDR_OK;
}

/* executor.h声明的execute函数定义 */
int execute(ShellCmd *pcur, int inter) {
    int res = CMDR_OK;
    int pipefd[2];
    pid_t pid;

    while (pcur) {
        /* 检查之前的jobs有没有结束 */
        Job *cur_job = NULL;
        for (int i = 0; i < MAX_JOBS; ++i) {
            if (g_joblist[i].state == JOB_RUNNING && kill(g_joblist[i].pid, 0) != 0) {
                g_joblist[i].state = JOB_NONE;
                /* 批处理不显示进程结束信息 */
                if (inter) {
                    fprintf(stdout, "Process %d done\n", g_joblist[i].pid);
                }
            }
            if (!cur_job && g_joblist[i].state == JOB_NONE) {
                cur_job = &g_joblist[i];
            }
        }
        if (pcur->next_op == OP_PIPE) {
            if (pipe(pipefd) < 0) {
                // exception
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
                /*解析管道命令
                将标准输出重定向至管道写端，这样标准输出会写入管道中 */
                if (pcur->next_op == OP_PIPE) {
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                }

                //解析重定向
                set_iofd(pcur);

                //解析内部命令
                //执行普通命令
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
            }
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
        /* 将管道读端重定向至标准输入，这样管道数据会写入写一条指令的标准输入中，实现pipe的功能 */
        if (pcur->next_op == OP_PIPE) {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
        } else {
            reset_iofd();
        }
        /* 如果是退出指令或者条件失败，那么退出执行 */
        if ((pcur->next_op == OP_OR && res != CMDR_ERR) || (pcur->next_op == OP_AND && res == CMDR_ERR) ||
            res == CMDR_EXIT) {
            break;
        }
        pcur = pcur->next;
    }
    return res;
}
