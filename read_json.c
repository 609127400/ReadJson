#include "read_json.h"

//行缓存
static char ROW[ROW_BUFFER_SIZE];
//当前处理的数据类型
static ELEMENT_TYPE dealing_vt;
//当前值是否处理完毕标识
static bool is_finished;
//用于Print中控制行缩进
static int tab = 0;

static P_ValueList CreateOneValueList()
{
    P_ValueList v = (P_ValueList)malloc(sizeof(ValueList));
    if(v == NULL){ printf("ValueList malloc failed\n"); return NULL; }
    memset(&(v->value),0,sizeof(Value));
    v->next = NULL;
    return v;
}

static P_Object CreateOneObject()
{
    P_Object obj = (P_Object)malloc(sizeof(Object));
    if(obj == NULL){ printf("Object malloc failed\n"); return NULL; }
    obj->head = NULL;
    obj->tail = NULL;
    obj->count = 0;
    return obj;
}

static P_Array CreateOneArray()
{
    P_Array arr = (P_Array)malloc(sizeof(Array));
    if(arr == NULL){ printf("Array malloc failed\n"); return NULL; }
    arr->head = NULL;
    arr->tail = NULL;
    arr->count = 0;
    return arr;
}

static P_KVPNode CreateOneKVP()
{
    P_KVPNode kvp = (P_KVPNode)malloc(sizeof(KVPNode));
    if(kvp == NULL){ printf("KVP malloc failed\n"); return NULL; }
    kvp->key = NULL;
    memset(&(kvp->value),0,sizeof(Value));
    kvp->type = 0;
    kvp->parent = NULL;
    kvp->last = NULL;
    kvp->next = NULL;
    return kvp;
}

static P_Bucket CreateOneBucket()
{
    P_Bucket b = (P_Bucket)malloc(sizeof(Bucket));
    if(b == NULL){ printf("Bucket malloc failed\n"); return false; }
    b->top = -1;
    return b;
}

static P_Bin CreateOneBin()
{
    P_Bin bin = (P_Bin)malloc(sizeof(Bin));
    if(bin == NULL){ printf("P_Bin malloc failed\n"); return NULL; }
    bin->addr = NULL;
    bin->size = 0;
    bin->lchild = NULL;
    bin->rchild = NULL;
    bin->parent = NULL;
    return bin;
}

static P_BinStation CreateOneBinStation()
{
    P_BinStation bs = (P_BinStation)malloc(sizeof(BinStation));
    if(bs == NULL){ printf("BinStation malloc falied\n"); return NULL; }
    bs->root = NULL;
    bs->count = 0;
    return bs;
}

static bool Pop(bool to_parent)
{
    if(BUCKET->top == -1){ return false; }
    while(BUCKET->bucket[(BUCKET->top)--]->type != OBJECT);
    if(to_parent == true)
    {
	while(BUCKET->top != -1 && BUCKET->bucket[BUCKET->top]->type != OBJECT)
	{ --(BUCKET->top); };
    }
    return true;
}

static bool Push(P_KVPNode kvp)
{
    if(BUCKET->top == (BUCKET_LEN - 1)){ printf("Bucket is full\n"); return false; }
    if(kvp == NULL){ return false; }
    
    if(BUCKET->top > -1)
    {
	P_KVPNode top_kvp = BUCKET->bucket[BUCKET->top];
	if(top_kvp->type == OBJECT)
	{
	    if(top_kvp->value.obj->head == NULL)
	    {
		top_kvp->value.obj->head = kvp;
		top_kvp->value.obj->tail = kvp;
	    }
	    else
	    {
		kvp->last = top_kvp->value.obj->tail;
		top_kvp->value.obj->tail->next = kvp;
		top_kvp->value.obj->tail = kvp;
	    }
	    if((dealing_vt & ARRAY) == 0){ ++(top_kvp->value.obj->count); }
	    kvp->parent = top_kvp;
	}
	else
	{
	    kvp->parent = top_kvp->parent;
	    kvp->last = top_kvp;
	    top_kvp->next = kvp;
	    top_kvp->parent->value.obj->tail = kvp;
	    if((dealing_vt & ARRAY) == 0){ ++(kvp->parent->value.obj->count); }
	}
    }
    
    ++(BUCKET->top);
    BUCKET->bucket[BUCKET->top] = kvp;
    return true;
}

static void BucketFree()
{
    SAFE_FREE(BUCKET);
}

static bool CreateRAM_Pool(unsigned int size)
{
    POOL = (P_RAM_POOL)malloc(sizeof(RAM_POOL));
    if(POOL == NULL){ printf("ram POOL malloc failed\n"); return false; }
    POOL->addr = (char*)malloc(size);
    if(POOL->addr == NULL){ SAFE_FREE(POOL); printf("ram POOL malloc failed\n"); return false; }
    POOL->size = size;
    POOL->read_addr = POOL->addr;
    POOL->write_addr = POOL->addr;
    POOL->data_len = 0;
    return true;
}

static int Pool_WriteData(char* v,bool is_add)
{
    if(v == NULL){ printf("Pool_WriteData data is NULL\n"); return 0; }
    
    int counter = 0;

    if(is_add == true)
    {
	--(POOL->write_addr);
    }
    while(*v != '"' && *v != '\0' && *v != '\r' && *v != '\n' && *v != EOF)
    {
	if(POOL->data_len == POOL->size)
	{
	    --(POOL->write_addr);
	    *(POOL->write_addr) = '\0';
	    printf("POOL is full\n");
	    return -1;
	}
	*(POOL->write_addr) = *v;
	++v; ++(POOL->write_addr);
	++counter; ++(POOL->data_len);
    }

    //维护内存池数据
    if(POOL->data_len < POOL->size)
    {
	*(POOL->write_addr) = '\0';
	(POOL->write_addr) += 1;
	POOL->data_len += 1;
    }
    else
    {
	printf("POOL is full\n");
	--(POOL->write_addr);
	*(POOL->write_addr) = '\0';
	++(POOL->write_addr);
    }
    
    return counter;
}

static void PoolFree()
{
    SAFE_FREE(POOL->addr);
    SAFE_FREE(POOL);
}

