/*=====================================================================================
 * 1.JSON������ݽṹ
 * 2.ջ���ݽṹ
 * 3.�ڴ�غ��ڴ����Bin���ݽṹ
 * 4.ȫ�־�̬����
 * 5.��������
 *   (1) ������㺯��
 *   (2) ջBUCKET��������
 *   (3) �ڴ�ز�������
 *   (4) ����վ��������
 *   (5) Json�����������
 * 6.ʹ������
 *   (1) ��ȡJson�ı����γɿɹ������Json���ݽṹJSON��������ReadJson()������
 *   (2) ������Ҫ��ȡ���޸�Json���ݡ�������GetKVPNode()��GetValue()��GetValueFromArray()��
 *       StringToKVP()��SetValue()��AddOne()��DelOne()������
 *   (3) �ύ�޸ĺ��JSON���ݵ�ָ���ļ���������CommitToFile()������
 *   (4) �ͷ��ڴ棬�ر�Json�ļ���������CloseJson()������
 *=====================================================================================*/

#ifndef __READ_JSON__
#define __READ_JSON__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//����ʹ��ϵͳ-��ѡһ
//#define _WINDOWS_
#define _LINUX_
//��windows�µ���dllʱ�������߰�����ͷ�ļ�ʱ�����˺�
//#define _USE_DLL_

typedef int bool;
#define false 0
#define true 1

#define SAFE_FREE(x) if(x!=NULL){free(x);x=NULL;}

#define RAM_POOL_SIZE (2*1024*1024)      //�ڴ�ش�С-2M
#define ROW_BUFFER_SIZE (1024+3)         //�л����С-1024���ַ�+���з�'\r'+'\n'+'\0'

#define BUCKET_LEN 20   //�������߸߶�

//���з�
#ifdef _WINDOWS_
    #define BR ("\r\n")
#endif

#ifdef _LINUX_
    #define BR ("\n")
#endif

//Ԫ������
enum ELEMENT_TYPE
{
    KEY    = 0x01,  //��
    VALUE  = 0x2,   //ֵ����ֵnull
    NUMBER = 0x04,  //��ֵ
    BOOL   = 0x08,  //����ֵ����0��1��ʾ
    STRING = 0x10,  //�ַ���ֵ
    OBJECT = 0x20,  //����ֵ
    ARRAY  = 0x40   //����ֵ
};

/*==================== JSON������ݽṹ ====================*/
typedef struct Object Object;
typedef struct Object *P_Object;
typedef struct Array Array;
typedef struct Array *P_Array;
typedef struct KVPNode KVPNode;
typedef struct KVPNode *P_KVPNode;

//ֵ
typedef union
{
    float num;      //���֣�����
    char* str;      //�ַ���
    P_Object obj;   //����
    P_Array arr;    //����
}Value;

//����ֵ
struct Object
{
    P_KVPNode head;
    P_KVPNode tail;
    int count;      //��Push��ά��
};

//����ֵ����
typedef struct ValueList
{
    Value value;
    ELEMENT_TYPE type;
    struct ValueList* next;
}ValueList,*P_ValueList;

//����ֵ
struct Array
{
    P_ValueList head;
    P_ValueList tail;
    int count;      //��DealOneRow�е�','��֧�д���
};

//Json��㣬KVP:key-value pair
struct KVPNode
{
    char* key;
    Value value;
    ELEMENT_TYPE type;
    struct KVPNode* last;
    struct KVPNode* next;
    struct KVPNode* parent;
};

/*==================== ջ���ݽṹ ====================*/
//ջ
typedef struct
{
    P_KVPNode bucket[BUCKET_LEN];
    int top;
}Bucket,*P_Bucket;

/*============ �ڴ�غ��ڴ����Bin���ݽṹ ============*/
//�ڴ�ؽ��
typedef struct
{
    char *addr;       //�׵�ַ
    int size;         //�ܴ�С
    char *read_addr;  //����ַ��δ��
    char *write_addr; //д��ַ
    int data_len;     //���ô�С
}RAM_POOL,*P_RAM_POOL;

//������
typedef struct Bin
{
    char* addr;
    unsigned int size;
    struct Bin* lchild;
    struct Bin* rchild;
    struct Bin* parent;
}Bin,*P_Bin;

