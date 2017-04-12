#include "read_json.h"


#ifdef _USE_DLL_
#pragma comment(lib,"read_json.lib")
#endif


int main()
{
    printf("-----\n");
    ReadJson("./json.json"); 
    JsonPrint(JSON,stdout);
    
    printf("-----------------------1\n");
    //获取person2中的key为id的kvp并打印
    P_KVPNode kvp = GetKVPNode(JSON,"person2","id");
    JsonPrint(kvp,stdout);
    
    printf("-----------------------2\n");
    //获取info数组中idid的值并打印
    char* str = NULL;
    GetValue(JSON,"info","idid",&str);
    if(str != NULL){ printf("%s\n",str); }
    else{ printf("GetValue error\n"); }
    
    printf("-----------------------3\n");
    //获取info数组中的第4个值（对象值）并打印
    P_KVPNode arr = GetKVPNode(JSON,NULL,"info");
    if(arr == NULL){ printf("arr is NULL\n"); }
    P_Object obj = NULL;
    GetValueFromArray(arr,4,&obj);
    if(obj == NULL){ printf("obj is NULL\n"); }
    else
    {
	kvp = obj->head;
	while(kvp != NULL)
	{
	    JsonPrint(kvp,stdout);
	    kvp = kvp->next;
	}
    }

    printf("-----------------------4\n");
    //设置把info的值（数组值）设置为10，该操作将回收66字节内存，因为
    //DealOneRow中处理info是连续的，因此这些回收的内存最终将合并为一
    //个Bin结点
    int info_value = 10;
    if(SetValue(JSON,"couple","info",&info_value,NUMBER,true) == false)
    { printf("SetValue error!\n"); }
    else{ JsonPrint(JSON,stdout); }
    //显示回收内存现状
    Bin_Show(BIN_STATION->root);
    //把最外层的JSON_FILE设置为字符串-失败
    if(SetValue(JSON,NULL,"JSON_FILE",&info_value,NUMBER,true) == false)
    { printf("SetValue error!\n"); }
    else{ JsonPrint(JSON,stdout); }

    printf("-----------------------5\n");
    printf("now POOL->write_addr:%p\n",POOL->write_addr);
    //将string_json转化为P_KVPNode并打印，该kvp中的字符串将耗费44字节
    //且全部使用回收内存
    char string_json[] 
	= "\"json_string\":{ \"one\":10,\"two\":{ \"tt\":true,\
	  \"hh\":\"haha\" },\"three\":[1,2,true,\"string\",NULL]}";
    P_KVPNode kvp_json = StringToKVP(string_json,true);
    if(kvp_json != NULL){ JsonPrint(kvp_json,stdout); }
    else{ printf("StringToKVP error\n"); }
    //再次显示回收内存现状和POOL的写指针
    printf("after StringToKVP POOL->write_addr:%p\n",POOL->write_addr);
    Bin_Show(BIN_STATION->root);

    printf("-----------------------6\n");
    //将string_json加入couple下的writing后
    if(AddOne(JSON,"couple","writing",kvp_json) == true){ JsonPrint(JSON,stdout); }
    else{ printf("AddOne error\n"); }
    
    printf("-----------------------7\n");
    //删除kvp_json，内存将再次回收，仍然是66个字节，一个Bin结点
    if(DelOne(JSON,"couple","json_string",true) == true){ JsonPrint(JSON,stdout); }
    else{ printf("DelOne error\n"); }
    Bin_Show(BIN_STATION->root);
    //删除writing结点
    if(DelOne(JSON,"couple","writing",true) == true){ JsonPrint(JSON,stdout); }
    else{ printf("DelOne error\n"); }
    Bin_Show(BIN_STATION->root);
    
    printf("-----------------------8\n");
    //提交改变到一个new.json中
    CommitToFile("new.json");
    
    //释放所有内存，关闭文件
    CloseJson();

    return 0;
}