static char* Bin_Search(P_Bin bin,const unsigned int size)
{
    if(bin == NULL || size <= 0){ return NULL; }
    char* addr = NULL;
    P_Bin p_bin = bin;
    while(bin != NULL)
    {
	p_bin = bin;
	if(size == bin->size){ break; }
	else if(size < bin->size){ bin = bin->lchild; }
	else{ bin = bin->rchild; }
    }
    
    //未找到相等的结点
    if(bin == NULL)
    {
	if(size < p_bin->size)//bin = p_bin->lchild
	{
	    addr = p_bin->addr;
	    //printf("original-addr:%p,size:%u\n",p_bin->addr,p_bin->size);
	    //删除p_bin，组织关系
	    if(p_bin->parent != NULL)
	    {
		if(p_bin->parent->lchild == p_bin){ p_bin->parent->lchild = p_bin->rchild; }
		else{ p_bin->parent->rchild = p_bin->rchild; }
		if(p_bin->rchild != NULL){ p_bin->rchild->parent = p_bin->parent; }
	    }
	    else
	    {
		//p_bin为根节点
		BIN_STATION->root = p_bin->rchild;
		if(p_bin->rchild != NULL){ p_bin->rchild->parent = NULL; }
	    }
	    --(BIN_STATION->count);
	    //将多余的内存重新做一个Bin插入树中
	    Bin_Insert(BIN_STATION->root,p_bin->addr + size,p_bin->size - size);
	    SAFE_FREE(p_bin);
	}
	else
	{
	    bin = p_bin;
	    //追溯到可用的结点
	    while(p_bin->parent != NULL)
	    {
		if(p_bin == p_bin->parent->lchild){ break; }
		p_bin = p_bin->parent;
	    }
	    if(p_bin->parent == NULL)
	    {
		printf("there is no big enough bin\n");
		return NULL;
	    }
	    
	    addr = p_bin->parent->addr;
	    //printf("original-addr:%p,size:%u\n",p_bin->parent->addr,p_bin->parent->size);
	    //此时bin应为以p_bin->parent为root的左子树中最大值
	    char* rest_addr = (p_bin->parent->addr + size);
	    int rest_size = p_bin->parent->size - size;
	    p_bin->parent->addr = bin->addr;
	    p_bin->parent->size = bin->size;

	    //bin->parent一定存在
	    if(bin == bin->parent->lchild)
	    { bin->parent->lchild = bin->lchild; }
	    else{ bin->parent->rchild = bin->lchild; }
	    if(bin->lchild != NULL){ bin->lchild->parent = bin->parent; }
	    
	    --(BIN_STATION->count);
	    Bin_Insert(BIN_STATION->root,rest_addr,rest_size);
	    SAFE_FREE(bin);
	}
    }
    else
    {
	addr = bin->addr;
	//printf("original-addr:%p,size:%u\n",bin->addr,bin->size);
	p_bin = bin->lchild;
	
	if(p_bin != NULL)
	{
	    while(p_bin->rchild != NULL){ p_bin = p_bin->rchild; }
	    bin->addr = p_bin->addr;
	    bin->size = p_bin->size;

	    if(p_bin = p_bin->parent->lchild){ p_bin->parent->lchild = p_bin->lchild; }
	    else{ p_bin->parent->rchild = p_bin->lchild; }
	    if(p_bin->lchild != NULL){ p_bin->lchild->parent = p_bin->parent; }
	    SAFE_FREE(p_bin);
	}
	else
	{
	    if(bin->parent != NULL)
	    {
		if(bin == bin->parent->lchild){ bin->parent->lchild = bin->rchild; }
		else{ bin->parent->rchild = bin->rchild; }
		if(bin->rchild != NULL){ bin->rchild->parent = bin->parent; }
	    }
	    else
	    {
		BIN_STATION->root = bin->rchild;
		if(bin->rchild != NULL){ bin->rchild->parent = NULL; }
	    }
	    SAFE_FREE(bin);
	}
	--(BIN_STATION->count);
    }
    
    return addr;
}

static P_Bin AttemptToLeft(P_Bin bin,char* addr,unsigned int size)
{
    if(bin == NULL || addr == NULL || size <= 0){ return false; }
    P_Bin re = NULL;
    if((bin->addr + bin->size) == addr)
    {
	bin->size += size;
	
	//将bin移到合适的位置
	if(bin->lchild != NULL)
	{
	    P_Bin lmax = bin->lchild;
	    while(lmax->rchild != NULL){ lmax = lmax->rchild; }
	    if(lmax->parent != bin)
	    {
		if(lmax == lmax->parent->lchild){ lmax->parent->lchild = lmax->lchild; }
		else{ lmax->parent->rchild = lmax->lchild; }
		if(lmax->lchild != NULL){ lmax->lchild->parent = lmax->parent; }

		if(bin->parent != NULL)
		{
		    if(bin == bin->parent->lchild){ bin->parent->lchild = lmax; }
		    else{ bin->parent->rchild = lmax; }
		}
		else
		{
		    BIN_STATION->root = lmax;
		}
		lmax->parent = bin->parent;
		lmax->lchild = bin->lchild;
		lmax->rchild = bin->rchild;
	    }
	    else
	    {
		if(bin->parent != NULL)
		{
		    if(bin == bin->parent->lchild){ bin->parent->lchild = lmax; }
		    else{ bin->parent->rchild = lmax; }
		}
		else
		{
		    BIN_STATION->root = lmax;
		}
		lmax->parent = bin->parent;
		lmax->rchild = bin->rchild;
	    }
	}
	else
	{
	    if(bin->parent != NULL)
	    {
		if(bin == bin->parent->lchild){ bin->parent->lchild = bin->rchild; }
		else{ bin->parent->rchild = bin->rchild; }
		if(bin->rchild != NULL){ bin->rchild->parent = bin->parent; }
	    }
	    else
	    {
		BIN_STATION->root = bin->rchild;
		if(bin->rchild != NULL){ bin->rchild->parent = NULL; }
	    }
	}
	
	if(BIN_STATION->root == NULL)
	{
	    BIN_STATION->root = bin;
	}
	else
	{
	    P_Bin pbin = NULL;
	    P_Bin newroot = BIN_STATION->root;
	    while(newroot != NULL)
	    {
		pbin = newroot;
		if(bin->size < newroot->size){ newroot = newroot->lchild; }
		else if(bin->size > newroot->size){ newroot = newroot->rchild; }
		else{ break; }
	    }

	    if(newroot == NULL)
	    {
		if(bin->size < pbin->size){ pbin->lchild = bin; }
		else{ pbin->rchild = bin; }
		bin->lchild = NULL;
		bin->rchild = NULL;
		bin->parent = pbin;
	    }
	    else
	    {
		if(pbin->lchild != NULL){ pbin->lchild->parent = bin; }
		bin->lchild = pbin->lchild;
		pbin->lchild = bin;
		bin->parent = pbin;
		bin->rchild = NULL;
	    }
	}

	re = bin;
	return bin;
    }

    re = AttemptToLeft(bin->lchild,addr,size);
    if(re != NULL){ return re; }
    re = AttemptToLeft(bin->rchild,addr,size);
    return re;
}