//����վ������Bin��
typedef struct
{
    P_Bin root;
    int count;
}BinStation,*P_BinStation;

/*===================== ȫ�ֱ��� =====================*/
//�ڴ��
#ifdef _USE_DLL_
__declspec(dllimport)
#endif
P_RAM_POOL POOL;
//����վ
#ifdef _USE_DLL_
__declspec(dllimport)
#endif
P_BinStation BIN_STATION;
//ջ
#ifdef _USE_DLL_
__declspec(dllimport)
#endif
P_Bucket BUCKET;
//Json�ļ����
#ifdef _USE_DLL_
__declspec(dllimport)
#endif
FILE* JSON_FILE;
//Json�洢����ͷ����ָ��Json���������ŵ�KVPNode
#ifdef _USE_DLL_
__declspec(dllimport)
#endif
P_KVPNode JSON;

/*===================== �������� =====================*/
/*==������㺯��==*/
static P_ValueList CreateOneValueList();
static P_Object CreateOneObject();
static P_Array CreateOneArray();
static P_KVPNode CreateOneKVP();
static P_Bucket CreateOneBucket();
static P_Bin CreateOneBin();
static P_BinStation CreateOneBinStation();

/*==ջBUCKET��������==*/
/*********************************************
 * ����
 * to_parent���Ƿ�Pop����ǰkvp��parent��ʶ
 * ����ֵ�����ɹ�������true�����򷵻�false��
 * ˵�����������ȡһ��Json����ֵ������Ķ�
 * BUCKET��kvp�ĵ�����������to_parent==true��
 * ���Pop����ǰkvp��parent�������ڷ������ж�
 * ��ֵ����to_parent==false�����Pop����ǰkvp
 * ����һ��kvp�������������еĶ���
 *********************************************/
static bool Pop(bool to_parent);

/*********************************************
 * ����
 * kvp��׼��ѹ��BUCKET�е�kvp��
 * ����ֵ�����ɹ�������true�����򷵻�false��
 * ˵����������BUCKET��ѹ��kvp�������kvp����
 * ǰһ��kvp�����ϼ�kvp֮��Ĺ�ϵ��
 *********************************************/
static bool Push(P_KVPNode kvp);

/*********************************************
 * ��������
 * ����ֵ���ޡ�
 * ˵�����ͷ�ջBUCKET��
 *********************************************/
static void BucketFree();

/*==�ڴ�ز�������==*/
/*********************************************
 * ����
 * size���ڴ�ش�С��
 * ����ֵ�����ɹ�������true�����򷵻�false��
 * ˵��������ָ��size��С���ڴ��POOL��
 *********************************************/
static bool CreateRAM_Pool(unsigned int size);

/*********************************************
 * ����
 * v���ַ���ֵ�׵�ַ
 * is_add��׷�ӱ�ʶ
 * ����ֵ�����سɹ�д���ڴ���ַ���������д��ʧ
 * �ܻ�д�벻�������򷵻�-1��
 * ˵�������Json���ݴ�������POOL��д���׷��
 * �ַ���д��ֵv��ֻд���ֵ�ַ�����STRING����
 * ֵ�������ݡ�
 *********************************************/
static int Pool_WriteData(char* v,bool is_add);

/*********************************************
 * ��������
 * ����ֵ���ޡ�
 * ˵�����ͷ��ڴ��POOL��
 *********************************************/
static void PoolFree();

/*==����վ��������==*/
/*********************************************
 * ����
 * bin�������������䣨����
 * size����Ҫ���ڴ��С
 * ����ֵ�����ɹ������������������ڴ���׵�ַ��
 * ��ʧ�ܣ�����NULL��
 * ˵������������binΪ���ڵ����������������һ
 * ���С���ʵ��ڴ档�����ڴ�С����size���ڴ棬
 * �򷵻ظ��ڴ��׵�ַ���������ڣ���ӱ�size��
 * ���ڴ���вü���size��С���ڴ沢���������
 * �����²�������վ�󣬷��زü������ڴ��׵�ַ��
 *********************************************/
static char* Bin_Search(P_Bin bin,const unsigned int size);

