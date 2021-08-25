#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
	int32_t constructor;
	int32_t prev;
} SPSTreeElement;

enum Constants {
	MAX_PATH_SIZE = 32,
	MAX_SEARCH_SIZE = 256,
	MAX_TREE_SIZE = 2048
};
typedef enum {
	Ok=0,
	LogicError=1,
	BufferTooSmall=2
} ErrorCode;

static const char* errorString( ErrorCode errcode)
{
	static const char* ar[] = {
		"",
		"logic error",
		"internal buffer too small"
	};
	return ar[ errcode];
}
static void throwError( ErrorCode errcode, const char* msg)
{
	if (msg) {
		fprintf( stderr, "Exception %d: %s (%s)\n", (int)errcode, errorString( errcode), msg);
	} else {
		fprintf( stderr, "Exception %d: %s\n", (int)errcode, errorString( errcode));
	}
	exit( 1);
}

typedef size_t (*SPSGetFollow)( FollowElement* buf, void* context, int32_t type);
typedef int (*SPSMatch)( void* context, int32_t type);

typedef struct TypeConstructorPair
{
	int32_t type;
	int32_t constructor;
} TypeConstructorPair;

typedef struct ShortestPathSearchResult
{
	float weight;
	int32_t fromindex;
	size_t arsize;
	TypeConstructorPair ar[ MAX_PATH_SIZE];
	size_t arsize_alt;
	TypeConstructorPair ar_alt[ MAX_PATH_SIZE];
} ShortestPathSearchResult;

static size_t fillResult( TypeConstructorPair* ar, SPSTreeElement const* tree, int32_t ti)
{
	size_t ai = 0;
	while (ti >= 0)
	{
		if (ai >= MAX_PATH_SIZE) throwError( BufferTooSmall, 0);
		ar[ ai].type = tree[ti].type;
		ar[ ai].constructor = tree[ti].constructor;
		ti = tree[ti].prev;
	}
	size_t bn = (ti & ~1) /2;
	size_t bi = 0;
	for (; bi <= bn; ++bi)
	{
		// swap
		int32_t tt = ar[bi].type;
		int32_t cc = ar[bi].constructor;
		ar[bi].type = ar[bn].type;
		ar[bi].constructor = ar[bn].constructor;
		ar[bn].type = tt;
		ar[bn].constructor = cc;
	}
	return ai;
}
static int shortestPathSearch( ShortestPathSearchResult* result, SPSMatch match, int32_t* fromtype, size_t fromtypesize, SPSGetFollow getFollow, void* context)
{
	SPSTreeElement tree[ MAX_TREE_SIZE];
	SPSStackElement stk[ MAX_SEARCH_SIZE];
	FollowElement follow[ MAX_SEARCH_SIZE];
	size_t followsize = 0;
	size_t stksize = 0;
	size_t treesize = 0;
	size_t fi = 0;
	result->weight = 0.0;
	result->fromindex = -1;
	result->arsize = 0;
	result->arsize_alt = 0;
	memset( &stk, 0, sizeof(stk));
	for (; fi != fromtypesize; ++fi)
	{
		if (match( context, fromtype[ fi]))
		{
			result->fromindex = fi;
			return 1;
		}
		SPSTreeElement* te = &tree[ fi];
		{te->type = fromtype[ fi]; te->constructor = fi; te->prev = -1;}
		stk[ fi].idx = fi;
		stk[ fi].weight = 0.0;
	}
	stksize = fromtypesize;
	while (stksize > 0)
	{
		SPSStackElement* elem = &stk[ stksize-1];
		int32_t fotreeidx = elem->idx;
		followsize = getFollow( follow, context, tree[ fotreeidx].type);
		for (fi=0; fi<followsize; ++fi)
		{
			int32_t fotype = follow[ fi].type;
			float foweight = elem->weight + follow[ fi].weight;
			size_t si = stksize;
			for (; si > 0 && stk[ si-1].weight < foweight; --si){}
			size_t ti = fotreeidx;
			for (; tree[ti].type != fotype && tree[ti].prev >= 0; ti = tree[ti].prev){}
			if (tree[ti].type != fotype)
			{
				if (treesize == MAX_TREE_SIZE) throwError( BufferTooSmall, 0);
				if (stksize == MAX_SEARCH_SIZE) throwError( BufferTooSmall, 0);
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

				if (result->fromindex >= 0 && result->weight < foweight + 2E-24)
				{
					break;
				}
				if (match( context, fotype))
				{
					if (result->fromindex >= 0) {
						result->arsize_alt = fillResult( result->ar_alt, tree, treesize-1);
						result->ar_alt[0].constructor = -1;
						break;
					} else {
						result->arsize = fillResult( result->ar, tree, treesize-1);
						result->fromindex = result->ar_alt[0].constructor;
						result->ar_alt[0].constructor = -1;
						result->weight = foweight;
					}
				}
			}
		}
	}
	return (result->fromindex >= 0);
}

int main( int argc, char const* argv[])
{
	return 0;
}

