/*=====================================================================================
 * 1.JSON结点数据结构
 * 2.栈数据结构
 * 3.内存池和内存回收Bin数据结构
 * 4.全局静态变量
 * 5.操作函数
 *   (1) 创建结点函数
 *   (2) 栈BUCKET操作函数
 *   (3) 内存池操作函数
 *   (4) 垃圾站操作函数
 *   (5) Json处理操作函数
 * 6.使用流程
 *   (1) 读取Json文本，形成可供处理的Json数据结构JSON。即调用ReadJson()函数。
 *   (2) 根据需要获取或修改Json数据。即调用GetKVPNode()，GetValue()，GetValueFromArray()，
 *       StringToKVP()，SetValue()，AddOne()，DelOne()函数。
 *   (3) 提交修改后的JSON数据到指定文件。即调用CommitToFile()函数。
 *   (4) 释放内存，关闭Json文件。即调用CloseJson()函数。
 *=====================================================================================*/

#ifndef __READ_JSON__
#define __READ_JSON__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//定义使用系统-多选一
//#define _WINDOWS_
#define _LINUX_
//在windows下调用dll时，调用者包含此头文件时开启此宏
//#define _USE_DLL_

typedef int bool;
#define false 0
#define true 1

#define SAFE_FREE(x) if(x!=NULL){free(x);x=NULL;}

#define RAM_POOL_SIZE (2*1024*1024)      //内存池大小-2M
#define ROW_BUFFER_SIZE (1024+3)         //行缓冲大小-1024个字符+换行符'\r'+'\n'+'\0'

#define BUCKET_LEN 20   //定义堆最高高度

//换行符
#ifdef _WINDOWS_
    #define BR ("\r\n")
#endif

#ifdef _LINUX_
    #define BR ("\n")
#endif

//元素类型
enum ELEMENT_TYPE
{
    KEY    = 0x01,  //键
    VALUE  = 0x2,   //值，空值null
    NUMBER = 0x04,  //数值
    BOOL   = 0x08,  //布尔值，用0和1表示
    STRING = 0x10,  //字符串值
    OBJECT = 0x20,  //对象值
    ARRAY  = 0x40   //数组值
};

/*==================== JSON结点数据结构 ====================*/
typedef struct Object Object;
typedef struct Object *P_Object;
typedef struct Array Array;
typedef struct Array *P_Array;
typedef struct KVPNode KVPNode;
typedef struct KVPNode *P_KVPNode;

//值
typedef union
{
    float num;      //数字，布尔
    char* str;      //字符串
    P_Object obj;   //对象
    P_Array arr;    //数组
}Value;

//对象值
struct Object
{
    P_KVPNode head;
    P_KVPNode tail;
    int count;      //在Push中维护
};

//数组值链表
typedef struct ValueList
{
    Value value;
    ELEMENT_TYPE type;
    struct ValueList* next;
}ValueList,*P_ValueList;

//数组值
struct Array
{
    P_ValueList head;
    P_ValueList tail;
    int count;      //在DealOneRow中的','分支中处理
};

//Json结点，KVP:key-value pair
struct KVPNode
{
    char* key;
    Value value;
    ELEMENT_TYPE type;
    struct KVPNode* last;
    struct KVPNode* next;
    struct KVPNode* parent;
};

/*==================== 栈数据结构 ====================*/
//栈
typedef struct
{
    P_KVPNode bucket[BUCKET_LEN];
    int top;
}Bucket,*P_Bucket;

/*============ 内存池和内存回收Bin数据结构 ============*/
//内存池结点
typedef struct
{
    char *addr;       //首地址
    int size;         //总大小
    char *read_addr;  //读地址，未用
    char *write_addr; //写地址
    int data_len;     //已用大小
}RAM_POOL,*P_RAM_POOL;

//垃圾箱
typedef struct Bin
{
    char* addr;
    unsigned int size;
    struct Bin* lchild;
    struct Bin* rchild;
    struct Bin* parent;
}Bin,*P_Bin;

//回收站，管理Bin树
typedef struct
{
    P_Bin root;
    int count;
}BinStation,*P_BinStation;

/*===================== 全局变量 =====================*/
//内存池
#ifdef _USE_DLL_
__declspec(dllimport)
#endif
P_RAM_POOL POOL;
//回收站
#ifdef _USE_DLL_
__declspec(dllimport)
#endif
P_BinStation BIN_STATION;
//栈
#ifdef _USE_DLL_
__declspec(dllimport)
#endif
P_Bucket BUCKET;
//Json文件句柄
#ifdef _USE_DLL_
__declspec(dllimport)
#endif
FILE* JSON_FILE;
//Json存储数据头，即指向Json最外层大括号的KVPNode
#ifdef _USE_DLL_
__declspec(dllimport)
#endif
P_KVPNode JSON;