/*********************************************
 * ����
 * bin��Ҫ�����������䣨����
 * addr�����Ժϲ����ڴ��׵�ַ
 * size�����Ժϲ����ڴ��С
 * ����ֵ�����ɹ������غϲ�����ڴ��׵�ַ����
 * ʧ�ܣ�����NULL��
 * ˵�������Խ���СΪsize��addrָ����ڴ����
 * bin�е��ڴ��ϲ����ϲ���������bin�д��ڵ�
 * ĳһ���ָ���ڴ���β��ַ��addr���ڡ�����
 * ������ϲ���
 *********************************************/
static P_Bin AttemptToLeft(P_Bin bin,char* addr,unsigned int size);

/*********************************************
 * ����
 * bin��Ҫ�����������䣨����
 * addr�����Ժϲ����ڴ��׵�ַ
 * size�����Ժϲ����ڴ��С
 * ����ֵ�����ɹ������غϲ�����ڴ��׵�ַ����
 * ʧ�ܣ�����NULL��
 * ˵�������Խ���СΪsize��addrָ����ڴ����
 * bin�е��ڴ��ϲ����ϲ���������addrָ���
 * �ڴ���β��ַ��bin�д��ڵ�ĳһ���ָ���
 * �ڴ����׵�ַ���ڡ����������Һϲ���
 *********************************************/
static P_Bin AttemptToRight(P_Bin bin,char* addr,unsigned int size);

/*********************************************
 * ����
 * bin��Ҫ�����������䣨����
 * addr�����Ժϲ����ڴ��׵�ַ
 * size�����Ժϲ����ڴ��С
 * ����ֵ�����ɹ�������true�����򷵻�false��
 * ˵�������Խ���СΪsize��addrָ����ڴ����
 * bin�е��ڴ��ϲ����ֱ����������Һϲ���
 *********************************************/
static bool AttemptToMerge(P_Bin bin,char* addr,unsigned int size);

/*********************************************
 * ����
 * bin��Ҫ����������䣨����
 * addr��Ҫ����Ļ��������ڴ��ַ
 * size��Ҫ����Ļ��������ڴ��ַ��С
 * ����ֵ�����ɹ�������true�����򷵻�false��
 * ˵�������ȳ��Խ�addr��bin���ѻ��յ��ڴ�ϲ���
 * ���ɹ�������������������ɹ����򴴽��µ�Bin
 * ��㲢���뵽���к���λ�á����²���Ľ������
 * �н���С��ȣ���Ĭ�Ͻ��½ڵ���Ϊ��Ƚ���
 * ���ӽڵ���롣
 *********************************************/
static bool Bin_Insert(P_Bin bin,char* addr,unsigned int size);

/*********************************************
 * ����
 * root��Ҫ�����������䣨����
 * ����ֵ���ޡ�
 * ˵���������������������root����ӡÿ��bin��
 * ����ָ���ڴ���׵�ַ�ʹ�С��
 *********************************************/
void Bin_Show(P_Bin root);

/*********************************************
 * ����
 * root��Ҫ�����������䣨����
 * ����ֵ���ޡ�
 * ˵�����ͷ�������rootΪ����Bin��㡣
 *********************************************/
static void Bin_Free(P_Bin root);

/*********************************************
 * ��������
 * ����ֵ���ޡ�
 * ˵���������ͷŻ���վ��
 *********************************************/
static void BinStation_Free();

/*==Json�����������==*/
/*********************************************
 * ����
 * v����ת�������ַ���
 * result���洢ת�����
 * ����ֵ�����ɹ������ش�����ַ�������������
 * ����-1��
 * ˵�������Json�ı������������Խ��ַ���ת
 * ��float���͵����֡��ַ����г������ַ�,'+',
 * '-','e','E'��������ַ��������ԡ�
 *********************************************/
static int StringToNumber(char* v,float* result);

/*********************************************
 * ����
 * row����ȡJson��ȡ��Json�л���
 * ����ֵ�����ɹ�������true�����򷵻�false��
 * ˵���������ȡ��Json�ı���һ�����ݡ�������ֵ
 * ������֯��JSON�µ������������ַ������������
 * ��ֵ�����ֱ�ʶΪ':'����ͬ����ֵ�����ֱ�ʶΪ
 * ':'��ĵ�һ���ַ�����ֵ��KVP��ǰ���ϵ������
 * ����ϵ����֯����ʶ��',''}'']'��ʶ�𡣸���ϸ
 * ��������֯˵����ο�ReadMe.md˵���ĵ��洢��
 * ʽ�½ڡ�
 *********************************************/
