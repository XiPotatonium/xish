#include "../inc/executor.h"
#include "../inc/xish.h"
#include "../inc/parser.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    int res = CMDR_OK;
    ShellCmd *list = NULL;
    if (argc == 1) {
        /* 从控制台输入中获得指令并执行 */
        init(1);
        while (res != CMDR_EXIT) {
            /* 采用控制符来设置控制台输出的字符串的颜色 */
            fprintf(stdout, "\033[32mxish\033[0m:\033[34m%s\033[0m$ ",
                    strcmp(g_cdpath, getenv("HOME")) == 0 ? "~" : g_cdpath);
            /* 读取控制台输入并解析生成命令链表 */
            fgets(g_line, MAX_LINE, stdin);
            ShellCmd *list = parse(g_line);
            res = execute(list, 1); /* 执行命令链表 */
            free_list(list);        /* 释放内存 */
        }
    } else {
        /* 从文本中按行读取指令并执行 */
        init(0);
        FILE *fp;
        long pos = 0;
        if (!fp) {
            fprintf(stderr, "Fail to open file %s\n", argv[1]);
        } else {
            while (res != CMDR_EXIT) {
                /* 文件不能常开，因为子进程结束会影响FILE*内容 */
                fp = fopen(argv[1], "r");
                fseek(fp, pos, SEEK_SET);
                fgets(g_line, MAX_LINE, fp);
                if (feof(fp)) {
                    break;
                }
                pos = ftell(fp);
                fclose(fp);
                ShellCmd *list = parse(g_line);
                res = execute(list, 0);
                free_list(list);
            }
        }
    }
    gbc();

    return 0;
}