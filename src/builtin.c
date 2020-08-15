#include "../inc/builtin.h"
#include "../inc/xish.h"

#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>


int exe_bg(char **argv) {
    if (!argv[1]) {
        fprintf(stderr, "Please specify pid\n");
        return CMDR_ERR;
    }
    int pid = atoi(argv[1]);
    pcont(pid);
    g_curfgjob = NULL;
    return CMDR_OK;
}

int exe_cd(char **argv) {
    char *dest = argv[1];
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

int exe_clear(char **argv) {
    fprintf(stdout, "\033[1H\033[2J\033[0m");
    return CMDR_OK;
}

int exe_dir(char **argv) {
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

int exe_echo(char **argv) {
    for (int i = 1; argv[i] != NULL; ++i) {
        fprintf(stdout, "%s ", parse_para(argv[i]));
    }
    fprintf(stdout, "\n");
    return CMDR_OK;
}

int exe_env(char **argv) {
    extern char **environ;
    char **envs = environ;
    while (*envs) {
        fprintf(stdout, "%s\n", *envs++);
    }
    return CMDR_OK;
}

int exe_exec(char **argv) {
    if (execvp(argv[1], &argv[1]) < 0) {
        fprintf(stderr, "%s not found\n", argv[1]);
        return CMDR_ERR;
    }
    return CMDR_ERR;
}

int exe_exit(char **argv) { return CMDR_EXIT; }

int exe_help(char **no_argv) {
    int ret;
    char *argv[3];
    argv[0] = (char *)malloc(sizeof(char) * 5);
    argv[2] = NULL;
    strcpy(argv[0], "less");
    argv[1] = (char *)malloc(sizeof(char) * (PATH_SIZE + 1));
    strcpy(argv[1], g_xish_dir);
    strcat(argv[1], "README.md");

    if (access(argv[1], F_OK & R_OK) == 0) {
        if (execvp("less", argv) < 0) {
            fprintf(stderr, "Fail in executing less.\n");
            ret = CMDR_ERR;
        }
        ret = CMDR_OK;
    } else {
        fprintf(stderr, "README.md doesn't exist\n");
        ret = CMDR_ERR;
    }
    return ret;
}

int exe_fg(char **argv) {
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

int exe_jobs(char **argv) {
    for (int i = 0; i < MAX_JOBS; ++i) {
        if (g_joblist[i].state != JOB_NONE) {
            int j = 0;
            fprintf(stdout, "%d %s %s\n", g_joblist[i].pid,
                g_joblist[i].state == JOB_RUNNING ? "[RUNNING]" : "[SUSPENDED]", g_joblist[i].cmd);
        }
    }
}

int exe_pwd(char **argv) {
    fprintf(stdout, "%s\n", getenv("PWD"));
    return CMDR_OK;
}

int exe_set(char **argv) {
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

int exe_shift(char **argv) {
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

int exe_test(char **argv) {
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

int exe_time(char **argv) {
    time_t timer = time(NULL);
    char buffer[100];
    strftime(buffer, 100, "Now it's %I:%M%p.", localtime(&timer));
    fprintf(stdout, "%s\n", buffer);
    return CMDR_OK;
}

int exe_umask_disp(char **argv) {
    mode_t a = 0000;
    mode_t old_umask = umask(a);
    fprintf(stdout, "%04o\n", old_umask);
    umask(old_umask);
    return CMDR_OK;
}

int exe_umask_set(char **argv) {
    mode_t new_mask;
    sscanf(argv[1], "%o", &new_mask);
    umask(new_mask);
    return CMDR_OK;
}

int exe_unset(char **argv) {
    argv++;
    while (*argv) {
        unsetenv(*argv);
        argv++;
    }
    return CMDR_OK;
}