static bool DealOneRow(char* row);

/*********************************************
 * ����
 * file��Json�ı�·��
 * ����ֵ�����ɹ�������true�����򷵻�false��
 * ˵������ʼ��POOL��BUCKET��BIN_STATION��JSON��
 * ��ȡ��Json�ı�������Json���ݡ�
 *********************************************/
bool ReadJson(char* file);

/*********************************************
 * ����
 * json��Ҫ������KVP���
 * ����ֵ�����ɹ�������ָ����KVP�����򷵻�NULL��
 * ˵������json�����������һ�������ļ�ֵΪkey
 * �Ľ�㲢���ء�
 *********************************************/
static P_KVPNode Json_Search(const P_KVPNode json,const char* key);

/*********************************************
 * ����
 * json��Ҫ������KVP���
 * pkey��Ҫ������KVP�ĸ��ڵ�KVP�ļ�
 * key��Ҫ������KVP�ļ�
 * ����ֵ�����ɹ�������ָ����KVP�����򷵻�NULL��
 * ˵������json������������ڵ��ֵΪpkey������
 * ��ֵΪkey�Ľ�㡣��pkeyΪNULL����ֱ����json
 * �����������ֵΪkey�Ľ�㲢���ء�
 *********************************************/
P_KVPNode GetKVPNode(const P_KVPNode json,char* pkey,char* key);

/*********************************************
 * ����
 * json��Ҫ������KVP���
 * pkey��ָ��Ҫ������KVP���ϼ�KVP�ļ�
 * key��ָ��Ҫ������KVP�ļ�
 * value���洢��������KVP��ֵ
 * ����ֵ���ޡ�
 * ˵������json���������ָ�������Ľ�㲢����
 * �ڵ��ֵ����value����������ͬ�ϡ�����������
 * �Դ����value���г�ʼ���Թ��������ý������
 * �ϣ�ͬʱ���б�֤value������������Ԥ��������
 * �Ľ��ֵ��ƥ�䡣����Ԥ����������ֵΪSTRING
 * ���ͣ������valueӦΪchar**���ͣ��Դ����ơ�
 *********************************************/
void GetValue(const P_KVPNode json,char* pkey,char* key,void* value);

/*********************************************
 * ����
 * arr��Ҫ������ֵΪARRAY���͵�KVP���
 * n��ָ��Ҫ����arr�еĵڼ���ֵ
 * value���洢��������KVP��ֵ
 * ����ֵ���ޡ�
 * ˵������������arr�еĵ�n��ֵ����value����
 * �������жԴ����value���г�ʼ���Թ���������
 * ������жϣ�ͬʱ���б�֤value������������Ԥ
 * ���������Ľ��ֵ��ƥ�䡣����Ԥ����������ֵΪ
 * STRING���ͣ������valueӦΪchar**���ͣ���
 * �����ơ�
 *********************************************/
void GetValueFromArray(const P_KVPNode arr,int n,void* value);

/*********************************************
 * ����
 * value��Ҫд����ַ���ֵ
 * pointer����¼д��value�ڴ���׵�ַ��ָ��
 * is_recycle���Ƿ���ʹ�û����ڴ��ʶ
 * ����ֵ��д��ֵ���ַ�������д��ʧ�ܻ�д�벻
 * �������򷵻�-1��
 * ˵�������ַ���ֵvalueд�뵽���ʵ��ڴ��в���
 * pointer��¼���ڴ���׵�ַ�����������ڴ�Ĳ�
 * ���ǣ�������ʹ�û����ڴ棬 ������ȳ������
 * ��վ�е��ڴ�дֵ�����ʧ�ܣ��������ڴ��POO-
 * L��дֵ����������ʹ�û����ڴ棬��ֱ����POOL
 * ��дֵ��
 *********************************************/
static int WriteStrToFit(char* value,char** pointer,bool is_recycle);

