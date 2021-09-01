#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct SPSStackElement {
	float weight;
	int32_t idx;
} SPSStackElement;

typedef struct SPSFollowElement {
	float weight;
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
	SCOPEDMAP_NODE_SIZE = 1<<17
};
typedef enum {
	Ok=0,
	LogicError=1,
	BufferTooSmall=2
} ErrorCode;

static const char* errorString( ErrorCode errcode) {
	static const char* ar[] = {
		"",
		"logic error",
		"internal buffer too small"
	};
	return ar[ errcode];
}
static void throwError( ErrorCode errcode, const char* msg) {
	if (msg) {
		fprintf( stderr, "Exception %d: %s (%s)\n", (int)errcode, errorString( errcode), msg);
	} else {
		fprintf( stderr, "Exception %d: %s\n", (int)errcode, errorString( errcode));
	}
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
	float weight;
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
			float foweight = elem->weight + follow[ fi].weight;
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

int main( int argc, char const* argv[]) {
	IdentMap identmap;
	initIdentMap( &identmap);
	return 0;
}