static P_Bin AttemptToRight(P_Bin bin,char* addr,unsigned int size)
{
    if(bin == NULL || addr == NULL || size <= 0){ return false; }
    P_Bin re = NULL;
    if(bin->addr == (addr + size))
    {
	bin->addr = addr;
	bin->size += size;
	
	//将bin移到合适的位置
	if(bin->lchild != NULL)
	{
	    P_Bin lmax = bin->lchild;
	    while(lmax->rchild != NULL){ lmax = lmax->rchild; }
	    if(lmax->parent != bin)
	    {
		if(lmax == lmax->parent->lchild){ lmax->parent->lchild = lmax->lchild; }
		else{ lmax->parent->rchild = lmax->lchild; }
		if(lmax->lchild != NULL){ lmax->lchild->parent = lmax->parent; }

		if(bin->parent != NULL)
		{
		    if(bin == bin->parent->lchild){ bin->parent->lchild = lmax; }
		    else{ bin->parent->rchild = lmax; }
		}
		else
		{
		    BIN_STATION->root = lmax;
		}
		lmax->parent = bin->parent;
		lmax->lchild = bin->lchild;
		lmax->rchild = bin->rchild;
	    }
	    else
	    {
		if(bin->parent != NULL)
		{
		    if(bin == bin->parent->lchild){ bin->parent->lchild = lmax; }
		    else{ bin->parent->rchild = lmax; }
		}
		else
		{
		    BIN_STATION->root = lmax;
		}
		lmax->parent = bin->parent;
		lmax->rchild = bin->rchild;
	    }
	}
	else
	{
	    if(bin->parent != NULL)
	    {
		if(bin == bin->parent->lchild){ bin->parent->lchild = bin->rchild; }
		else{ bin->parent->rchild = bin->rchild; }
		if(bin->rchild != NULL){ bin->rchild->parent = bin->parent; }
	    }
	    else
	    {
		BIN_STATION->root = bin->rchild;
		if(bin->rchild != NULL){ bin->rchild->parent = NULL; }
	    }
	}
	
	if(BIN_STATION->root == NULL)
	{
	    BIN_STATION->root = bin;
	}
	else
	{
	    P_Bin pbin = NULL;
	    P_Bin newroot = BIN_STATION->root;
	    while(newroot != NULL)
	    {
		pbin = newroot;
		if(bin->size < newroot->size){ newroot = newroot->lchild; }
		else if(bin->size > newroot->size){ newroot = newroot->rchild; }
		else{ break; }
	    }

	    if(newroot == NULL)
	    {
		if(bin->size < pbin->size){ pbin->lchild = bin; }
		else{ pbin->rchild = bin; }
		bin->lchild = NULL;
		bin->rchild = NULL;
		bin->parent = pbin;
	    }
	    else
	    {
		if(pbin->lchild != NULL){ pbin->lchild->parent = bin; }
		bin->lchild = pbin->lchild;
		pbin->lchild = bin;
		bin->parent = pbin;
		bin->rchild = NULL;
	    }
	}

	re = bin;
	return bin;
    }

    re = AttemptToRight(bin->lchild,addr,size);
    if(re != NULL){ return re; }
    re = AttemptToRight(bin->rchild,addr,size);
    return re;
}

static bool AttemptToMerge(P_Bin bin,char* addr,unsigned int size)
{
    bool result = false;
    P_Bin lbin = NULL;
    P_Bin rbin = NULL;
    lbin = AttemptToLeft(bin,addr,size);
    if(lbin != NULL)
    {
	rbin = AttemptToRight(BIN_STATION->root,lbin->addr,lbin->size);
	//现有的结点与现有的结点合并成功
	if(rbin != NULL)
	{
	    //组织lbin的子节点和父节点的关系
	    if(lbin->lchild != NULL)
	    {
		rbin = lbin->lchild;
		while(rbin->rchild != NULL){ rbin = rbin->rchild; }
		lbin->addr = rbin->addr;
		lbin->size = rbin->size;

		if(rbin == rbin->parent->lchild){ rbin->parent->lchild = rbin->lchild; }
		else{ rbin->parent->rchild = rbin->lchild; }
		if(rbin->lchild != NULL){ rbin->lchild->parent = rbin->parent; }
		lbin = rbin;
	    }
	    else
	    {
		if(lbin->parent != NULL)
		{
		    if(lbin == lbin->parent->lchild){ lbin->parent->lchild = lbin->rchild; }
		    else{ lbin->parent->rchild = lbin->rchild; }
		    if(lbin->rchild != NULL){ lbin->rchild->parent = lbin->parent; }
		}
		else
		{
		    BIN_STATION->root = lbin->rchild;
		    if(lbin->rchild != NULL){ lbin->rchild->parent = NULL; }
		}
	    }
	    SAFE_FREE(lbin);
	    --(BIN_STATION->count);
	}
	result = true;
    }
    else
    {
	rbin = AttemptToRight(bin,addr,size);
	if(rbin != NULL){ result = true; }
    }
    return result;
}

static bool Bin_Insert(P_Bin bin,char* addr,unsigned int size)
{
    if(addr == NULL || size <= 0){ return false; }
    if(AttemptToMerge(bin,addr,size) == true){ return true; }
    
    P_Bin new = CreateOneBin();
    if(new == NULL){ return false; }
    new->addr = addr;
    new->size = size;

    if(BIN_STATION->root == NULL)
    {
	BIN_STATION->root = new;
	BIN_STATION->count = 1;
	return true;
    }

    P_Bin p_bin = NULL;
    while(bin != NULL)
    {
	p_bin = bin;
	if(size < bin->size){ bin = bin->lchild; }
	else if(size > bin->size){ bin = bin->rchild; }
	else{ if(bin->addr == addr){ return true; }else{ break; } }
    }
    
    //不相等
    if(bin == NULL)
    {
	if(p_bin == NULL){ SAFE_FREE(new); return false; }
	if(size < p_bin->size){ p_bin->lchild = new; }
	else{ p_bin->rchild = new; }
	new->parent = p_bin;
    }
    //相等
    else
    {
	if(bin->lchild != NULL){ bin->lchild->parent = new; }
	new->lchild = bin->lchild;
	bin->lchild = new;
	new->parent = bin;
    }
    ++(BIN_STATION->count);
    
    return true;
}

void Bin_Show(P_Bin root)
{
    if(root == NULL){ return; }
    printf("addr:%p,size:%u\n",root->addr,root->size);
    Bin_Show(root->lchild);
    Bin_Show(root->rchild);
}

static void Bin_Free(P_Bin root)
{
    if(root == NULL){ return; }
    Bin_Free(root->lchild);
    Bin_Free(root->rchild);
    SAFE_FREE(root);
}

static void BinStation_Free()
{
    Bin_Free(BIN_STATION->root);
    SAFE_FREE(BIN_STATION);
}