/*===================== 操作函数 =====================*/
/*==创建结点函数==*/
static P_ValueList CreateOneValueList();
static P_Object CreateOneObject();
static P_Array CreateOneArray();
static P_KVPNode CreateOneKVP();
static P_Bucket CreateOneBucket();
static P_Bin CreateOneBin();
static P_BinStation CreateOneBinStation();

/*==栈BUCKET操作函数==*/
/*********************************************
 * 参数
 * to_parent：是否Pop到当前kvp的parent标识
 * 返回值：若成功，返回true，否则返回false。
 * 说明：负责处理读取一个Json对象值结束后的堆
 * BUCKET中kvp的弹出工作。当to_parent==true，
 * 则会Pop到当前kvp的parent，适用于非数组中对
 * 象值。当to_parent==false，则会Pop到当前kvp
 * 的上一个kvp，适用于数组中的对象。
 *********************************************/
static bool Pop(bool to_parent);

/*********************************************
 * 参数
 * kvp：准备压入BUCKET中的kvp。
 * 返回值：若成功，返回true，否则返回false。
 * 说明：负责向BUCKET中压入kvp并处理该kvp与其
 * 前一个kvp，其上级kvp之间的关系。
 *********************************************/
static bool Push(P_KVPNode kvp);

/*********************************************
 * 参数：无
 * 返回值：无。
 * 说明：释放栈BUCKET。
 *********************************************/
static void BucketFree();

/*==内存池操作函数==*/
/*********************************************
 * 参数
 * size：内存池大小。
 * 返回值：若成功，返回true，否则返回false。
 * 说明：创建指定size大小的内存池POOL。
 *********************************************/
static bool CreateRAM_Pool(unsigned int size);

/*********************************************
 * 参数
 * v：字符串值首地址
 * is_add：追加标识
 * 返回值：返回成功写入内存池字符个数。若写入失
 * 败或写入不完整，则返回-1。
 * 说明：配合Json数据处理函数向POOL中写入或追加
 * 字符串写入值v。只写入键值字符串和STRING类型
 * 值两种数据。
 *********************************************/
static int Pool_WriteData(char* v,bool is_add);

/*********************************************
 * 参数：无
 * 返回值：无。
 * 说明：释放内存池POOL。
 *********************************************/
static void PoolFree();

/*==垃圾站操作函数==*/
/*********************************************
 * 参数
 * bin：搜索的垃圾箱（树）
 * size：需要的内存大小
 * 返回值：若成功，返回搜索到合适内存的首地址，
 * 若失败，返回NULL。
 * 说明：在垃圾箱bin为根节点的垃圾箱树中搜索一
 * 块大小合适的内存。若存在大小等于size的内存，
 * 则返回该内存首地址，若不存在，则从比size大
 * 的内存块中裁剪出size大小的内存并将多余的内
 * 存重新插入垃圾站后，返回裁剪出的内存首地址。
 *********************************************/
static char* Bin_Search(P_Bin bin,const unsigned int size);

/*********************************************
 * 参数
 * bin：要遍历的垃圾箱（树）
 * addr：尝试合并的内存首地址
 * size：尝试合并的内存大小
 * 返回值：若成功，返回合并后的内存首地址，若
 * 失败，返回NULL。
 * 说明：尝试将大小为size的addr指向的内存块与
 * bin中的内存块合并，合并的条件：bin中存在的
 * 某一结点指向内存块的尾地址与addr相邻。即尝
 * 试向左合并。
 *********************************************/
static P_Bin AttemptToLeft(P_Bin bin,char* addr,unsigned int size);

/*********************************************
 * 参数
 * bin：要遍历的垃圾箱（树）
 * addr：尝试合并的内存首地址
 * size：尝试合并的内存大小
 * 返回值：若成功，返回合并后的内存首地址，若
 * 失败，返回NULL。
 * 说明：尝试将大小为size的addr指向的内存块与
 * bin中的内存块合并，合并的条件：addr指向的
 * 内存块的尾地址与bin中存在的某一结点指向的
 * 内存块的首地址相邻。即尝试向右合并。
 *********************************************/
static P_Bin AttemptToRight(P_Bin bin,char* addr,unsigned int size);

/*********************************************
 * 参数
 * bin：要遍历的垃圾箱（树）
 * addr：尝试合并的内存首地址
 * size：尝试合并的内存大小
 * 返回值：若成功，返回true，否则返回false。
 * 说明：尝试将大小为size的addr指向的内存块与
 * bin中的内存块合并。分别尝试向左向右合并。
 *********************************************/
