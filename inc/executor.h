/**
 * 模块：executor。文件：executor.h executor.c
 * 声明定义了执行ShellCmd链表的函数
 * 声明定义了内置指令的执行函数。
 */

#pragma once

#include "../inc/xish.h"

/**
 * execute: 顺序执行list中的指令
 * 参数 list: 指令链表
 * 参数 inter: 交互型xish选1，执行批处理选0，批处理将不会提示后台进程结束
 * 模板中的解析内部命令，解析管道命令，解析重定向，执行普通命令全部在这个函数中实现
 * 由于每一种命令的执行代码长度不长但是很难分离，拆开会造成很多重复代码，
 * 所以在同一个函数中实现
 */
int execute(ShellCmd *list, int inter);