static int StringToNumber(char* v,float* result)
{
    if(v == NULL || result == NULL){ return -1; }
    int positive = 0;
    float nagative = 1.0f;
    float decimal = 0.0f;
    int E = 0;
    /*四个数位表示：正负标识位，小数状态位，科学计数状态位，科学计数正负位*/
    //0x0000 正整数
    //0x1000 负整数
    //0x0100 小数
    //0x0010 +科学记数
    //0x0011 -科学记数
    int status = 0x0000;
    int counter = 0;
    *result = 0.00f;
    while(*v != ',' && *v != ']' && *v != '}' && *v != '\r' && *v != '\n' && *v != '\0' && *v != EOF)
    {
	if(47 < *v && *v < 58)
	{
	    //小数状态
	    if(status & 0x0100)
	    {
		nagative *= 0.1f;
		decimal += (nagative * (*v - '0'));
	    }
	    //科学计数状态
	    else if(status & 0x0010)
	    {
		E = (E << 1) + (E << 3);
		E += (*v - '0');
	    }
	    //整数状态
	    else
	    {
		decimal *= 10;
		decimal += (*v - '0');
	    }
	}
	else if(*v == '.')
	{
	    status &= 0x1001;
	    status |= 0x0100;
	}
	else if(*v == 'e' || *v == 'E')
	{
	    status &= 0x1001;
	    status |= 0x0010;
	}
	else if(*v == '-')
	{
	    if(status & 0x0010){ status |= 0x0001; }
	    else{ status |= 0x1000; }
	}
	++counter;
	++v;
    }
    
    if(status & 0x0001){ E = 0 - E; }
    if(E > 0){ while(E > 0){ decimal *= 10; --E; } }
    else{ while(E < 0){ decimal *= 0.1f; ++E; } }
    if(status & 0x1000){ decimal = 0 - decimal; }
    
    *result = decimal;
    
    return counter;
}

static bool DealOneRow(char* row)
{
    int counter = 0;
    while((*row) != '\n' && (*row) != '\r' && (*row) != '\0' && (*row) != EOF)
    {
	if((*row) == ' ' || (*row) == 0x9)
	{
	    //do nothing，可以清除 值开头结尾的空白字符
	}
	else if((*row) == ':')
	{
	    if((dealing_vt & ARRAY) != 0){ dealing_vt = (ARRAY | OBJECT | VALUE); }
	    else{ dealing_vt = VALUE; }
	}
	else if((*row) == ',')
	{
	    if((dealing_vt & ARRAY) != 0)
	    {
		if((dealing_vt & OBJECT) != 0)
		{ dealing_vt = (ARRAY | OBJECT | KEY); }
		else
		{
		    BUCKET->bucket[BUCKET->top]->value.arr->count += 1;
		    dealing_vt = ARRAY;
		}
	    }
	    else{ dealing_vt = KEY; }
	}
	else if((*row) == '}')
	{
	    if((dealing_vt & ARRAY) != 0){ Pop(false); dealing_vt = ARRAY; }
	    else{ Pop(true); dealing_vt = KEY; }
	}
	else if((*row) == ']')
	{
	    BUCKET->bucket[BUCKET->top]->value.arr->count += 1;
	    dealing_vt = KEY;
	}
	else if((*row) == '"')//key或string值的开始"
	{
	    ++row;
	    if((dealing_vt & KEY) != 0)
	    {
		P_KVPNode kvp = CreateOneKVP();
		if(kvp == NULL){ return false; }
		kvp->key = POOL->write_addr;
		counter = Pool_WriteData(row,false);
		if(counter < 0){ SAFE_FREE(kvp); return false; }
		Push(kvp);
		row += counter;
	    }
	    else
	    {
		is_finished = false;
		if((dealing_vt & ARRAY) != 0)
		{
		    if((dealing_vt & OBJECT) != 0)
		    {
			dealing_vt = (ARRAY | OBJECT | STRING);
			BUCKET->bucket[BUCKET->top]->type = STRING;
			BUCKET->bucket[BUCKET->top]->value.str = POOL->write_addr;
			counter = Pool_WriteData(row,false);
		    }
		    else
		    {
			dealing_vt = (ARRAY | STRING);
			P_ValueList one = CreateOneValueList();
			if(one == NULL){ return false; }
			one->value.str = POOL->write_addr;
			one->type = STRING;
			counter = Pool_WriteData(row,false);
			if(BUCKET->bucket[BUCKET->top]->value.arr->head == NULL)
			{
			    BUCKET->bucket[BUCKET->top]->value.arr->head = one;
			    BUCKET->bucket[BUCKET->top]->value.arr->tail = one;
			}
			else
			{
			    BUCKET->bucket[BUCKET->top]->value.arr->tail->next = one;
			    BUCKET->bucket[BUCKET->top]->value.arr->tail = one;
			}
		    }
		}
		else
		{
		    dealing_vt = STRING;
		    BUCKET->bucket[BUCKET->top]->type = STRING;
		    BUCKET->bucket[BUCKET->top]->value.str = POOL->write_addr;
		    counter = Pool_WriteData(row,false);
		}
		if(counter  < 0){ return false; }
		row += counter;
		if((*row) == '"'){ is_finished = true; }
	    }
	}
	else if((*row) == '{')//object值
	{
	    if(JSON == NULL)
	    {
		JSON = CreateOneKVP();
		if(JSON == NULL){ return false; }
		P_Object obj = CreateOneObject();
		if(obj == NULL){ SAFE_FREE(JSON); return false; }
		JSON->key = POOL->write_addr;
		counter = Pool_WriteData("FILE-JSON\"",false);
		if(counter < 0){ SAFE_FREE(JSON); SAFE_FREE(obj); return false; }
		JSON->type = OBJECT;
		JSON->value.obj = obj;

		Push(JSON);
		dealing_vt = KEY;
	    }
	    else
	    {
		P_Object obj = CreateOneObject();
		if(obj == NULL){ return false; }
		if((dealing_vt & ARRAY) == 0)
		{
		    BUCKET->bucket[BUCKET->top]->type = OBJECT;
		    BUCKET->bucket[BUCKET->top]->value.obj = obj;
		    dealing_vt = KEY;
		}
		else//数组中的对象，必须开kvp，不然上下级无法通过push组织
		{
		    P_KVPNode kvp = CreateOneKVP();
		    if(kvp == NULL){ SAFE_FREE(obj); return false; }
		    kvp->key = POOL->write_addr;
		    counter = Pool_WriteData("ARRAY\"",false);
		    if(counter < 0){ SAFE_FREE(obj); SAFE_FREE(kvp); return false; }
		    kvp->type = OBJECT;
		    kvp->value.obj = obj;

		    P_ValueList one = CreateOneValueList();
		    if(one == NULL){ return false; }
		    one->value.obj = (P_Object)kvp;//将kvp伪装成转为obj存储
		    one->type = OBJECT;
		    if(BUCKET->bucket[BUCKET->top]->value.arr->head == NULL)
		    {
			BUCKET->bucket[BUCKET->top]->value.arr->head = one;
			BUCKET->bucket[BUCKET->top]->value.arr->tail = one;
		    }
		    else
		    {
			BUCKET->bucket[BUCKET->top]->value.arr->tail->next = one;
			BUCKET->bucket[BUCKET->top]->value.arr->tail = one;
		    }
		    
		    //Push必须放到将one插入数组值链表之后
		    Push(kvp);
		    dealing_vt = (ARRAY | OBJECT | KEY);
		}
	    }
	}
	else if((*row) == '[')//数组值
	{
	    dealing_vt = ARRAY;
	    BUCKET->bucket[BUCKET->top]->type = ARRAY;
	    P_Array arr = CreateOneArray();
	    if(arr == NULL){ return false; }
	    BUCKET->bucket[BUCKET->top]->value.arr = arr;
	}
	else//number，null值，string值，或array值
	{
	    if((dealing_vt & STRING) != 0)
	    {
		if(is_finished == false)
		{
		    counter = Pool_WriteData(row,true);
		    if(counter < 0){ return false; }
		    row += counter;
		    if((*row) == '"'){ is_finished = true; }
		}
	    }
	    else
	    {
		float result = 0;
		ELEMENT_TYPE vt;
		if((47 < (*row) && (*row) < 58) || *row == '+' || *row == '-')
		{
		    counter = StringToNumber(row,&result);
		    vt = NUMBER;
		}
		else if((*row) == 'T' || (*row) == 't')//true
		{
		    vt = BOOL;
		    result = 1;
		    counter = 4;
		}
		else if((*row) == 'F' || (*row) == 'f')//false
		{
		    vt = BOOL;
		    result = 0;
		    counter = 5;
		}
		else if((*row) == 'N' || (*row) == 'n')//null值
		{
		    vt = VALUE;
		    result = 0;
		    counter = 4;
		}
    
		//是数组中且不是对象中的值
		if((dealing_vt & ARRAY) != 0 && (dealing_vt & OBJECT) == 0)
		{
		    P_ValueList one = CreateOneValueList();
		    if(one == NULL){ return false; }
		    one->value.num = result;
		    one->type = vt;
		    if(BUCKET->bucket[BUCKET->top]->value.arr->head == NULL)
		    {
			BUCKET->bucket[BUCKET->top]->value.arr->head = one;
			BUCKET->bucket[BUCKET->top]->value.arr->tail = one;
		    }
		    else
		    {
			BUCKET->bucket[BUCKET->top]->value.arr->tail->next = one;
			BUCKET->bucket[BUCKET->top]->value.arr->tail = one;
		    }
		}
		else
		{
		    BUCKET->bucket[BUCKET->top]->type = vt;
		    BUCKET->bucket[BUCKET->top]->value.num = result;
		}
		if(counter < 0){ return false; }
		row += (counter - 1);
	    }
	}
	//处理完毕，向前走一个字节
	++row;
    }
    return true;
}