static bool AttemptToMerge(P_Bin bin,char* addr,unsigned int size);

/*********************************************
 * 参数
 * bin：要插入的垃圾箱（树）
 * addr：要插入的回收来的内存地址
 * size：要插入的回收来的内存地址大小
 * 返回值：若成功，返回true，否则返回false。
 * 说明：首先尝试将addr与bin中已回收的内存合并，
 * 若成功，则操作结束，若不成功，则创建新的Bin
 * 结点并插入到树中合适位置。若新插入的结点与已
 * 有结点大小相等，则默认将新节点作为相等结点的
 * 做子节点插入。
 *********************************************/
static bool Bin_Insert(P_Bin bin,char* addr,unsigned int size);

/*********************************************
 * 参数
 * root：要遍历的垃圾箱（树）
 * 返回值：无。
 * 说明：中序遍历搜索二叉树root，打印每个bin结
 * 点所指向内存的首地址和大小。
 *********************************************/
void Bin_Show(P_Bin root);

/*********************************************
 * 参数
 * root：要遍历的垃圾箱（树）
 * 返回值：无。
 * 说明：释放所有以root为根的Bin结点。
 *********************************************/
static void Bin_Free(P_Bin root);

/*********************************************
 * 参数：无
 * 返回值：无。
 * 说明：清理释放回收站。
 *********************************************/
static void BinStation_Free();

/*==Json处理操作函数==*/
/*********************************************
 * 参数
 * v：待转化数字字符串
 * result：存储转化结果
 * 返回值：若成功，返回处理的字符串个数，否则
 * 返回-1。
 * 说明：配合Json文本处理函数，尝试将字符串转
 * 成float类型的数字。字符串中除数字字符,'+',
 * '-','e','E'外的其他字符将被忽略。
 *********************************************/
static int StringToNumber(char* v,float* result);

/*********************************************
 * 参数
 * row：读取Json获取的Json行缓存
 * 返回值：若成功，返回true，否则返回false。
 * 说明：处理读取的Json文本的一行数据。将键和值
 * 进行组织到JSON下的数据链。按字符逐个遍历，键
 * 和值的区分标识为':'，不同类型值的区分标识为
 * ':'后的第一个字符。键值对KVP的前后关系和上下
 * 级关系的组织靠标识符',''}'']'的识别。更详细
 * 的数据组织说明请参考ReadMe.md说明文档存储方
 * 式章节。
 *********************************************/
static bool DealOneRow(char* row);

/*********************************************
 * 参数
 * file：Json文本路径
 * 返回值：若成功，返回true，否则返回false。
 * 说明：初始化POOL，BUCKET，BIN_STATION，JSON，
 * 读取的Json文本并处理Json数据。
 *********************************************/
bool ReadJson(char* file);

/*********************************************
 * 参数
 * json：要遍历的KVP结点
 * 返回值：若成功，返回指定的KVP，否则返回NULL。
 * 说明：在json结点中搜索第一个遇到的键值为key
 * 的结点并返回。
 *********************************************/
static P_KVPNode Json_Search(const P_KVPNode json,const char* key);

/*********************************************
 * 参数
 * json：要遍历的KVP结点
 * pkey：要搜索的KVP的父节点KVP的键
 * key：要搜索的KVP的键
 * 返回值：若成功，返回指定的KVP，否则返回NULL。
 * 说明：在json结点中搜索父节点键值为pkey且自身
 * 键值为key的结点。若pkey为NULL，则直接在json
 * 结点中搜索键值为key的结点并返回。
 *********************************************/
P_KVPNode GetKVPNode(const P_KVPNode json,char* pkey,char* key);

/*********************************************
 * 参数
 * json：要遍历的KVP结点
 * pkey：指定要搜索的KVP的上级KVP的键
 * key：指定要搜索的KVP的键
 * value：存储搜索到的KVP的值
 * 返回值：无。
 * 说明：在json结点中搜索指定条件的结点并将该
 * 节点的值存入value。搜索策略同上。调用者自行
 * 对传入的value进行初始化以供函数调用结果的判
 * 断，同时自行保证value的数据类型与预期搜索到
 * 的结点值相匹配。即如预期搜索出的值为STRING
 * 类型，则传入的value应为char**类型，以此类推。
 *********************************************/
void GetValue(const P_KVPNode json,char* pkey,char* key,void* value);

/*********************************************
 * 参数
 * arr：要遍历的值为ARRAY类型的KVP结点
 * n：指定要返回arr中的第几个值
 * value：存储搜索到的KVP的值
 * 返回值：无。
 * 说明：将数组结点arr中的第n个值存入value。调
 * 用者自行对传入的value进行初始化以供函数调用
 * 结果的判断，同时自行保证value的数据类型与预
 * 期搜索到的结点值相匹配。即如预期搜索出的值为
 * STRING类型，则传入的value应为char**类型，以
 * 此类推。
 *********************************************/
