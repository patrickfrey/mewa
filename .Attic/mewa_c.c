#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct SPSStackElement {
	int32_t weight;
	int32_t idx;
} SPSStackElement;

typedef struct SPSFollowElement {
	int32_t weight;
	int32_t type;
	int32_t constructor;
} SPSFollowElement;

typedef struct SPSTreeElement {
	int32_t type;
	int32_t constructor;
	int32_t prev;
} SPSTreeElement;

enum {
	MAX_PATH_SIZE = 32,
	MAX_SEARCH_SIZE = 256,
	MAX_TREE_SIZE = 2048,
	IDENTBUF32_SIZE = 1<<16,
	HASHTABLE_SIZE = 1<<17,
	HASHTABLE_MASK = HASHTABLE_SIZE-1,
	SCOPEDMAP_BANK_SIZE = 63,
	SCOPEDMAP_SIZE = 2048,
	SCOPEDMAP_NODE_SIZE = 1<<17,
	MAX_AST_SIZE = 1<<16,
	MAX_PARSE_STACK_SIZE = 512
};
typedef enum {
	Ok=0,
	LogicError=1,
	BufferTooSmall=2,
	ValueOutOfRange=3,
	SyntaxError=21,
	UnexpectedEOF=22
} ErrorCode;

static const char* errorString( ErrorCode errcode) {
	static const char* ar[] = {
		"",
		"logic error",
		"internal buffer too small",
		"value out of range",
		"syntax error",
		"unexpected end of program source"
	};
	return ar[ errcode];
}
static void int2str( char* buf, int value, size_t* size) {
	size_t bn = 0;
	for (; value; ++bn,value/=10) {
		buf[bn]='0'+(value%10);
	}
	if (bn == 0) {
		buf[bn++] = '0';
	}
	size_t bi = 0, bm = (bn-1)/2;
	for (; bi != bm; ++bi) {
		char tmp = buf[ bi];
		buf[ bi] = buf[ bn - bi - 1];
		buf[ bn - bi - 1] = tmp;
	}
	buf[ bn] = 0;
	*size = bn;
}
static void throwError( ErrorCode errcode, int line) {
	char const* estr = errorString( errcode);
	size_t elen = strlen( estr);
	write( 2, "Exception: ", 11);
	write( 2, estr, elen);
	if (line) {
		char num[32];
		size_t numsize;
		write( 2, " on line ", 9);
		int2str( num, line, &numsize);
		write( 2, num, numsize);
	}
	write( 2, "\n", 1);
	exit( 1);
}
static void assertBufferSize( int32_t pos, int32_t size) {
	if (pos >= size) {
		throwError( BufferTooSmall, 0);
	}
}
typedef size_t (*SPSGetFollow)( SPSFollowElement* buf, void* context, int32_t type);
typedef int (*SPSMatch)( void* context, int32_t type);

typedef struct TypeConstructorPair {
	int32_t type;
	int32_t constructor;
} TypeConstructorPair;

typedef struct ShortestPathSearchResult {
	int32_t weight;
	int32_t fromindex;
	size_t arsize;
	size_t arsize_alt;
	TypeConstructorPair ar[ MAX_PATH_SIZE];
	TypeConstructorPair ar_alt[ MAX_PATH_SIZE];
} ShortestPathSearchResult;