bool ReadJson(char* file)
{
    if(CreateRAM_Pool(RAM_POOL_SIZE) == false){ return false; }
    BIN_STATION = CreateOneBinStation();
    if(BIN_STATION == NULL){ SAFE_FREE(POOL->addr); SAFE_FREE(POOL); return false; }
    BUCKET = CreateOneBucket();
    if(BUCKET == NULL)
    { SAFE_FREE(POOL->addr); SAFE_FREE(POOL); SAFE_FREE(BIN_STATION); return false; }
    JSON = NULL;
    JSON_FILE = fopen(file,"r");
    if(JSON_FILE == NULL)
    {
	printf("open file failed\n");
	SAFE_FREE(POOL->addr);
	SAFE_FREE(POOL);
	SAFE_FREE(BIN_STATION);
	SAFE_FREE(BUCKET);
	return false;
    }
    
    //处理报文体
    while(fgets(ROW,ROW_BUFFER_SIZE,JSON_FILE) != NULL)
    {
	if(DealOneRow(ROW) == false)
	{
	    printf("DealOneRow error!\n");
	    return false;
	}
    }
    return true;
}

static P_KVPNode Json_Search(const P_KVPNode json,const char* key)
{
    if(json == NULL || key == NULL){ return NULL; }
    if(strcmp(json->key,key) == 0)
    {
	return json;
    }
    else
    {
	P_KVPNode re = NULL;
	P_KVPNode kvp = NULL;
	if(json->type == OBJECT)
	{
	    kvp = json->value.obj->head;
	    while(kvp != NULL)
	    {
		re = Json_Search(kvp,key);
		if(re != NULL){ break; }
		kvp = kvp->next;
	    }
	}
	else if(json->type == ARRAY)
	{
	    P_ValueList one = json->value.arr->head;
	    while(one != NULL)
	    {
		if(one->type == OBJECT)
		{
		    kvp = (P_KVPNode)(one->value.obj);
		    re = Json_Search(kvp,key);
		    if(re != NULL){ break; }
		}
		one = one->next;
	    }
	}
	return re;
    }
}

P_KVPNode GetKVPNode(const P_KVPNode json,char* pkey,char* key)
{
    if(json == NULL || key == NULL){ return NULL; }
    
    P_KVPNode pkvp = NULL;
    P_KVPNode kvp  = NULL;
    if(pkey != NULL)
    {
	pkvp = Json_Search(json,pkey);
	if(pkvp == NULL){ return NULL; }
	else
	{
	    kvp = Json_Search(pkvp,key);
	    return kvp;
	}
    }
    else
    {
	kvp = Json_Search(json,key);
	return kvp;
    }
}

void GetValue(const P_KVPNode json,char* pkey,char* key,void* value)
{
    if(json == NULL || key == NULL || value == NULL){ return; }
    
    P_KVPNode kvp = GetKVPNode(json,pkey,key);
    if(kvp != NULL)
    {
	if(kvp->type == OBJECT){ *((P_Object*)value) = kvp->value.obj; }
	else if(kvp->type == ARRAY){ *((P_Array*)value) = kvp->value.arr; }
	else if(kvp->type == STRING){ *((char**)value) = kvp->value.str; }
	else if(kvp->type == VALUE){ memset(value,0,sizeof(value)); }
	else{ *((float*)value) = kvp->value.num; }
    }
}

void GetValueFromArray(const P_KVPNode arr,int n,void* value)
{
    if(arr == NULL || n < 0 || value == NULL){ return; }
    if(arr->type != ARRAY){ return; }
    int i = 1;
    P_ValueList one = arr->value.arr->head;
    while(i < n && one != NULL)
    {
	++i;
	one = one->next;
    }
    if(i < n){ return; }

    if(one->type == OBJECT){ *((P_Object*)value) = ((P_KVPNode)(one->value.obj))->value.obj; }
    else if(one->type == STRING){ *((char**)value) = one->value.str; }
    else if(one->type == VALUE){ memset(value,0,sizeof(value)); }
    else{ *((float*)value) = one->value.num; }
}

