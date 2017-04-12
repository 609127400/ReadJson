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
    //��ȡperson2�е�keyΪid��kvp����ӡ
    P_KVPNode kvp = GetKVPNode(JSON,"person2","id");
    JsonPrint(kvp,stdout);
    
    printf("-----------------------2\n");
    //��ȡinfo������idid��ֵ����ӡ
    char* str = NULL;
    GetValue(JSON,"info","idid",&str);
    if(str != NULL){ printf("%s\n",str); }
    else{ printf("GetValue error\n"); }
    
    printf("-----------------------3\n");
    //��ȡinfo�����еĵ�4��ֵ������ֵ������ӡ
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
    //���ð�info��ֵ������ֵ������Ϊ10���ò���������66�ֽ��ڴ棬��Ϊ
    //DealOneRow�д���info�������ģ������Щ���յ��ڴ����ս��ϲ�Ϊһ
    //��Bin���
    int info_value = 10;
    if(SetValue(JSON,"couple","info",&info_value,NUMBER,true) == false)
    { printf("SetValue error!\n"); }
    else{ JsonPrint(JSON,stdout); }
    //��ʾ�����ڴ���״
    Bin_Show(BIN_STATION->root);
    //��������JSON_FILE����Ϊ�ַ���-ʧ��
    if(SetValue(JSON,NULL,"JSON_FILE",&info_value,NUMBER,true) == false)
    { printf("SetValue error!\n"); }
    else{ JsonPrint(JSON,stdout); }

    printf("-----------------------5\n");
    printf("now POOL->write_addr:%p\n",POOL->write_addr);
    //��string_jsonת��ΪP_KVPNode����ӡ����kvp�е��ַ������ķ�44�ֽ�
    //��ȫ��ʹ�û����ڴ�
    char string_json[] 
	= "\"json_string\":{ \"one\":10,\"two\":{ \"tt\":true,\
	  \"hh\":\"haha\" },\"three\":[1,2,true,\"string\",NULL]}";
    P_KVPNode kvp_json = StringToKVP(string_json,true);
    if(kvp_json != NULL){ JsonPrint(kvp_json,stdout); }
    else{ printf("StringToKVP error\n"); }
    //�ٴ���ʾ�����ڴ���״��POOL��дָ��
    printf("after StringToKVP POOL->write_addr:%p\n",POOL->write_addr);
    Bin_Show(BIN_STATION->root);

    printf("-----------------------6\n");
    //��string_json����couple�µ�writing��
    if(AddOne(JSON,"couple","writing",kvp_json) == true){ JsonPrint(JSON,stdout); }
    else{ printf("AddOne error\n"); }
    
    printf("-----------------------7\n");
    //ɾ��kvp_json���ڴ潫�ٴλ��գ���Ȼ��66���ֽڣ�һ��Bin���
    if(DelOne(JSON,"couple","json_string",true) == true){ JsonPrint(JSON,stdout); }
    else{ printf("DelOne error\n"); }
    Bin_Show(BIN_STATION->root);
    //ɾ��writing���
    if(DelOne(JSON,"couple","writing",true) == true){ JsonPrint(JSON,stdout); }
    else{ printf("DelOne error\n"); }
    Bin_Show(BIN_STATION->root);
    
    printf("-----------------------8\n");
    //�ύ�ı䵽һ��new.json��
    CommitToFile("new.json");
    
    //�ͷ������ڴ棬�ر��ļ�
    CloseJson();

    return 0;
}