static size_t fillSPSResult( TypeConstructorPair* ar, SPSTreeElement const* tree, int32_t ti) {
	size_t ai = 0;
	while (ti >= 0) {
		assertBufferSize( ai, MAX_PATH_SIZE);
		ar[ ai].type = tree[ti].type;
		ar[ ai].constructor = tree[ti].constructor;
		ti = tree[ti].prev;
	}
	size_t bn = (ti & ~1) /2;
	size_t bi = 0;
	for (; bi <= bn; ++bi) {
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
static int shortestPathSearch( ShortestPathSearchResult* result, SPSMatch match, int32_t* fromtype, size_t fromtypesize, SPSGetFollow getFollow, void* context) {
	SPSTreeElement tree[ MAX_TREE_SIZE];
	SPSStackElement stk[ MAX_SEARCH_SIZE];
	SPSFollowElement follow[ MAX_SEARCH_SIZE];
	size_t followsize = 0;
	size_t stksize = 0;
	size_t treesize = 0;
	size_t fi = 0;
	result->weight = 0.0;
	result->fromindex = -1;
	result->arsize = 0;
	result->arsize_alt = 0;
	memset( &stk, 0, sizeof(stk));
	for (; fi != fromtypesize; ++fi) {
		if (match( context, fromtype[ fi])) {
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
		for (fi=0; fi<followsize; ++fi) {
			int32_t fotype = follow[ fi].type;
			int32_t foweight = elem->weight + follow[ fi].weight;
			size_t si = stksize;
			for (; si > 0 && stk[ si-1].weight < foweight; --si){}
			size_t ti = fotreeidx;
			for (; tree[ti].type != fotype && tree[ti].prev >= 0; ti = tree[ti].prev){}
			if (tree[ti].type != fotype)
			{
				assertBufferSize( treesize, MAX_TREE_SIZE);
				assertBufferSize( stksize, MAX_SEARCH_SIZE);
				tree[ treesize].prev = fotreeidx;
				tree[ treesize].type = fotype;
				tree[ treesize].constructor = follow[ fi].constructor;
				size_t sn = stksize;
				for (; sn != si; --sn) {
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
						result->arsize_alt = fillSPSResult( result->ar_alt, tree, treesize-1);
						result->ar_alt[0].constructor = -1;
						break;
					} else {
						result->arsize = fillSPSResult( result->ar, tree, treesize-1);
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

typedef struct IdentSeqBuf {
	int32_t ar[ IDENTBUF32_SIZE];
	int32_t arpos;
} IdentSeqBuf;
static void initIdentSeqBuf( IdentSeqBuf* buf) {
	buf->arpos = 0;
}
static uint32_t rot( uint32_t n, uint32_t d)
{
	return (n<<d)|(n>>(32-d));
}
static uint32_t hashword( const int32_t* key, size_t keysize)
{
	uint32_t a = 0xdeadbeef;
	uint32_t b = 0xdeadbeef;
	uint32_t c = 0xdeadbeef;
	int32_t length = keysize;
	uint32_t const* kk = (uint32_t const*)key;

	while (length > 3)
	{
		a += kk[0];
		b += kk[1];
		c += kk[2];
		a -= c;  a ^= rot(c, 4);  c += b;
		b -= a;  b ^= rot(a, 6);  a += c;
		c -= b;  c ^= rot(b, 8);  b += a;
		a -= c;  a ^= rot(c,16);  c += b;
		b -= a;  b ^= rot(a,19);  a += c;
		c -= b;  c ^= rot(b, 4);  b += a;
		length -= 3;
		kk += 3;
	}
	switch(length)
	{
		case 3 : c += kk[2];
		case 2 : b += kk[1];
		case 1 : a += kk[0];
		c ^= b; c -= rot(b,14);
		a ^= c; a -= rot(c,11);
		b ^= a; b -= rot(a,25);
		c ^= b; c -= rot(b,16);
		a ^= c; a -= rot(c,4);
		b ^= a; b -= rot(a,14);
		c ^= b; c -= rot(b,24);
	}
	return (c + b + a);
}
static int32_t newIdent( IdentSeqBuf* buf, const char* id, size_t idsize) {
	int32_t ai = buf->arpos;
	assertBufferSize( idsize + buf->arpos, IDENTBUF32_SIZE);
	int32_t ae = buf->arpos + (idsize+1)/4;
	buf->ar[ ae + 1] = 0;
	memcpy( buf->ar + buf->arpos + 1, id, idsize);
	buf->ar[ ai] = hashword( buf->ar + buf->arpos + 1, ae - ai);
	return ai+1;
}
static void finalizeIdent( IdentSeqBuf* buf, size_t idsize)
{
	buf->arpos += (idsize+1)/4 + 1;
}
static int32_t getIdentHash( IdentSeqBuf const* buf, int32_t id)
{
	return buf->ar[ id-1];
}
static int32_t getIdentRehash( IdentSeqBuf const* buf, int32_t id, int32_t hs, int32_t hi)
{
	return rot( buf->ar[ id], hi) + hs + hi;
}
static const char* getIdentString( IdentSeqBuf const* buf, int32_t id)
{
	return (const char*)(buf->ar + id);
}
typedef struct IdentMap {
	IdentSeqBuf idseqbuf;
	int32_t slotar[ HASHTABLE_SIZE];
	int32_t fillsize;
} IdentMap;

static void initIdentMap( IdentMap* map) {
	initIdentSeqBuf( &map->idseqbuf);
	memset( map->slotar, 0, sizeof(map->slotar));
	map->fillsize = 0;
}
static int identMatches( IdentMap const* map, int32_t id, int32_t hash, const char* idstr, size_t idsize) {
	return (hash==getIdentHash( &map->idseqbuf, id) && 0==memcmp( getIdentString( &map->idseqbuf, id), idstr, idsize+1));
}
static int32_t getIdent( IdentMap* map, const char* idstr, size_t idsize) {
	int32_t rt = newIdent( &map->idseqbuf, idstr, idsize);
	int32_t hash = getIdentHash( &map->idseqbuf, rt);
	int32_t hi = 0;
	int32_t slotidx = hash;
	int32_t slot = map->slotar[ slotidx & HASHTABLE_MASK];

	while (slot != 0 && !identMatches( map, slot, hash, idstr, idsize)) {
		slotidx = getIdentRehash( &map->idseqbuf, rt, hash, hi);
		slot = map->slotar[ slotidx & HASHTABLE_MASK];
		hi += 1;
	}
	if (slot != 0) {
		rt = slot;
	} else {
		map->slotar[ slotidx & HASHTABLE_MASK] = rt;
		finalizeIdent( &map->idseqbuf, idsize);
	}
	return rt;
}
typedef struct Scope {
	int32_t start;
	int32_t end;
} Scope;

static int Scope_covers( Scope const* upper, Scope const* inner) {
	return upper->start <= inner->start && upper->end >= inner->end;
}
static int Scope_contains( Scope const* upper, int32_t step) {
	return upper->start <= step && upper->end > step;
}

typedef struct ScopedMapEntry {
	uint64_t key;
	int32_t scope_end;
	int32_t nodeidx;
} ScopedMapEntry;

static void initScopedMapEntry( ScopedMapEntry* st, uint64_t key, int32_t scope_end, int32_t nodeidx) {
	st->key = key; st->scope_end = scope_end; st->nodeidx = nodeidx;
}
static void copyScopedMapEntry( ScopedMapEntry* st, ScopedMapEntry const* ot) {
	st->key = ot->key; st->scope_end = ot->scope_end; st->nodeidx = ot->nodeidx;
}
static int32_t ScopedMapEntryArray_upperbound( ScopedMapEntry const* ar, int32_t arsize, uint64_t key, int32_t scope_end) {
	int32_t bi = 0, bn = arsize;
	while (bn - bi > 3) {
		int32_t bm = bi + (bn-bi) / 2;
		if (ar[ bm].key >= key && (ar[ bm].key > key || ar[ bm].scope_end > scope_end)) {
			bn = bm;
		} else {
			bi = bm;
		}
	}
	for (; bi < bn && ar[ bi].key < key; ++bi){}
	for (; bi < bn && ar[ bi].key == key && ar[ bi].scope_end < scope_end; ++bi){}
	return bi;
}
static void ScopedMapEntryArray_insert_at( ScopedMapEntry* ar, int32_t* arsize, int32_t bi, uint64_t key, int32_t scope_end, int32_t nodeidx) {
	int32_t be = *arsize;
	for (; be > bi; be--) {
		ar[be].key = ar[be-1].key; ar[be].scope_end = ar[be-1].scope_end; ar[be].nodeidx = ar[be-1].nodeidx;
	}
	ar[bi].key = key; ar[bi].scope_end = scope_end; ar[bi].nodeidx = nodeidx;
	*arsize += 1;
}

typedef struct ScopedMapBank {
	ScopedMapEntry ar[ SCOPEDMAP_BANK_SIZE];
	int32_t arsize;
	int32_t next;
	int32_t __reserved;
} ScopedMapBank;

static void initScopedMapBank( ScopedMapBank* st) {
	st->arsize = 0; st->next = -1;
}

typedef struct ScopedMapValueNode {
	int32_t uplink;
	int32_t value;
	Scope scope;
}  ScopedMapValueNode;

static void initScopedMapValueNode( ScopedMapValueNode* st, int32_t uplink, int32_t value, int32_t scope_start, int32_t scope_end) {
	st->uplink = uplink; st->value = value; st->scope.start = scope_start; st->scope.end = scope_end;
}

typedef struct ScopedMap {
	ScopedMapEntry firstar[ SCOPEDMAP_SIZE];
	ScopedMapBank bankar[ SCOPEDMAP_SIZE];
	ScopedMapValueNode nodear[ SCOPEDMAP_NODE_SIZE];
	int32_t firstarsize;
	int32_t bankarsize;
	int32_t nodearsize;
} ScopedMap;

static void initScopedMap( ScopedMap* st) {
	memset( st, 0, sizeof(ScopedMap));
}
static int32_t appendBank( ScopedMap* st) {
	int32_t rt = st->bankarsize;
	assertBufferSize( st->bankarsize, SCOPEDMAP_SIZE);
	st->bankarsize += 1;
	initScopedMapBank( st->bankar + rt);
	return rt;
}
static int32_t splitBank( ScopedMap* st, int32_t firstidx, int32_t bi) {
	ScopedMapEntry* et;
	ScopedMapEntry* dbase;
	ScopedMapEntry* sbase;
	int32_t rt = st->bankarsize, di = 0, si, sn;

	assertBufferSize( st->bankarsize, SCOPEDMAP_SIZE);
	assertBufferSize( st->firstarsize, SCOPEDMAP_SIZE);
	st->bankarsize += 1;
	initScopedMapBank( st->bankar + rt);
	sn = st->bankar[ bi].arsize;
	si = sn / 2;
	st->bankar[ bi].arsize = si;
	dbase = st->bankar[ rt].ar;
	sbase = st->bankar[ bi].ar;
	for (; si < sn; ++si,++di) {
		copyScopedMapEntry( dbase + di, sbase + si);
	}
	st->bankar[ rt].arsize = di;
	if (di == 0) throwError( LogicError, 0);
	et = st->bankar[rt].ar + di-1;
	ScopedMapEntryArray_insert_at( st->firstar, &st->firstarsize, 0, et->key, et->scope_end, rt/*nodeidx*/);
	et = st->bankar[bi].ar + si-1;
	st->firstar[ firstidx].key = et->key;
	st->firstar[ firstidx].scope_end = et->scope_end;
	return rt;
}
static int32_t appendNode( ScopedMap* st, int32_t value, int32_t scope_start, int32_t scope_end) {
	int32_t nodeidx = st->nodearsize;
	assertBufferSize( st->nodearsize, SCOPEDMAP_NODE_SIZE);
	st->nodearsize += 1;
	initScopedMapValueNode( st->nodear+nodeidx, -1/*uplink*/, value, scope_start, scope_end);
	return nodeidx;
}
static int32_t findNode( ScopedMap const* st, uint64_t key, int32_t step)
{
	int32_t fi = ScopedMapEntryArray_upperbound( st->firstar, st->firstarsize, key, step);
	if (fi == st->firstarsize || st->firstar[ fi].key != key) {
		return -1;
	}
	int32_t bi = st->firstar[ fi].nodeidx;
	ScopedMapBank const* bank = st->bankar + bi;
	int32_t ei = ScopedMapEntryArray_upperbound( bank->ar, bank->arsize, key, step);
	if (ei == bank->arsize || bank->ar[ ei].key != key) {
		return -1;
	}
	int32_t ni = bank->ar[ ei].nodeidx;
	for (; ni >= 0 && st->nodear[ ni].scope.start > step; ni = st->nodear[ ni].uplink){}
	return ni;
}
static void rewireNodesLeftHand( ScopedMap* st, int32_t nodeidx, uint64_t key, int32_t step) {
	while (step >= 0) {
		int32_t ni = findNode( st, key, step);
		if (ni < 0) {
			throwError( LogicError, 0);
		}
		if (st->nodear[ ni].scope.end != st->nodear[ nodeidx].scope.end) {
			while (ni >= 0 && st->nodear[ ni].scope.start < st->nodear[ nodeidx].scope.start) {
				step = st->nodear[ ni].scope.end;
				ni = st->nodear[ ni].uplink;
			}
			if (ni >= 0) {
				int32_t pi = ni;
				while (pi >= 0 && Scope_covers( &st->nodear[ nodeidx].scope, &st->nodear[ pi].scope)) {
					pi = st->nodear[ ni = pi].uplink;
				}
				if (Scope_covers( &st->nodear[ nodeidx].scope, &st->nodear[ ni].scope)) {
					st->nodear[ ni].uplink = nodeidx;
					step = st->nodear[ ni].scope.end;
				}
			}
		}
	}
}
static int32_t findRightHandNode( ScopedMap* st, int32_t nodeidx, uint64_t key) {

	int32_t ni = findNode( st, key, st->nodear[ nodeidx].scope.end);
	if (ni >= 0) {
		while (ni >= 0 && st->nodear[ ni].scope.start > st->nodear[ nodeidx].scope.start) {
			ni = st->nodear[ ni].uplink;
		}
		return ni;
	}
	return -1;
}
static int32_t insertNode( ScopedMap* st, int32_t* nodeidx, uint64_t key, int32_t value, int32_t scope_start, int32_t scope_end) {
	int32_t ni = *nodeidx;
	if (ni < 0 || ni >= st->nodearsize || scope_end != st->nodear[ ni].scope.end) {
		throwError( LogicError, 0);
	}
	if (st->nodear[ ni].scope.start <= scope_start) {
		if (st->nodear[ ni].scope.start == scope_start) {
			throwError( LogicError, 0);
		}
		ni = appendNode( st, value, scope_start, scope_end);
		st->nodear[ ni].uplink = *nodeidx;
		*nodeidx = ni;
	} else {
		int32_t ui = st->nodear[ ni].uplink;
		for (; ui >= 0 && st->nodear[ ui].scope.start <= scope_start; ni = ui, ui = st->nodear[ ni].uplink){}
		if (ui < 0) {
			ni = st->nodear[ ni].uplink = appendNode( st, value, scope_start, scope_end);
			st->nodear[ ni].uplink = findRightHandNode( st, ni, key);
		} else {
			int32_t mi = appendNode( st, value, scope_start, scope_end);
			st->nodear[ mi].uplink = ui;
			st->nodear[ ni].uplink = mi;
			ni = mi;
		}
	}
	return ni;
}

static void ScopedMap_insert( ScopedMap* st, uint64_t key, int32_t value, int32_t scope_start, int32_t scope_end)
{
	int32_t fi;
	if (st->firstarsize == 0) { //... first element to insert
		int32_t nodeidx = appendNode( st, value, scope_start, scope_end);
		appendBank( st);
		ScopedMapEntryArray_insert_at( st->bankar[0].ar, &st->bankar[0].arsize, 0, key, scope_end, nodeidx);
		ScopedMapEntryArray_insert_at( st->firstar, &st->firstarsize, 0, key, scope_end, 0/*nodeidx*/);
	} else {
		for (;;) {
			fi = ScopedMapEntryArray_upperbound( st->firstar, st->firstarsize, key, scope_end);
			if (fi == st->firstarsize) {
				int32_t nodeidx = appendNode( st, value, scope_start, scope_end);
				fi -= 1;
				int32_t bi = st->firstar[ fi].nodeidx;
				if (bi < 0 || bi >= st->bankarsize) {
					throwError( LogicError, 0);
				}
				ScopedMapBank* bank = st->bankar + bi;
				if (bank->arsize == SCOPEDMAP_BANK_SIZE) {
					bi = appendBank( st);
					ScopedMapEntryArray_insert_at( bank->ar, &bank->arsize, 0, key, scope_end, nodeidx);
					ScopedMapEntryArray_insert_at( st->firstar, &st->firstarsize, 0, key, scope_end, bi/*nodeidx*/);
				} else {
					int32_t di = bank->arsize;
					ScopedMapEntryArray_insert_at( bank->ar, &bank->arsize, di, key, scope_end, nodeidx);
					initScopedMapEntry( st->firstar + fi, key, scope_end, di/*nodeidx*/);
				}
				rewireNodesLeftHand( st, nodeidx, key, scope_start);
			} else {
				int32_t bi = st->firstar[ fi].nodeidx;
				if (bi < 0 || bi >= st->bankarsize) {
					throwError( LogicError, 0);
				}
				ScopedMapBank* bank = st->bankar + bi;
				int32_t di = ScopedMapEntryArray_upperbound( bank->ar, bank->arsize, key, scope_end);
				if (di < 0 || di >= bank->arsize) {
					throwError( LogicError, 0);
				}
				if (bank->ar[ di].key == key && bank->ar[ di].scope_end == scope_end) {
					int32_t nodeidx = insertNode( st, &bank->ar[ di].nodeidx, key, value, scope_start, scope_end);
					rewireNodesLeftHand( st, nodeidx, key, scope_start);
				} else {
					int32_t nodeidx = appendNode( st, value, scope_start, scope_end);
					assertBufferSize( st->firstarsize, SCOPEDMAP_SIZE);
					assertBufferSize( st->bankarsize, SCOPEDMAP_SIZE);
					if (bank->arsize == SCOPEDMAP_BANK_SIZE) {
						splitBank( st, fi, bi);
						continue;
					}
					ScopedMapEntryArray_insert_at( bank->ar, &bank->arsize, di, key, scope_end, nodeidx);
					rewireNodesLeftHand( st, nodeidx, key, scope_start);
				}
			}
			break;
		}
	}
}

typedef struct TreeNode {
	int32_t type;	//<  6 bits
	int32_t leaf;	//<  1 bit
	int32_t size;	//<  3 bits
	int32_t index;	//< 22 bits
} TreeNode;

static uint32_t packTreeNode( int32_t type, int32_t leaf, int32_t size, uint32_t index)
{
	if (type >= (1<<6) || leaf >= (1<<1) || size >= (1<<3) || index >= (1<<22)) {
		throwError( ValueOutOfRange, 0);
	}
	return index | (size << 22) | (leaf << 25) | (type << 26);
}
static void unpackTreeNode( TreeNode* dest, uint32_t value)
{
	dest->type  = (value >> 26) & ((1<<6)-1);
	dest->leaf  = (value >> 25) & 1;
	dest->size  = (value >> 22) & ((1<<3)-1);
	dest->index = value & ((1<<22)-1);
}

typedef struct AbstractSyntaxTree {
	uint32_t nodear[ MAX_AST_SIZE];
	uint32_t nodescope[ MAX_AST_SIZE];
	size_t nodearsize;
	IdentMap identMap;
} AbstractSyntaxTree;

void initAbstractSyntaxTree( AbstractSyntaxTree* ast) {
	memset( ast->nodear, 0, sizeof( ast->nodear));
	memset( ast->nodescope, 0, sizeof( ast->nodescope));
	ast->nodearsize = 0;
	initIdentMap( &ast->identMap);
}
typedef struct ParseStackElement {
	uint32_t astnode;
	int8_t state;
} ParseStackElement;
typedef struct ParseStack {
	ParseStackElement ar[ MAX_PARSE_STACK_SIZE];
	size_t arsize;
} ParseStack;

void initParseStack( ParseStack* ast) {
	ast->arsize = 0;
}
static void pushLexemNode( ParseStack* stk, AbstractSyntaxTree* ast, uint8_t state, int32_t type, int32_t id)
{
	assertBufferSize( stk->arsize, MAX_PARSE_STACK_SIZE);
	ParseStackElement* elem = stk->ar + stk->arsize;
	elem->astnode = packTreeNode( type, 1/*leaf*/, 0/*size*/, id);
	elem->state = state;
	stk->arsize += 1;
}
static void reduceNode( ParseStack* stk, AbstractSyntaxTree* ast, uint8_t state, int32_t type, size_t size)
{
	uint32_t id = ast->nodearsize;
	size_t si = 0;
	ParseStackElement const* ei = stk->ar + stk->arsize - size;
	if (stk->arsize < size) {
		throwError( LogicError, 0);
	}
	assertBufferSize( ast->nodearsize + size, MAX_AST_SIZE);
	for (; si != size; ++si,++ei) {
		ast->nodear[ ast->nodearsize + si] = ei->astnode;
	}
	assertBufferSize( stk->arsize, MAX_PARSE_STACK_SIZE);
	ParseStackElement* elem = stk->ar + stk->arsize - size;
	elem->astnode = packTreeNode( type, 0/*leaf*/, size, id);
	elem->state = state;
	stk->arsize -= size;
	stk->arsize += 1;
}
static uint32_t packScope( int32_t scope_start, int32_t scope_end)
{
	uint32_t scope_diff;
	if (scope_end < 0) {
		scope_diff = 0;
	} else {
		scope_diff = scope_end - scope_start;
		if (scope_diff >= (1<<13)) {
			throwError( ValueOutOfRange, 0);
		}
	}
	uint32_t scope_base = scope_start;
	if (scope_base >= (1<<18)) {
		throwError( ValueOutOfRange, 0);
	}
	return (scope_base << 13)|scope_diff;
}
static uint32_t packStep( uint32_t scope_step)
{
	if (scope_step > (1<<18)) {
		throwError( ValueOutOfRange, 0);
	}
	return scope_step|(1<<31);
}
static void setScope( AbstractSyntaxTree* ast, uint32_t nodeid, int32_t scope_start, int32_t scope_end)
{
	if (nodeid >= ast->nodearsize) {
		throwError( LogicError, 0);
	}
	ast->nodescope[ nodeid] = packScope( scope_start, scope_end);
}
static void setStep( AbstractSyntaxTree* ast, uint32_t nodeid, int32_t scope_step)
{
	if (nodeid >= ast->nodearsize) {
		throwError( LogicError, 0);
	}
	ast->nodescope[ nodeid] = packStep( scope_step);
}
typedef enum {
	Lexeme_EOF=0,		// '\0'
	Lexeme_Identifier,	// alpha alphanum-seq
	Lexeme_SQString,	// ''' ... '''
	Lexeme_DQString,	// '"' ... '"'
	Lexeme_Integer,		// digit-seq
	Lexeme_Float,		// digit-seq '.' digit-seq
	Operator_ASSIGN,	// '='
	Operator_DPLUS,		// '++'
	Operator_DMINUS,	// '--'
	Operator_PLUS,		// '+'
	Operator_MINUS,		// '-'
	Operator_ASTERISK,	// '*'
	Operator_SLASH,		// '/'
	Operator_PERCENT,	// '%'
	Operator_BSLASH,	// '\'
	Operator_RSHIFT,	// '>>'
	Operator_LSHIFT,	// '<<'
	Operator_LAND,		// '&&'
	Operator_LOR,		// '||'
	Operator_AND,		// '&'
	Operator_OR,		// '|'
	Operator_XOR,		// '^'
	Operator_NOT,		// '!'
	Operator_INV,		// '~'
	Operator_GT,		// '>'
	Operator_GE,		// '>='
	Operator_LT,		// '<'
	Operator_LE,		// '<='
	Operator_EQ,		// '=='
	Operator_NE,		// '!='
	Operator_SEMICOLON,	// ';'
	Operator_COLON,		// ':'
	Operator_QUESTION,	// '?'
	Operator_COMMA,		// ','
	Operator_DOT,		// '.'
	Operator_ARROW,		// '->'
	Operator_OVAL_OPEN,	// '('
	Operator_OVAL_CLOSE,	// ')'
	Operator_CURLY_OPEN,	// '{'
	Operator_CURLY_CLOSE,	// '}'
	Operator_SQUARE_OPEN,	// '['
	Operator_SQUARE_CLOSE,	// ']'
	Keyword_WHILE,		// "while"
	Keyword_FOR,		// "for"
	Keyword_DO,		// "do"
	Keyword_DEFAULT,	// "default"
	Keyword_IF,		// "if"
	Keyword_ELSE,		// "else"
	Keyword_SWITCH,		// "switch"
	Keyword_CASE,		// "case"
	Keyword_CONST,		// "const"
	Keyword_BREAK,		// "break"
	Keyword_CONTINUE,	// "continue"
	Keyword_RETURN,		// "return"
	Keyword_STATIC,		// "static"
	Keyword_ENUM,		// "enum"
	Keyword_SIZEOF,		// "sizeof"
	Keyword_TYPEDEF,	// "typedef"
	Keyword_UNION,		// "typedef"
	Keyword_SIGNED,		// "signed"
	Keyword_UNSIGNED,	// "unsigned"
	Keyword_INT,		// "int"
	Keyword_SHORT,		// "short"
	Keyword_CHAR,		// "char"
	Keyword_LONG		// "long"
} LexemeType;

static int8_t operatorPrecedence_C( LexemeType op) {
	switch (op) {
		case Operator_DPLUS: case Operator_DMINUS: case Operator_DOT: case Operator_ARROW: case Operator_OVAL_OPEN:
			case Operator_OVAL_CLOSE: case Operator_CURLY_OPEN: case Operator_CURLY_CLOSE:
			case Operator_SQUARE_OPEN: Operator_SQUARE_CLOSE: return 1;
		case Operator_NOT: case Operator_INV: return 2;
		case Operator_ASTERISK: case Operator_SLASH: case Operator_PERCENT: return 3;
		case Operator_PLUS: case Operator_MINUS: return 4;
		case Operator_RSHIFT: case Operator_LSHIFT: return 5;
		case Operator_GT: case Operator_GE: case Operator_LT: case Operator_LE: return 6;
		case Operator_EQ: case Operator_NE: return 7;
		case Operator_AND: return 8;
		case Operator_XOR: return 9;
		case Operator_OR: return 10;
		case Operator_LAND: return 11;
		case Operator_LOR: return 12;
		case Operator_QUESTION: 13;
		case Operator_ASSIGN: return 14;
		case Operator_COMMA: return 15;
	}
	return 127;
}

typedef struct Lexeme {
	size_t pos;
	int32_t id;
	LexemeType type;
} Lexeme;

static int lexeme( Lexeme* lx, LexemeType type, int32_t id, size_t pos) {
	lx->type = type;
	lx->id = id;
	lx->pos = pos;
	return 1;
}
static int lineNumber( char const* src, size_t pos) {
	int rt = 1;
	for (size_t pi = 0; pi < pos; ++pi) {
		if (src[pi] == '\n') {
			rt += 1;
		}
	}
	return rt;
}
static size_t skipEndOfLine( char const* src, size_t pos) {
	while (src[pos] && (unsigned char) src[pos] != '\n') {
		pos += 1;
	}
	return src[pos] ? (pos+1) : pos;
}
static size_t skipEndOfComment( char const* src, size_t pos) {
	while (src[pos]) {
		if (src[pos] == '*' && src[pos+1] == '/') {
			return pos+2;
		}
		pos += 1;
	}
	return pos;
}
static size_t skipSpaces( char const* src, size_t pos) {
	for (;;) {
		while (src[pos] && (unsigned char) src[pos] <= 32) {
			pos = pos + 1;
		}
		if (src[pos] == '/') {
			if (src[pos+1] == '/') {
				pos = skipEndOfLine( src, pos);
				continue;
			} else if (src[pos+1] == '*') {
				pos = skipEndOfComment( src, pos);
				continue;
			}
		}
		break;
	}
	return pos;
}
static int isDigit( char ch) {
	return (ch - '0') <= 9;
}
static int isAlpha( char ch) {
	return (unsigned char)((ch|32) - 'a') <= 26 || ch == '_';
}
static int isAlphaNum( char ch) {
	return (unsigned char)((ch|32) - 'a') <= 26 || ch == '_' || (ch - '0') <= 9;
}
static int isKeyword( char const* kw, char const* src, size_t pos) {
	for (; src[pos] == *kw; ++kw,++pos){}
	return !isAlphaNum( src[pos]);
}
static int nextLexeme( Lexeme* lx, char const* src, size_t* pos, IdentMap* identMap) {
	for (;;) {
		*pos = skipSpaces( src, *pos);
		switch (src[*pos]) {
			case '\0': return lexeme( lx, Lexeme_EOF, -1/*id*/, *pos);
			case '+': ++*pos; return (src[*pos] == '+') ? lexeme( lx, Operator_DPLUS, -1/*id*/, *pos += 1) : lexeme( lx, Operator_PLUS, -1/*id*/, *pos);
			case '-': ++*pos; return (src[*pos] == '-') ? lexeme( lx, Operator_DMINUS, -1/*id*/, *pos += 1) : (src[*pos] == '>') ? lexeme( lx, Operator_ARROW, -1/*id*/, *pos += 1) : lexeme( lx, Operator_MINUS, -1/*id*/, *pos);
			case '*': ++*pos; return lexeme( lx, Operator_ASTERISK, -1/*id*/, *pos);
			case '/': ++*pos; return lexeme( lx, Operator_SLASH, -1/*id*/, *pos);
			case '%': ++*pos; return lexeme( lx, Operator_PERCENT, -1/*id*/, *pos);
			case '\\': ++*pos; return lexeme( lx, Operator_BSLASH, -1/*id*/, *pos);
			case '&': ++*pos; return (src[*pos] == '&') ? lexeme( lx, Operator_LAND, -1/*id*/, *pos += 1) : lexeme( lx, Operator_AND, -1/*id*/, *pos);
			case '|': ++*pos; return (src[*pos] == '|') ? lexeme( lx, Operator_LOR, -1/*id*/, *pos += 1) : lexeme( lx, Operator_OR, -1/*id*/, *pos);
			case '^': ++*pos; return lexeme( lx, Operator_XOR, -1/*id*/, *pos);
			case '~': ++*pos; return lexeme( lx, Operator_INV, -1/*id*/, *pos);
			case '>': ++*pos; return (src[*pos] == '=') ? lexeme( lx, Operator_GE, -1/*id*/, *pos += 1) : lexeme( lx, Operator_GT, -1/*id*/, *pos);
			case '<': ++*pos; return (src[*pos] == '=') ? lexeme( lx, Operator_LE, -1/*id*/, *pos += 1) : lexeme( lx, Operator_LT, -1/*id*/, *pos);
			case '=': ++*pos; return (src[*pos] == '=') ? lexeme( lx, Operator_EQ, -1/*id*/, *pos += 1) : lexeme( lx, Operator_ASSIGN, -1/*id*/, *pos);
			case '!': ++*pos; return (src[*pos] == '=') ? lexeme( lx, Operator_NE, -1/*id*/, *pos += 1) : lexeme( lx, Operator_NOT, -1/*id*/, *pos);
			case ';': ++*pos; return lexeme( lx, Operator_SEMICOLON, -1/*id*/, *pos);
			case ':': ++*pos; return lexeme( lx, Operator_COLON, -1/*id*/, *pos);
			case '?': ++*pos; return lexeme( lx, Operator_QUESTION, -1/*id*/, *pos);
			case ',': ++*pos; return lexeme( lx, Operator_COMMA, -1/*id*/, *pos);
			case '.': if (!isDigit(src[*pos+1])) {++*pos; return lexeme( lx, Operator_DOT, -1/*id*/, *pos);} else break;
			case '(': ++*pos; return lexeme( lx, Operator_OVAL_OPEN, -1/*id*/, *pos);
			case ')': ++*pos; return lexeme( lx, Operator_OVAL_CLOSE, -1/*id*/, *pos);
			case '{': ++*pos; return lexeme( lx, Operator_CURLY_OPEN, -1/*id*/, *pos);
			case '}': ++*pos; return lexeme( lx, Operator_CURLY_CLOSE, -1/*id*/, *pos);
			case '[': ++*pos; return lexeme( lx, Operator_SQUARE_OPEN, -1/*id*/, *pos);
			case ']': ++*pos; return lexeme( lx, Operator_SQUARE_CLOSE, -1/*id*/, *pos);
			case '#': ++*pos; if (isKeyword( "include", src, *pos)) {
				*pos = skipEndOfLine( src, *pos);
				continue;
			}
			case 'w': if (isKeyword( "while", src, *pos)) {
				return lexeme( lx, Keyword_WHILE, -1/*id*/, *pos += 5);
			}
			case 'f': if (isKeyword( "for", src, *pos)) {
				return lexeme( lx, Keyword_FOR, -1/*id*/, *pos += 3);
			}
			case 'i': if (isKeyword( "if", src, *pos)) {
				return lexeme( lx, Keyword_IF, -1/*id*/, *pos += 2);
			} else if (isKeyword( "int", src, *pos)) {
				return lexeme( lx, Keyword_INT, -1/*id*/, *pos += 3);
			}
			case 'l': if (isKeyword( "long", src, *pos)) {
				return lexeme( lx, Keyword_LONG, -1/*id*/, *pos += 4);
			}
			case 'e': if (isKeyword( "else", src, *pos)) {
				return lexeme( lx, Keyword_ELSE, -1/*id*/, *pos += 4);
			} else if (isKeyword( "enum", src, *pos)) {
				return lexeme( lx, Keyword_ENUM, -1/*id*/, *pos += 4);
			}
			case 's': if (isKeyword( "switch", src, *pos)) {
				return lexeme( lx, Keyword_SWITCH, -1/*id*/, *pos += 6);
			} else if (isKeyword( "static", src, *pos)) {
				return lexeme( lx, Keyword_STATIC, -1/*id*/, *pos += 6);
			} else if (isKeyword( "signed", src, *pos)) {
				return lexeme( lx, Keyword_SIGNED, -1/*id*/, *pos += 6);
			} else if (isKeyword( "sizeof", src, *pos)) {
				return lexeme( lx, Keyword_SIZEOF, -1/*id*/, *pos += 6);
			} else if (isKeyword( "short", src, *pos)) {
				return lexeme( lx, Keyword_SHORT, -1/*id*/, *pos += 5);
			}
			case 't': if (isKeyword( "typedef", src, *pos)) {
				return lexeme( lx, Keyword_TYPEDEF, -1/*id*/, *pos += 7);
			}
			case 'd': if (isKeyword( "do", src, *pos)) {
				return lexeme( lx, Keyword_DO, -1/*id*/, *pos += 2);
			} else if (isKeyword( "default", src, *pos)) {
				return lexeme( lx, Keyword_DEFAULT, -1/*id*/, *pos += 7);
			}
			case 'c': if (isKeyword( "case", src, *pos)) {
				return lexeme( lx, Keyword_CASE, -1/*id*/, *pos += 4);
			} else if (isKeyword( "const", src, *pos)) {
				return lexeme( lx, Keyword_CONST, -1/*id*/, *pos += 5);
			} else if (isKeyword( "char", src, *pos)) {
				return lexeme( lx, Keyword_CHAR, -1/*id*/, *pos += 4);
			} else if (isKeyword( "continue", src, *pos)) {
				return lexeme( lx, Keyword_CONTINUE, -1/*id*/, *pos += 8);
			}
			case 'b': if (isKeyword( "break", src, *pos)) {
				return lexeme( lx, Keyword_BREAK, -1/*id*/, *pos += 5);
			}
			case 'r': if (isKeyword( "return", src, *pos)) {
				 return lexeme( lx, Keyword_RETURN, -1/*id*/, *pos += 6);
			}
			case 'u': if (isKeyword( "union", src, *pos)) {
				return lexeme( lx, Keyword_UNION, -1/*id*/, *pos += 5);
			} else if (isKeyword( "unsigned", src, *pos)) {
				return lexeme( lx, Keyword_UNSIGNED, -1/*id*/, *pos += 8);
			}
		}
		if (src[*pos] == '\'' || src[*pos] == '\"')
		{
			char eb = src[*pos];
			char const* si = src + ++*pos;
			for (; *si != eb && *si != '\n'; ++si) {
				if (*si == '\\') {
					++si;
				}
			}
			if (*si == eb) {
				size_t len = si - src - *pos;
				int32_t id = getIdent( identMap, src + *pos, len);
				return lexeme( lx, eb == '\'' ? Lexeme_SQString : Lexeme_DQString, id, *pos += len+1);
			} else {
				throwError( SyntaxError, lineNumber( src, *pos));
			}
		}
		else if (isAlpha( src[*pos]))
		{
			char const* si = src + *pos;
			for (; isAlphaNum( *si); ++si){}
			size_t len = si - src - *pos;
			int32_t id = getIdent( identMap, src + *pos, len);
			return lexeme( lx, Lexeme_Identifier, id, *pos += len);
		}
		else if (isDigit( src[*pos]) || src[*pos] == '.')
		{
			char const* si = src + *pos;
			for (; isDigit( *si); ++si){}
			LexemeType lt = Lexeme_Integer;
			if (*si == '.') {
				lt = Lexeme_Float;
				++si;
				for (; isDigit( *si); ++si){}
			}
			size_t len = si - src - *pos;
			int32_t id = getIdent( identMap, src + *pos, len);
			return lexeme( lx, lt, id, *pos += len);
		}
		else
		{
			throwError( SyntaxError, lineNumber( src, *pos));
		}
	}
}

typedef enum {
	AstInteger,
	AstFloat,
	AstDQString,
	AstSQString,
	AstIdentifier,
	AstPointer,
	AstConst
} AstNodeType;

static void parseTypeDeclaration( ParseStack* stk, AbstractSyntaxTree* ast, const char* srcstr, size_t srclen, size_t* srcpos) {
	Lexeme lx;
	size_t prevpos = *srcpos;
	LexemeType prevtype;
	char const* lxstr;
	int state = 0; //... 0: prefix const, 1: expect idenfitier, 2: expect const, '*', 3: done

	for (; state <= 2 && nextLexeme( &lx, srcstr, srcpos, &ast->identMap); prevpos=*srcpos) {
		switch (lx.type) {
			case Keyword_CONST:
				if (state == 0) {//... prefix const
					if (!nextLexeme( &lx, srcstr, srcpos, &ast->identMap)) {
						throwError( UnexpectedEOF, lineNumber( srcstr, lx.pos));
					}
					if (lx.type != Lexeme_Identifier) {
						throwError( SyntaxError, lineNumber( srcstr, lx.pos));
					}
					pushLexemNode( stk, ast, state=1, AstIdentifier, lx.id);
					reduceNode( stk, ast, state, AstConst, 1);
				} else {//... postfix const
					reduceNode( stk, ast, state, AstConst, 1);
				}
				break;
			case Operator_ASTERISK:
				if (state <= 1) {
					throwError( SyntaxError, lineNumber( srcstr, lx.pos));
				} else {
					reduceNode( stk, ast, state, AstPointer, 1);
				}
				break;
			case Keyword_UNSIGNED:
			case Keyword_SIGNED:
				if (state <= 1) {
					prevtype = lx.type;
					prevpos = *srcpos;
					if (nextLexeme( &lx, srcstr, srcpos, &ast->identMap)) {
						switch (lx.type) {
							case Keyword_INT:
								lxstr = prevtype == Keyword_UNSIGNED ? "unsigned int" : "signed int";
								break;
							case Keyword_SHORT:
								lxstr = prevtype == Keyword_UNSIGNED ? "unsigned short" : "signed short";
								break;
							case Keyword_LONG:
								lxstr = prevtype == Keyword_UNSIGNED ? "unsigned long" : "signed long";
								break;
							case Keyword_CHAR:
								lxstr = prevtype == Keyword_UNSIGNED ? "unsigned char" : "signed char";
								break;
							default:
								lxstr = prevtype == Keyword_UNSIGNED ? "unsigned int" : "signed int";
								*srcpos = prevpos;
						}
					} else {
						lxstr = prevtype == Keyword_UNSIGNED ? "unsigned int" : "signed int";
						*srcpos = prevpos;
					}
					pushLexemNode( stk, ast, state=2, AstIdentifier, getIdent( &ast->identMap, lxstr, strlen(lxstr)));
				} else	{
					throwError( SyntaxError, lineNumber( srcstr, lx.pos));
				}
				break;
			case Keyword_INT:
				pushLexemNode( stk, ast, state=2, AstIdentifier, getIdent( &ast->identMap, "int", 3));
				break;
			case Keyword_SHORT:
				pushLexemNode( stk, ast, state=2, AstIdentifier, getIdent( &ast->identMap, "short", 5));
				break;
			case Keyword_LONG:
				pushLexemNode( stk, ast, state=2, AstIdentifier, getIdent( &ast->identMap, "long", 4));
				break;
			case Keyword_CHAR:
				pushLexemNode( stk, ast, state=2, AstIdentifier, getIdent( &ast->identMap, "char", 4));
				break;
			case Lexeme_Identifier:
				if (state <= 1) {
					pushLexemNode( stk, ast, state=2, AstIdentifier, lx.id);
				} else {
					*srcpos = prevpos;
					state = 3;
				}
				break;
			default: throwError( SyntaxError, lineNumber( srcstr, lx.pos));
		}
	}
}

static void parseSource( AbstractSyntaxTree* ast, const char* srcstr, size_t srclen) {
	Lexeme lx;
	size_t srcpos = 0;
	size_t prevpos = srcpos;
	ParseStack stk;
	initParseStack( &stk);

	for (; nextLexeme( &lx, srcstr, &srcpos, &ast->identMap); prevpos=srcpos) {
		switch (lx.type) {
			case Lexeme_Identifier: Keyword_CONST: case Keyword_SIGNED: case Keyword_UNSIGNED: case Keyword_CHAR: case Keyword_SHORT: case Keyword_INT: case Keyword_LONG:
				srcpos = prevpos;
				parseTypeDeclaration( &stk, ast, srcstr, srclen, &srcpos);
				break;
			default: throwError( SyntaxError, lineNumber( srcstr, lx.pos));
		}
	}
}

int main( int argc, char const* argv[]) {
	AbstractSyntaxTree ast;
	initAbstractSyntaxTree( &ast);
	parseSource( &ast, 0, 0);
	return 0;
}

