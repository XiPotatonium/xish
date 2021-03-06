# XISH

BASH like shell.

## TODO

* 命令使用线程的方式实现而不是进程
* 交互式获取输入用getch手搓一个，而不是使用fgets，防止输出不可打印的字符
* parser不用等到解析完整个句子再开始执行，使用类似XiLang那样的buffer方式
* 变量的处理，${HOME} $HOME，可以暂时不考虑定义变量

## 1. IO重定向

支持重定向符号：> >> <。支持准输入重定向、标准输出重定向、标准错误输出重定向。
其中标准输出重定和标准错误输出重定向向还可分为普通输出重定向和追加输出重定向。
    
* 标准输入重定向可以将标准输入重定向到其他文件上。
    * 例如cat < readme就会将文本文件readme的内容作为标准输入传给cat，cat将打印readme的内容
* 标准输出重定向可以将指令的输出重定向到其他文件中
    * 例如echo "Hello world" 2> a.txt会创建a.txt并将echo的输出Hello world添加到文件中，如果a.txt存在那么会清空原有内容
    * 可以通过echo "Hello world" 2>> a.txt将echo的输出追加到a.txt的文件尾，这样不会清空原有内容
* 标准错误输出重定向会在指令发生错误时将错误信息输出重定向至其他文件中，用法类似标准输出重定向。

## 2. 管道

管道 | 可以实现多条指令之间的数据传输。
管道会将上一条指令的标准输出作为下一条指令的标准输入。
    
事实上管道是一种特殊的重定向，它将上一个指令的输出重定向到下一个指令的输入。
在程序中管道的实现和IO重定向的实现是类似的。

## 3. 程序环境

环境包括系统环境和程序自带的环境。
系统环境由系统给出，xish中可以通过env来打印系统环境变量，通过unset来删除系统环境变量。

程序自带环境是xish的全局变量中维护的环境变量。xish程序实现了bash中的命令行参数。
可以通过set, shift指令来修改程序自带的环境变量。

## 4. 指令后台执行

& 指令后台执行，多条指令后台运行体现出来的效果就是并行。
例如date & time，就会让在date初始化之后不等待date返回，直接执行time，由于date是外部命令而time是内建命令，往往time会比date先返回.
在实现中，程序会将后台执行的指令在一个子进程中执行，父进程直接返回交互界面，实现后台执行的功能。
执行的指令会被记入job数组中。程序会在指令执行之前检查之前的job是否完成，如果完成会产生输出。
因此后台执行的指令在结束后，会在下一次指令执行时通知用户。

指令后台执行的方法不仅有在指令之后增加&操作符，也可以通过前台指令转换。
例如，执行sleep 10，而后在sleep结束之前按下ctrl+z，就可以将sleep指令暂停并转移至后台。
这个时候输入jobs指令可以查找这个已暂停的sleep指令的pid。
再执行bg [pid]，就可以让这个sleep指令在后台执行。

xish中bg, cd, fg, set, shift, test, 带参数的umask, unset不支持后台
这些特殊指令需要修改全局变量或环境变量，不适合在子进程中执行，因此做了特殊处理，没有后台功能

## 5. 内建指令

内部指令由xish程序自己实现。
xish程序支持的内部指令有

```
·	bg [pid]
    将进程号pid对应的job转至后台执行
·	cd [dirname]
    切换工作目录，如果dirname为空或者为~，那么切换回HOME
·	clear
    清屏
·	dir [dirname]
    显示dirname指定的文件夹的内容
·	echo [string] …
    打印echo的参数
·	exec [progname]
    执行外部命令
·	exit
    退出xish
·	env
    打印环境变量
·	fg [pid]
    将进程号pid对应的job转至前台执行
·	help
    显示帮助文档(README.md)，注意README.md需要和可执行文件xish放在同一目录下，不然xish无法找到帮助文档
·	jobs
    显示job数组中正在运行或暂停的job
·	pwd
    打印当前工作目录
·	exit
    退出xish
·	set [arg1] …
    设置命令行参数，支持在test和echo中利用$1-$9和${xx}来引用命令行参数
·	shift
    左移命令行参数，将会丢弃第一个命令行参数。
·	test [expr]
    测试表达式的结果，如果表达式为true，那么指令执行成功，否则指令执行失败。支持以下测试：
    -e [filename]，文件是否存在，存在则返回true
    -r [filename]，文件是否可读，可读则返回true
    -w [filename]，文件是否则可写，可写则返回true
    -x [filename]，文件是否可执行，可执行则返回true
    -n [string]，字符串长度是否大于0，大于0则返回true
    -z [string]，字符串长度是否等于0，等于0则返回true
    [str1] = [str2]，字符串是否相等，等于则返回true
    [str1] != [str2]，字符串是否不等，不等则返回true
    -d [filename]，文件是否为文件夹，是文件夹则返回true
    -f [filename]，文件是否为普通文件，是普通文件则返回true
    -b [filename]，文件是否为块特殊文件，是块特殊文件则返回true
    -c [filename]，文件是否为字符特殊文件，是字符特殊文件则返回true
    -L [filename]，文件是否为符号链接文件，是符号链接文件则返回true
    [num1] –eq [num2]，两数是否相等，相等则返回true
    [num1] –ne [num2]，两数是否不等，不等则返回true
    [num1] –gt [num2]，num1是否大于等于num2，是则返回true
    [num1] –ge [num2]，num1是否大于num2，是则返回true
    [num1] –lt [num2]，num1是否小于等于num2，是则返回true
    [num1] –le [num2]，num1是否小于num2，是则返回true
    所有测试表达式可以通过-a(逻辑与)和-o(逻辑或)组合起来，表达式之前支持’!’来取反。
·	time
    输出当前时间
·	umask ([mask])
    显示当前文件创建权限掩码，如果指定了掩码，可以改变当前权限掩码。
·	unset [varname]
    删除varname对应的环境变量
```