static int WriteStrToFit(char* value,char** pointer,bool is_recycle)
{
    int counter = 0;
    if(is_recycle == true)
    {
	char* v = value;
	while(*v != '"' && *v != '\0' && *v != '\r' && *v != '\n' && *v != EOF)
	{ ++counter; ++v; }
	char* recycle = Bin_Search(BIN_STATION->root,(counter + 1));
	if(recycle != NULL)
	{
	    *pointer = recycle;
	    int i = 0;
	    while(i < counter)
	    {
		*recycle = *value;
		++recycle; ++value; ++i;
	    }
	    *recycle = '\0';
	}
	else
	{
	    //尝试获取bin内存失败，仍按写入pool内存池
	    *pointer = POOL->write_addr;
	    counter = Pool_WriteData(value,false);
	}
    }
    else
    {
	*pointer = POOL->write_addr;
	counter = Pool_WriteData(value,false);
    }
    return counter;
}

P_KVPNode StringToKVP(const char* json_string,bool is_recycle)
{
    if(json_string == NULL){ return NULL; }
    P_KVPNode root = NULL;

    char* row = (char*)json_string;
    int counter = 0;
    dealing_vt = KEY;

    while((*row) != '\0')
    {
	if((*row) == ' ' || (*row) == 0x9)
	{
	    //do nothing
	}
	else if((*row) == ':')
	{
	    if((dealing_vt & ARRAY) != 0){ dealing_vt = (ARRAY | OBJECT | VALUE); }
	    else{ dealing_vt = VALUE; }
	}
	else if((*row) == ',')
	{
	    if((dealing_vt & ARRAY) != 0)
	    {
		if((dealing_vt & OBJECT) != 0)
		{ dealing_vt = (ARRAY | OBJECT | KEY); }
		else
		{
		    BUCKET->bucket[BUCKET->top]->value.arr->count += 1;
		    dealing_vt = ARRAY;
		}
	    }
	    else{ dealing_vt = KEY; }
	}
	else if((*row) == '}')
	{
	    if((dealing_vt & ARRAY) != 0){ Pop(false); dealing_vt = ARRAY; }
	    else{ Pop(true); dealing_vt = KEY; }
	}
	else if((*row) == ']')
	{
	    BUCKET->bucket[BUCKET->top]->value.arr->count += 1;
	    dealing_vt = KEY;
	}
	else if((*row) == '"')//key或string值的开始"
	{
	    ++row;
	    if((dealing_vt & KEY) != 0)
	    {
		P_KVPNode kvp = CreateOneKVP();
		if(kvp == NULL){ goto StringToKVP_Clean_Label; }
		counter = WriteStrToFit(row,&(kvp->key),is_recycle);
		if(counter < 0){ SAFE_FREE(kvp); goto StringToKVP_Clean_Label; }
		Push(kvp);
		if(root == NULL){ root = kvp; }
	    }
	    else
	    {
		if((dealing_vt & ARRAY) != 0)
		{
		    if((dealing_vt & OBJECT) != 0)
		    {
			dealing_vt = (ARRAY | OBJECT | STRING);
			BUCKET->bucket[BUCKET->top]->type = STRING;
			counter = WriteStrToFit(row,&(BUCKET->bucket[BUCKET->top]->value.str),is_recycle);
		    }
		    else
		    {
			dealing_vt = (ARRAY | STRING);
			P_ValueList one = CreateOneValueList();
			if(one == NULL){ goto StringToKVP_Clean_Label; }
			one->type = STRING;
			counter = WriteStrToFit(row,&(one->value.str),is_recycle);
			
			if(BUCKET->bucket[BUCKET->top]->value.arr->head == NULL)
			{
			    BUCKET->bucket[BUCKET->top]->value.arr->head = one;
			    BUCKET->bucket[BUCKET->top]->value.arr->tail = one;
			}
			else
			{
			    BUCKET->bucket[BUCKET->top]->value.arr->tail->next = one;
			    BUCKET->bucket[BUCKET->top]->value.arr->tail = one;
			}
		    }
		}
		else
		{
		    dealing_vt = STRING;
		    BUCKET->bucket[BUCKET->top]->type = STRING;
		    counter = WriteStrToFit(row,&(BUCKET->bucket[BUCKET->top]->value.str),is_recycle);
		}
	    }
	    if(counter < 0){ goto StringToKVP_Clean_Label; }
	    row += counter;
	}
	else if((*row) == '{')//object值
	{
	    P_Object obj = CreateOneObject();
	    if(obj == NULL){ goto StringToKVP_Clean_Label; }
	    if((dealing_vt & ARRAY) == 0)
	    {
		BUCKET->bucket[BUCKET->top]->type = OBJECT;
		BUCKET->bucket[BUCKET->top]->value.obj = obj;
		dealing_vt = KEY;
	    }
	    else//数组中的对象，必须开kvp，不然上下级无法通过push组织
	    {
		P_KVPNode kvp = CreateOneKVP();
		if(kvp == NULL){ SAFE_FREE(obj); goto StringToKVP_Clean_Label; }
		counter = WriteStrToFit("ARRAY\"",&(kvp->key),is_recycle);
		if(counter < 0){ SAFE_FREE(obj); SAFE_FREE(kvp); goto StringToKVP_Clean_Label; }
		kvp->type = OBJECT;
		kvp->value.obj = obj;

		P_ValueList one = CreateOneValueList();
		if(one == NULL){ return root; }
		one->value.obj = (P_Object)kvp;//将kvp伪装成转为obj存储
		one->type = OBJECT;
		if(BUCKET->bucket[BUCKET->top]->value.arr->head == NULL)
		{
		    BUCKET->bucket[BUCKET->top]->value.arr->head = one;
		    BUCKET->bucket[BUCKET->top]->value.arr->tail = one;
		}
		else
		{
		    BUCKET->bucket[BUCKET->top]->value.arr->tail->next = one;
		    BUCKET->bucket[BUCKET->top]->value.arr->tail = one;
		}

		//Push必须放到将one插入数组值链表之后
		Push(kvp);
		dealing_vt = (ARRAY | OBJECT | KEY);
	    }
	}
	else if((*row) == '[')//数组值
	{
	    dealing_vt = ARRAY;
	    BUCKET->bucket[BUCKET->top]->type = ARRAY;
	    P_Array arr = CreateOneArray();
	    if(arr == NULL){ goto StringToKVP_Clean_Label; }
	    BUCKET->bucket[BUCKET->top]->value.arr = arr;
	}
	else//number，null值，string值，或array值
	{
	    float result = 0;
	    ELEMENT_TYPE vt;
	    if((47 < (*row) && (*row) < 58) || *row == '+' || *row == '-')
	    {
		counter = StringToNumber(row,&result);
		vt = NUMBER;
	    }
	    else if((*row) == 'T' || (*row) == 't')//true
	    {
		vt = BOOL;
		result = 1;
		counter = 4;
	    }
	    else if((*row) == 'F' || (*row) == 'f')//false
	    {
		vt = BOOL;
		result = 0;
		counter = 5;
	    }
	    else if((*row) == 'N' || (*row) == 'n')//null值
	    {
		vt = VALUE;
		result = 0;
		counter = 4;
	    }

	    //是数组中且不是对象中的值
	    if((dealing_vt & ARRAY) != 0 && (dealing_vt & OBJECT) == 0)
	    {
		P_ValueList one = CreateOneValueList();
		if(one == NULL){ return false; }
		one->value.num = result;
		one->type = vt;
		if(BUCKET->bucket[BUCKET->top]->value.arr->head == NULL)
		{
		    BUCKET->bucket[BUCKET->top]->value.arr->head = one;
		    BUCKET->bucket[BUCKET->top]->value.arr->tail = one;
		}
		else
		{
		    BUCKET->bucket[BUCKET->top]->value.arr->tail->next = one;
		    BUCKET->bucket[BUCKET->top]->value.arr->tail = one;
		}
	    }
	    else
	    {
		BUCKET->bucket[BUCKET->top]->type = vt;
		BUCKET->bucket[BUCKET->top]->value.num = result;
	    }

	    if(counter < 0){ goto StringToKVP_Clean_Label; }
	    row += (counter - 1);
	}
	//处理完毕，向前走一个字节
	++row;
    }

    return root;

StringToKVP_Clean_Label:
    JsonFree(root,false);
    return NULL;
}