/*********************************************
 * ����
 * json_string��һ��������json��ֵ���ַ���
 * is_recycle���Ƿ���ʹ�û����ڴ��ʶ
 * ����ֵ�����ɹ�������ת��ΪKVPNode�ṹ��P_KV-
 * PNodeָ�룬��ʧ�ܣ�����NULL��
 * ˵������json_stringת��ΪP_KVPNode�ṹ��jso-
 * n_stringҪ��ѭ��һ�������� "key":value ��ʽ��
 * ��value�е���ʽ�����ƣ�������Ƕ�ס�����һ��
 * �ط�������󣬶�����NULL��
 *********************************************/
P_KVPNode StringToKVP(const char* json_string,bool is_recycle);

/*********************************************
 * ����
 * json��Ҫ������Json���
 * pkey��ָ���ĸ������ļ�
 * key��ָ���ļ�
 * value��Ҫ���õ���ֵ
 * vt��Ҫ���õ���ֵ������
 * is_recycle���Ƿ���ʹ�û����ڴ��ʶ�����ձ�
 * �滻����ֵ���ڴ�
 * ����ֵ�����ɹ�������true�����򷵻�false��
 * ˵������ָ��������KVP����ֵ��Ϊvt���͵�va-
 * lue����������ͬ�ϡ����������б�֤�����value
 * ��������vt�����value��ͨ������StringToKVP()
 * ��������,��������Ч����pool���뱾����Ĵ洢
 * �ṹ����һ�¡�boolֵ�Ļ���"True""true"��"F-
 * alse""false"��ʾNULLֵ��ֻ��֤value�ǿռ��ɡ�
 *********************************************/
bool SetValue(P_KVPNode json,const char* pkey,const char* key,const void* value,ELEMENT_TYPE vt,bool is_recycle);

/*********************************************
 * ����
 * json��Ҫ������Json���
 * pkey��ָ���ĸ������ļ�
 * last_key��ָ������һ�����ļ�
 * one��Ҫ������½ڵ�
 * ����ֵ�����ɹ�������true�����򷵻�false��
 * ˵������json�������������������ͬ�ϡ�����
 * ��last_key����������one��㡣
 *********************************************/
bool AddOne(P_KVPNode json,const char* pkey,const char* last_key,P_KVPNode one);

/*********************************************
 * ����
 * json��Ҫ������Json���
 * pkey��ָ���ĸ������ļ�
 * key��ָ������һ�����ļ�
 * is_recycle���Ƿ�����ڴ��ʶ
 * ����ֵ�����ɹ�������true�����򷵻�false��
 * ˵������json�����ɾ��ָ�������Ľ�㣬������
 * ��ͬ�ϡ�������is_recycle�����ڴ���չ�����
 *********************************************/
bool DelOne(P_KVPNode json,const char* pkey,const char* key,bool is_recycle);

/*********************************************
 * ����
 * json��Ҫ��ӡ��Json���
 * s�������󣬼�Json������ݵ�ȥ��
 * ����ֵ���ޡ�
 * ˵������Json����е��������ݴ�ӡ����s�С�
 *********************************************/
void JsonPrint(P_KVPNode json,FILE* s);

/*********************************************
 * ����
 * file���ļ�·��
 * ����ֵ�����ɹ�������true�����򷵻�false��
 * ˵����������JSON�е����ݴ�ӡ��file�ļ��С�
 *********************************************/
bool CommitToFile(char* file);

/*********************************************
 * ����
 * json��Ҫ�ͷŵ�Json���
 * is_recycle���Ƿ�����ͷŽ������ʹ�õ�POOL
 * �ڴ��ʶ
 * ����ֵ���ޡ�
 * ˵������json��㼰���ӽ���ͷŲ�����is_rec-
 * ycleָʾ����POOL�ڴ���չ�����
 *********************************************/
static void JsonFree(P_KVPNode json,const bool is_recycle);

/*********************************************
 * ��������
 * ����ֵ���ޡ�
 * ˵���������ͷ�JSON���ڴ��POOL������վBIN_-
 * STATION��ջBUCKET���ر�JSON_FILE�ļ��������
 * ��ȫ������������
 *********************************************/
void CloseJson();


#endif
