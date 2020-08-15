// 内建指令

#pragma once

typedef int(*BuiltInFunc)(char**);

int exe_bg(char **argv);
int exe_cd(char **argv);
int exe_clear(char **argv);
int exe_dir(char **argv);
int exe_echo(char **argv);
int exe_env(char **argv);
int exe_exec(char **argv);
int exe_exit(char **argv);
int exe_help(char **no_argv);
int exe_fg(char **argv);
int exe_jobs(char **argv);
int exe_pwd(char **argv);
int exe_set(char **argv);
int exe_shift(char **argv);
int exe_test(char **argv);
int exe_time(char **argv);
int exe_umask_disp(char **argv);
int exe_umask_set(char **argv);
int exe_unset(char **argv);