bool SetValue(P_KVPNode json,const char* pkey,const char* key,const void* value,ELEMENT_TYPE vt,bool is_recycle)
{
    if(json == NULL || key == NULL || value == NULL){ return false; }
    P_KVPNode pkvp = NULL;
    P_KVPNode kvp = NULL;
    if(pkey != NULL)
    {
	pkvp = Json_Search(json,pkey);
	if(pkvp != NULL){ kvp = Json_Search(pkvp,key); }
    }
    else
    {
	kvp = Json_Search(json,key);
    }
    
    if(kvp == NULL){ return false; }
    //最外层的KVP，kvp=JSON，不允许更换
    if(kvp->parent == NULL){ return false; }
    
    if(vt == OBJECT)
    {
	//kvp->parent一定不为NULL
	if(kvp->last != NULL)
	{
	    kvp->last->next = ((P_KVPNode)value);
	    ((P_KVPNode)value)->last = kvp->last;
	    ((P_KVPNode)value)->next = kvp->next;
	    if(kvp->next != NULL){ kvp->next->last = ((P_KVPNode)value); }
	    if(kvp->next == NULL){ kvp->parent->value.obj->tail = ((P_KVPNode)value); }
	    ((P_KVPNode)value)->parent = kvp->parent;
	}
	else
	{
	    kvp->parent->value.obj->head = ((P_KVPNode)value);
	    ((P_KVPNode)value)->next = kvp->next;
	    if(kvp->next != NULL){ kvp->next->last = ((P_KVPNode)value); }
	    if(kvp->next == NULL){ kvp->parent->value.obj->tail = ((P_KVPNode)value); }
	}
	JsonFree(kvp,is_recycle);
    }
    else if(vt == STRING || vt == NUMBER || vt == BOOL || vt == VALUE)
    {
	if(kvp->type == OBJECT)
	{
	    while(kvp->value.obj->head != NULL)
	    {
		//借用一下pkvp
		pkvp = kvp->value.obj->head;
		kvp->value.obj->head = kvp->value.obj->head->next;
		JsonFree(pkvp,is_recycle);
	    }
	    SAFE_FREE(kvp->value.obj);
	}
	else if(kvp->type == ARRAY)
	{
	    P_ValueList one;
	    while(kvp->value.arr->head != NULL)
	    {
		one = kvp->value.arr->head;
		kvp->value.arr->head = kvp->value.arr->head->next;
		if(one->type == OBJECT)
		{
		    pkvp = ((P_KVPNode)(one->value.obj))->value.obj->head;
		    while(pkvp != NULL)
		    {
			((P_KVPNode)(one->value.obj))->value.obj->head = pkvp;
			pkvp = pkvp->next;
			JsonFree(((P_KVPNode)(one->value.obj))->value.obj->head,is_recycle);
		    }
		    SAFE_FREE(((P_KVPNode)(one->value.obj))->value.obj);
		    pkvp = ((P_KVPNode)(one->value.obj));
		    if(is_recycle == true)
		    { Bin_Insert(BIN_STATION->root,pkvp->key,(strlen(pkvp->key) + 1)); }
		    SAFE_FREE(pkvp);
		}
		if(is_recycle == true && one->type == STRING)
		{ Bin_Insert(BIN_STATION->root,one->value.str,(strlen(one->value.str) + 1)); }
		SAFE_FREE(one);
	    }
	    SAFE_FREE(kvp->value.arr);
	}

	kvp->type = vt;
	if(vt == STRING){ WriteStrToFit((char*)value,&(kvp->value.str),is_recycle); }
	else if(vt == NUMBER){ kvp->value.num = *((float*)value); }
	else if(vt == BOOL)
	{
	    if(*((char*)value) == 'T' || *((char*)value) == 't'){ kvp->value.num = 1; }
	    else{ kvp->value.num = 0; }
	}
	else if(vt == VALUE){ kvp->value.num = 0; }
	
    }
    else
    {
	printf("unknown value_type\n");
    }
    return true;
}

bool AddOne(P_KVPNode json,const char* pkey,const char* last_key,P_KVPNode one)
{
    if(json == NULL || last_key == NULL || one == NULL){ return false; }
    P_KVPNode kvp = NULL;
    if(pkey != NULL)
    {
	kvp = Json_Search(json,pkey);
	kvp = Json_Search(kvp,last_key);
    }
    else
    {
	kvp = Json_Search(json,last_key);
    }
    if(kvp == NULL){ return false; }
    if(kvp->parent == NULL){ return false; }

    one->parent = kvp->parent;
    one->next = kvp->next;
    if(kvp->next != NULL){ kvp->next->last = one; }
    else{ kvp->parent->value.obj->tail = one; }
    kvp->next = one;
    one->last = kvp;
    ++(kvp->parent->value.obj->count);

    return true;
}

