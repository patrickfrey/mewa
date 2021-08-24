
typedef struct SPSStackElement {
	float weight;
	int32_t idx;
} SPSStackElement;

typedef struct FollowElement {
	float weight;
	int32_t type;
	int32_t constructor;
} FollowElement;

typedef struct SPSTreeElement {
	int32_t type;
	int32_t prev;
	int32_t constructor;
} SPSTreeElement;

#define MAX_SEARCH_SIZE 256
#define MAX_TREE_SIZE 2048

enum ErrorCode {
	Ok=0,
	LogicError=1,
	BufferTooSmall=2
};
static const char* errorString( ErrorCode errcode)
{
	static const char* ar[] = {
		"",
		"logic error",
		"internal buffer too small"
	};
	return ar[ errcode];
}
static void throwError( ErrorCode errcode, const char* msg=0)
{
	if (msg) {
		fprintf( stderr, "Exception %d: %s (%s)\n", (int)errcode, errorString( errcode), msg);
	} else {
		fprintf( stderr, "Exception %d: %s\n", (int)errcode, errorString( errcode));
	}
	exit( 1);
}

typedef size_t (*GetFollow)( FollowElement* buf, void* context, int32_t type);

typedef struct TypeConstructorPair
{
	int32_t type;
	int32_t constructor;
};

typedef struct ShortestPathSearchResult
{
	float weight;
	size_t arsize;
	TypeConstructorPair ar[ MAX_SEARCH_SIZE];
	size_t arsize_alt;
	TypeConstructorPair ar_alt[ MAX_SEARCH_SIZE];
};

static bool shortestPathSearch( int32_t desttype, int32_t* fromtype, size_t fromtypesize, GetFollow getFollow, void* getFollowContext)
{
	SPSTreeElement tree[ MAX_TREE_SIZE];
	SPSStackElement stk[ MAX_SEARCH_SIZE];
	FollowElement follow[ MAX_SEARCH_SIZE];
	size_t followsize = 0;
	size_t stksize = 0;
	size_t treesize = 0;
	bool stksel = false;
	size_t fi = 0;
	memset( &stk, 0, sizeof(stk));
	for (; fi != fromtypesize; ++fi)
	{
		SPSTreeElement* te = &stk[ fi];
		{te->type = fromtype[ fi]; te->weight = 0.0; te->prev = -1;}
	}
	stksize = fromtypesize;
	while (stksize > 0)
	{
		SPSStackElement* elem = &stk[ stksize-1];
		int32_t fotreeidx = elem->idx;
		followsize = getFollow( follow, getFollowContext, elem->type);
		for (fi=0; fi<followsize; ++fi)
		{
			int32_t fotype = follow[ fi].type;
			float foweight = elem->weight + follow[ fi].weight;
			size_t si = stksize;
			for (; si > 0 && stk[ si-1].weight < foweight; --si){}
			size_t ti = elem->idx;
			for (; tree[ti].type != fotype && tree[ti].idx >= 0; ti = tree[ti].idx){}
			if (tree[ti].type != fotype)
			{
				if (treesize == MAX_TREE_SIZE) throwError( BufferTooSmall);
				if (stksize == MAX_SEARCH_SIZE) throwError( BufferTooSmall);
				tree[ treesize].prev = fotreeidx;
				tree[ treesize].type = fotype;
				tree[ treesize].constructor = follow[ fi].constructor;
				size_t sn = stksize;
				for (; sn != si; --sn)
				{
					stk[ sn].weight = stk[ sn-1].weight;
					stk[ sn].idx = stk[ sn-1].idx;
				}
				stk[ si].weight = foweight;
				stk[ si].idx = treesize;
				++treesize;
				++stksize;

				if (fotype == desttype)
				{

				}
			}
		}
	}
}