void GetValueFromArray(const P_KVPNode arr,int n,void* value);

/*********************************************
 * 参数
 * value：要写入的字符串值
 * pointer：记录写入value内存的首地址的指针
 * is_recycle：是否尝试使用回收内存标识
 * 返回值：写入值的字符数。若写入失败或写入不
 * 完整，则返回-1。
 * 说明：将字符串值value写入到合适的内存中并用
 * pointer记录该内存的首地址。搜索合适内存的策
 * 略是：若允许使用回收内存， 则会优先尝试向回
 * 收站中的内存写值，如果失败，则再向内存池POO-
 * L中写值；若不允许使用回收内存，则直接向POOL
 * 中写值。
 *********************************************/
static int WriteStrToFit(char* value,char** pointer,bool is_recycle);

/*********************************************
 * 参数
 * json_string：一个完整的json键值对字符串
 * is_recycle：是否尝试使用回收内存标识
 * 返回值：若成功，返回转换为KVPNode结构的P_KV-
 * PNode指针，若失败，返回NULL。
 * 说明：将json_string转换为P_KVPNode结构，jso-
 * n_string要遵循单一且完整的 "key":value 格式，
 * 但value中的形式不限制，可随意嵌套。其中一处
 * 地方处理错误，都返回NULL。
 *********************************************/
P_KVPNode StringToKVP(const char* json_string,bool is_recycle);

/*********************************************
 * 参数
 * json：要遍历的Json结点
 * pkey：指定的父级结点的键
 * key：指定的键
 * value：要设置的新值
 * vt：要设置的新值的类型
 * is_recycle：是否尝试使用回收内存标识并回收被
 * 替换掉的值的内存
 * 返回值：若成功，返回true，否则返回false。
 * 说明：将指定条件的KVP结点的值改为vt类型的va-
 * lue。搜索策略同上。调用者自行保证传入的value
 * 的类型与vt相符。value可通过函数StringToKVP()
 * 函数产生,这样可有效利用pool并与本程序的存储
 * 结构保持一致。bool值的话用"True""true"或"F-
 * alse""false"表示NULL值的只保证value非空即可。
 *********************************************/
bool SetValue(P_KVPNode json,const char* pkey,const char* key,const void* value,ELEMENT_TYPE vt,bool is_recycle);

/*********************************************
 * 参数
 * json：要遍历的Json结点
 * pkey：指定的父级结点的键
 * last_key：指定的上一个结点的键
 * one：要加入的新节点
 * 返回值：若成功，返回true，否则返回false。
 * 说明：在json结点中搜索，搜索策略同上。查找
 * 出last_key并在其后插入one结点。
 *********************************************/
bool AddOne(P_KVPNode json,const char* pkey,const char* last_key,P_KVPNode one);

/*********************************************
 * 参数
 * json：要遍历的Json结点
 * pkey：指定的父级结点的键
 * key：指定的上一个结点的键
 * is_recycle：是否回收内存标识
 * 返回值：若成功，返回true，否则返回false。
 * 说明：在json结点中删除指定条件的结点，搜索策
 * 略同上。并根据is_recycle进行内存回收工作。
 *********************************************/
bool DelOne(P_KVPNode json,const char* pkey,const char* key,bool is_recycle);

/*********************************************
 * 参数
 * json：要打印的Json结点
 * s：流对象，即Json结点数据的去向
 * 返回值：无。
 * 说明：将Json结点中的所有数据打印到流s中。
 *********************************************/
void JsonPrint(P_KVPNode json,FILE* s);

/*********************************************
 * 参数
 * file：文件路径
 * 返回值：若成功，返回true，否则返回false。
 * 说明：将现有JSON中的数据打印到file文件中。
 *********************************************/
bool CommitToFile(char* file);

/*********************************************
 * 参数
 * json：要释放的Json结点
 * is_recycle：是否回收释放结点中所使用的POOL
 * 内存标识
 * 返回值：无。
 * 说明：将json结点及其子结点释放并依据is_rec-
 * ycle指示进行POOL内存回收工作。
 *********************************************/
static void JsonFree(P_KVPNode json,const bool is_recycle);

/*********************************************
 * 参数：无
 * 返回值：无。
 * 说明：依次释放JSON，内存池POOL，回收站BIN_-
 * STATION，栈BUCKET，关闭JSON_FILE文件句柄。进
 * 行全部的清理工作。
 *********************************************/
void CloseJson();


#endif