bool DelOne(P_KVPNode json,const char* pkey,const char* key,bool is_recycle)
{
    if(json == NULL || key == NULL){ return false; }
    P_KVPNode kvp = NULL;
    if(pkey != NULL)
    {
	kvp = Json_Search(json,pkey);
	kvp = Json_Search(kvp,key);
    }
    else
    {
	kvp = Json_Search(json,key);
    }
    if(kvp == NULL){ return false; }
    //根结点
    if(kvp->parent == NULL)
    {
	JsonFree(kvp,false);
	Bin_Free(BIN_STATION->root);
	JSON = NULL;
	POOL->write_addr = POOL->addr;
	return true;
    }

    if(kvp->last != NULL && kvp->next != NULL)
    {
	kvp->last->next = kvp->next;
	kvp->next->last = kvp->last;
    }
    else if(kvp->last == NULL && kvp->next != NULL)
    {
	kvp->next->last = NULL;
	kvp->parent->value.obj->head = kvp->next;
    }
    else if(kvp->last != NULL && kvp->next == NULL)
    {
	kvp->last->next = NULL;
	kvp->parent->value.obj->tail = kvp->last;
    }
    else
    {
	kvp->parent->value.obj->head = NULL;
	kvp->parent->value.obj->tail = NULL;
    }
    --(kvp->parent->value.obj->count);
    JsonFree(kvp,is_recycle);

    return true;
}

void JsonPrint(P_KVPNode json,FILE* s)
{
    if(json == NULL || s == NULL){ return; }
    
    int i = 0;
    if(dealing_vt != (OBJECT | ARRAY)){ while(i < tab){ fputc(0x9,s); ++i; } }
    
    //key
    if(json->parent != NULL)
    {
	if((dealing_vt != (OBJECT | ARRAY)) 
	|| (dealing_vt == (OBJECT | ARRAY) && json->type != OBJECT))
	{ fprintf(s,"\"%s\":",json->key); }	
    }
    
    //value
    if(json->type == OBJECT)
    {
	if((dealing_vt & ARRAY) == 0)
	{
	    fprintf(s,"%s",BR);
	    i = 0;
	    while(i < tab){ fputc(0x9,s); ++i; }
	}
	
	fputc('{',s);

	if((dealing_vt & ARRAY) == 0){ fprintf(s,"%s",BR); }
	
	P_KVPNode kvp = json->value.obj->head;
	while(kvp != NULL)
	{
	    ++tab;
	    JsonPrint(kvp,s);
	    --tab;
	    kvp = kvp->next;
	}

	if((dealing_vt & ARRAY) == 0)
	{
	    fprintf(s,"%s",BR);
	    i = 0;
	    while(i < tab){ fputc(0x9,s); ++i; }
	}
	
	fputc('}',s);
    }
    else if(json->type == ARRAY)
    {
	dealing_vt = ARRAY;
	fputc('[',s);
	P_ValueList list = json->value.arr->head;
	while(list != NULL)
	{
	    if(list->type == STRING)
	    {
		fprintf(s,"\"%s\"",list->value.str);
	    }
	    else if(list->type == NUMBER)
	    {
		fprintf(s,"%g",list->value.num);
	    }
	    else if(list->type == OBJECT)//kvp
	    {
		dealing_vt = (OBJECT | ARRAY);
		P_KVPNode kvp = (P_KVPNode)(list->value.obj);
		while(kvp != NULL)
		{
		    JsonPrint(kvp,s);
		    kvp = kvp->next;
		}
	    }
	    else if(list->type == BOOL)
	    {

		fprintf(s,"%s",(list->value.num == 1) ? "true" : "false");
	    }
	    else if(list->type == VALUE)
	    {
		fprintf(s,"%s","null");
	    }
	    else
	    {
		printf("unknown vt\n");
	    }
	    list = list->next;
	    //数组值之间的逗号
	    if(list != NULL){ fputc(',',s); }
	}
	fputc(']',s);
	dealing_vt = OBJECT;
    }
    else if(json->type == STRING)
    {
	fprintf(s,"\"%s\"",json->value.str);
    }
    else if(json->type == BOOL)
    {
	fprintf(s,"%s",(json->value.num == 1) ? "true" : "false");
    }
    else if(json->type == NUMBER)
    {
	fprintf(s,"%g",json->value.num);
    }
    else if(json->type == VALUE)
    {
	fprintf(s,"%s","null");
    }
    else
    {
	printf("unknown vt\n");
    }
    
    //kvp间的逗号
    if(json->parent != NULL && json->parent->value.obj->tail != json)
    {
	if((dealing_vt & ARRAY) == 0){ fprintf(s,",%s",BR); }
	else if(dealing_vt == (OBJECT | ARRAY) && json->type != OBJECT)
	{ fputc(',',s); }
    }
}

bool CommitToFile(char* file)
{
    if(file == NULL){ return false; }
    FILE *json = fopen(file,"w");
    if(json == NULL){ printf("open file failed\n"); return false; }
    JsonPrint(JSON,json);
    fclose(json);
    return true;
}

static void JsonFree(P_KVPNode json,const bool is_recycle)
{
    if(json == NULL){ return; }
    if(json->type == OBJECT)
    {
	P_KVPNode kvp = json->value.obj->head;
	while(kvp != NULL)
	{
	    json->value.obj->head = kvp;
	    kvp = kvp->next;
	    JsonFree(json->value.obj->head,is_recycle);
	}
	SAFE_FREE(json->value.obj);
    }
    else if(json->type == ARRAY)
    {
	P_ValueList one = json->value.arr->head;
	P_ValueList temp_one = NULL;
	while(one != NULL)
	{
	    if(one->type == OBJECT)
	    {
		P_KVPNode kvp = (P_KVPNode)(one->value.obj);
		P_KVPNode obj_kvp = kvp->value.obj->head;
		while(obj_kvp != NULL)
		{
		    kvp->value.obj->head = obj_kvp;
		    obj_kvp = obj_kvp->next;
		    JsonFree(kvp->value.obj->head,is_recycle);
		}
		SAFE_FREE(kvp->value.obj);
		if(is_recycle == true)
		{ Bin_Insert(BIN_STATION->root,kvp->key,(strlen(kvp->key) + 1)); }
		SAFE_FREE(kvp);
	    }
	    temp_one = one;
	    one = one->next;
	    if(is_recycle == true && temp_one->type == STRING)
	    { Bin_Insert(BIN_STATION->root,temp_one->value.str,(strlen(temp_one->value.str) + 1)); }
	    SAFE_FREE(temp_one);
	}
	SAFE_FREE(json->value.arr);
    }

    if(is_recycle == true)
    {
	if(json->type == STRING)
	{ Bin_Insert(BIN_STATION->root,json->value.str,(strlen(json->value.str) + 1)); }
	Bin_Insert(BIN_STATION->root,json->key,(strlen(json->key) + 1));
    }

    SAFE_FREE(json);
}

void CloseJson()
{
    JsonFree(JSON,false);
    PoolFree();
    BinStation_Free();
    BucketFree(BUCKET);
    fclose(JSON_FILE);
}

