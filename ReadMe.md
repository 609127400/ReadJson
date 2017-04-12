# READ_JSON 插件
            
                    version：1.0
                    by Mr.Wang
                    2017年4月9日     
               
---

__目录__

1. 简介
2. 插件信息
3. 术语
4. 处理限制
5. 使用方法
6. 作者信息和Bug反馈

---


## 简介

本插件实现对json文件的读写，此版本只实现对标准json文档进行处理。由C语言实现，处理速度较快。本插件遵循GPL协议。

## 插件信息
    语言：C
    版本：1.0
    编译器：GCC
    运行环境：windows，linux

## 术语

    键：json的值的名称，也叫key值，键值。
    值：json中的值，有string，number，object，array，bool，null类型值。
    KVP：key-value pair，键值对。
    内存池：专门用于存储json的键和string类型值两种字符串数据。
    垃圾站：用于管理回收的内存块。
    行Buffer：存储读取json文件一行内容的内存。
    空白字符：本程序中只代表文本中的空格，tab键值两个非可见字符
    注：为叙述方便，下文中，键以key，值以value表示。

## 文件
+ ```read_json.h -- read_json头文件``` 
+ ```read_json.c -- read_json源码```
+ ```ReadMe.md -- read_json说明文档```
+ ```json.json -- 示例json文件```
+ ```new.json -- test.c实例新生成的json文件```
+ ```libread_json.so -- 封装的linux动态库```
+ ```read_json.def -- VisualStudio2015生成的windows编译动态库导出表```
+ ```read_json.lib -- VisualStudio2015生成的windows编译动态库接口```
+ ```read_json.dll -- 封装的windows动态库```
+ ```test.c --示例```

## 处理限制
1.文本限制
* 只保证正确处理ANSI编码格式的json文本。
* 单个key和key的双引号不能跨行。
* 数字值，布尔值，空值 自身不允许跨行。
* 数字类型值用float格式存储，遵循C对float的限制，使用者自行保证文本中的数字值不溢出。
* 布尔值在json文本中可用true/True/false/False表示，由于C89中没有bool类型，故在处理时用整形0和1表示。
* 数组类型的值中不允许嵌套数组类型的值。数组中的值最后一个值与]之间不能多加 , 号，否则数组中的值个数将多1。
* null值在文本中可用NULL/null表示。
* json文件key中和string值中的空白字符默认保留。
* 不验证json的合法性。虽然程序已经尽可能的提高容错性，但请使用者自行尽可能地保证所读写的json文件符合规范，不规范的json不保证能正确处理。

2.数值限制
* 内存池：2M——整个文档的key和string值，每个值字符串附加一个'\0'字符。合计最大为2M。
* 行Buffer大小：1024 byte。
* 栈高度：20个。

## 存储方法
存储内容：键，值。  
存储结构：栈，双向链表，搜索二叉树。
> __栈__  
通过压栈和弹栈，对json各个值之间前后关系，从属关系进行组织。栈由全局静态变量BUCKET管理。

> __双向链表__  
存储json数据内容，包括键和值。  
存储的基本结构为KVPNode：  
KVPNode->last/next/parent联系一个节点的前后关系和从属关系；  
KVPNode->value指向一个值，value为联合类型，可为P_Array，char*，P_Object，num类型值；  
KVPNode->type表明该KVP的值类型。  
P_Array：数组类型值，P_Array->head指向数组中的值链表的头，此链表为单向链表，结点为ValueList类型。  
P_Object：对象类型值，P_Object->head指向其子值链表的头，此链表为双向链表，结点为KVPNode类型。  
char*：字符串类型值。  
num：数字类型值，布尔类型值，空值null类型值。  
json数据由全局静态变量JSON管理。

> __搜索二叉树__  
存储回收的内存块。在调用SetValue()/DelOne()等修改JSON数据的函数中，被替换和删除的结点中所使用内存池中的内存块可以被回收到回收站中。回收站由全局静态变量BIN_STATION管理。BIN_STATION->root指向回收内存树的根节点，回收内存树中的结点结构为Bin。    
Bin结构：  
Bin->addr指向回收内存块，Bin->lchild/rchild/parent联系父子节点。

>
关于存储结构的更多详细信息请参看read_json.h文件。

## 使用方法
* 源码编译  
    源码中不涉及C99特征，根据所在系统，可直接将提供的libread_json.so放至所在系统默认动态库目录中，也可根据需要变更read_json.h中的系统宏定义，自己编译动态库。  
    * 编译动态库libread_json.so：  
    ```gcc -fPIC -shared -o libread_json.so read_json.c```  
    将编译的libread_json.so放至所在系统默认动态库目录中。  
    * 编译动态库read_json.dll：  
    本例中在windows下借助了VS2015软件进行编译。  
    ```新建项目->Win32项目->DLL，附加选项勾选空项目->完成```  
    ```添加read_json.h，read_json.c，read_json.def到项目中，在read_json.h开启_WINDOWS_（同时关闭_LINUX_宏）后编译即可```  
    * 编译例子test.c：  
    ```gcc test.c -L. -lread_json```  

    因read_json属于插件范畴，故read_json以动态库的形式呈现，所以read_json.c中并没有主函数，且原则上只对外提供非static修饰的函数。用户可自己修改并编译，文档结构十分简单，只有一个.c文件和一个同名的.h文件。用户可自行对read_json.h中的一些宏，主要是针对各种Buffer的大小进行配置。

* 函数调用  
请参考read_json.h文件对各个函数的说明和test.c文件对各个函数的使用。  
* 动态库调用  
    windows下调用read_json.dll：  
    * 将read_json.dll，read_json.lib，read_json.h放至源码文件目录中。  
    * 在源码文件中包含read_json.h头文件，并开启read_json.h中的_WINDOWS_（同时关闭_LINUX_宏），_USE_DLL_两个宏。

    linux下调用libread_json.so：  
    * 将read_json.h 放至源码文件目录中，将libread_json.so放至系统动态库中，一般是/lib下。
    * 在源码文件中包含read_json.h头文件，在编译源码文件时加上参数-read_json即可。

## 作者信息和Bug反馈  
王雅震，名不见经传，还未成为大牛，努力中...。相互学习，相互进步。个人检测有限，内存泄露检测方面，所使用的工具为valgrind，保证程序正常结束下释放所有人工分配的内存。若发现Bug，且您不嫌麻烦，还请将Bug的详细情况发邮件至 609127400@qq.com。
