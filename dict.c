#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define DICT_OK 0
#define DICT_ERR 1

#define DICT_NOTUSED(V)	((void) V)

static uint8_t dict_hash_function_seed[16];

typedef struct dictEntry {
	void *key;
	union {
		void *val;
		uint64_t u64;
		int64_t s64;
		double d;
	}v;
	struct dictEntry *next;
}dictEntry;

typedef struct dictType{
	uint64_t (*hashFunction)(const void *key);
	void *(*keyDup)(void *privdata, const void *key);
	void *(*valDup)(void *privdata, const void *obj);
	int (*keyCompare)(void *privdata, const void *key1, const void *key2);
	void (*keyDestructor)(void *privdata, void *key);
	void (*valDestructor)(void *privdata, void *obj);
}dictType;

typedef struct dictht{
	dictEntry **table;
	unsigned long size;
	unsigned long sizemask;
	unsigned long used;
}dictht;

typedef struct dict{
	dictType *type;
	void *privdata;
	dictht ht[2];
	long rehashidx;
	unsigned long iterators;
}dict;

typedef struct dictIterator{
	dict *d;
	long index;
	int table, safe;
	dictEntry *entry, *nextEntry;
	long long fingerprint;
}dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);
typedef void (dictScanBucketFunction)(void *privdata, dictEntry **bucketref);


#define DICT_HT_INITIAL_SIZE 4
#define LONG_MAX   64//这个还有问题

#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        (entry)->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        (entry)->v.val = (_val_); \
} while(0)

#define dictSetSignedIntegerVal(entry, _val_) \
    do { (entry)->v.s64 = _val_; } while(0)

#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { (entry)->v.u64 = _val_; } while(0)

#define dictSetDoubleVal(entry, _val_) \
    do { (entry)->v.d = _val_; } while(0)

#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        (entry)->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        (entry)->key = (_key_); \
} while(0)

#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

#define dictHashKey(d, key) (d)->type->hashFunction(key)
#define dictGetKey(he) ((he)->key)
#define dictGetVal(he) ((he)->v.val)
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
#define dictGetDoubleVal(he) ((he)->v.d)
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
#define dictIsRehashing(d) ((d)->rehashidx != -1)

uint64_t siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);
uint64_t siphash_nocase(const uint8_t *in, const size_t inlen, const uint8_t *k);


void *zmalloc(size_t size)
{
	void *ptr = malloc(size);
	return ptr;
}

void *zcalloc(size_t size)
{
	void *ptr = malloc(size);
	return ptr;
}

void zfree(void *ptr)
{
	if(ptr == NULL) return;
	
	free(ptr);
}


static int dict_can_resize = 1;
static unsigned int dict_force_resize_ratio = 5;

/* API */
dict *dictCreate(dictType *type, void *privDataPtr);
int dictExpand(dict *d, unsigned long size);
int dictAdd(dict *d, void *key, void *val);
dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing);
dictEntry *dictAddOrFind(dict *d, void *key);
int dictReplace(dict *d, void *key, void *val);
int dictDelete(dict *d, const void *key);
dictEntry *dictUnlink(dict *ht, const void *key);
void dictFreeUnlinkedEntry(dict *d, dictEntry *he);
void dictRelease(dict *d);
dictEntry * dictFind(dict *d, const void *key);
void *dictFetchValue(dict *d, const void *key);
int dictResize(dict *d);
dictIterator *dictGetIterator(dict *d);
dictIterator *dictGetSafeIterator(dict *d);
dictEntry *dictNext(dictIterator *iter);
void dictReleaseIterator(dictIterator *iter);
dictEntry *dictGetRandomKey(dict *d);
//dictEntry *dictGetFairRandomKey(dict *d);
//unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);
void dictGetStats(char *buf, size_t bufsize, dict *d);
uint64_t dictGenHashFunction(const void *key, int len);
uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len);
void dictEmpty(dict *d, void(callback)(void*));
void dictEnableResize(void);
void dictDisableResize(void);
int dictRehash(dict *d, int n);
//int dictRehashMilliseconds(dict *d, int ms);
void dictSetHashFunctionSeed(uint8_t *seed);
uint8_t *dictGetHashFunctionSeed(void);
//unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, dictScanBucketFunction *bucketfn, void *privdata);
//uint64_t dictGetHash(dict *d, const void *key);
//dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, uint64_t hash);

uint64_t dictGenHashFunction(const void *key, int len) {
    return siphash(key,len,dict_hash_function_seed);
}

uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len)
{
	return siphash_nocase(buf, len, dict_hash_function_seed);
}

void dictSetHashFunctionSeed(uint8_t *seed)
{
	memcpy(dict_hash_function_seed, seed, sizeof(dict_hash_function_seed));
}

uint8_t *dictGetHashFunctionSeed(void){
	return dict_hash_function_seed;
}

int dictReplace(dict *d, void *key, void *val)
{
	dictEntry *entry, *existing, auxentry;
	
	entry = dictAddRaw(d, key, &existing);
	if(entry)
	{
		dictSetVal(d, entry, val);
		return 1;
	}
	
	auxentry = *existing;
	dictSetVal(d, existing, val);
	dictFreeVal(d, &auxentry);
}


dictEntry *dictAddOrFind(dict *d, void *key)
{
	dictEntry *entry, *existing;
	entry = dictAddRaw(d, key, &existing);
	return entry ? entry : existing;
}

static unsigned long _dictNextPower(unsigned long size)
{
	unsigned long i = DICT_HT_INITIAL_SIZE;
	
	if(size >= LONG_MAX) return LONG_MAX + 1LU;
	while(1)
	{
		if(i >= size)
			return i;
		i *= 2;
	}		
}

static void _dictReset(dictht *ht)
{
	ht->table = NULL;
	ht->size = 0;
	ht->sizemask = 0;
	ht->used = 0;
}

dict *dictCreate(dictType *type, void *privDataPtr)
{
	dict *d = zmalloc(sizeof(*d));
	
	_dictInit(d, type, privDataPtr);
	return d;
}
int _dictInit(dict *d, dictType *type, void *privDataPtr)
{
	_dictReset(&d->ht[0]);
	_dictReset(&d->ht[1]);
	d->type = type;
	d->privdata = privDataPtr;
	d->rehashidx = -1;
	d->iterators = 0;
	return DICT_OK;
}

int dictAdd(dict *d, void *key, void *val)
{
	dictEntry *entry = dictAddRaw(d, key, NULL);
	
	if(!entry) return DICT_ERR;
	dictSetVal(d, entry, val);
	return DICT_OK;
}

int dictExpand(dict *d, unsigned long size)
{
	if(dictIsRehashing(d) || d->ht[0].used > size)
		return DICT_ERR;
	
	dictht n;
	unsigned long realsize = _dictNextPower(size);
	
	if(realsize == d->ht[0].size) return DICT_ERR;
	
	n.size = realsize;
	n.sizemask = realsize - 1;
	n.table = zcalloc(realsize * sizeof(dictEntry*));
	n.used = 0;
	
	if(d->ht[0].table == NULL)
	{
		d->ht[0] = n;
		return DICT_OK;
	}
	
	d->ht[1] = n;
	d->rehashidx = 0;
	return DICT_OK;
}


static int _dictExpandIfNeeded(dict *d)
{
	if(dictIsRehashing(d)) return DICT_OK;
	
	if(d->ht[0].size == 0) return dictExpand(d, DICT_HT_INITIAL_SIZE);
	
	if(d->ht[0].used >= d->ht[0].size && (dict_can_resize || d->ht[0].used / d->ht[0].size > dict_force_resize_ratio))
	{
		printf("begin to expand, %s, %d\n", __func__, __LINE__);
		return dictExpand(d, d->ht[0].used * 2);
	}
	return DICT_OK;
}


static long _dictKeyIndex(dict *d, const void *key, uint64_t hash, dictEntry **existing)
{
	unsigned long idx, table;
	dictEntry *he;
	if(existing) *existing = NULL;
	
	if(_dictExpandIfNeeded(d) == DICT_ERR)
		return -1;

    for (table = 0; table <= 1; table++) {
        idx = hash & d->ht[table].sizemask;
        /* Search if this slot does not already contain the given key */
        he = d->ht[table].table[idx];
        while(he) {
            if (key == he->key || dictCompareKeys(d, key, he->key)) {
                if (existing) *existing = he;
                return -1;
            }
            he = he->next;
        }
        if (!dictIsRehashing(d)) break;
    }
    return idx;
}

int dictRehash(dict *d, int n)
{
	int empty_visits = n * 10;
	if(!dictIsRehashing(d)) return 0;
	
	while(n-- && d->ht[0].used != 0)
	{
		dictEntry *de, *nextde;
		
		while(d->ht[0].table[d->rehashidx] == NULL)
		{
			d->rehashidx++;
			if(--empty_visits == 0) return 1;
		}
		de = d->ht[0].table[d->rehashidx];
		while(de)
		{
			uint64_t h;
			
			nextde = de->next;
			
			h = dictHashKey(d, de->key) & d->ht[1].sizemask;
			de->next = d->ht[1].table[h];
			d->ht[1].table[h] = de;
			d->ht[0].used--;
			d->ht[1].used++;
			de = nextde;
		}
		d->ht[0].table[d->rehashidx] = NULL;
		d->rehashidx++;
	}
	
	if(d->ht[0].used == 0)
	{
		zfree(d->ht[0].table);
		d->ht[0] = d->ht[1];
		_dictReset(&d->ht[1]);
		d->rehashidx = -1;
		return 0;
	}
	
	return 1;
}


static void _dictRehashStep(dict *d)
{
	if(d->iterators == 0) dictRehash(d, 1);
}


dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing)
{
	long index;
	dictEntry *entry;
	dictht *ht;
	
	if(dictIsRehashing(d)) _dictRehashStep(d);
	
	if((index = _dictKeyIndex(d, key, dictHashKey(d, key), existing)) == -1)
		return NULL;
	printf("hashkey is %llu, %s, %d\n", dictHashKey(d, key), __func__, __LINE__);
	ht = dictIsRehashing(d) ? &d->ht[1] : &d->ht[0];
	entry = zmalloc(sizeof(*entry));
	entry->next = ht->table[index];
	ht->table[index] = entry;
	ht->used++;
	
	dictSetKey(d, entry, key);
	return entry;
}

static dictEntry *dictGenericDelete(dict *d, const void *key, int nofree)
{
	uint64_t h, idx;
	dictEntry *he, *preHe;
	int table;
	
	if(d->ht[0].used == 0 && d->ht[1].used == 0) return NULL;
	
	if(dictIsRehashing(d)) _dictRehashStep(d);
	h = dictHashKey(d, key);
	
	for(table = 0; table <= 1; table++)
	{
		idx = h & d->ht[table].sizemask;
		he = d->ht[table].table[idx];
		preHe = NULL;
		while(he)
		{
			if(key == he->key || dictCompareKeys(d, key, he->key))
			{
				if(preHe)
					preHe->next = he->next;
				else
					d->ht[table].table[idx] = he->next;
				if(!nofree)
				{
					dictFreeKey(d, he);
					dictFreeVal(d, he);
					zfree(he);
				}
				d->ht[table].used--;
				return he;
			}
			preHe = he;
			he = he->next;
		}
		if(!dictIsRehashing(d)) break;
	}
	return NULL;
}

int dictDelete(dict *ht, const void *key)
{
	return dictGenericDelete(ht, key, 0) ? DICT_OK : DICT_ERR;
}

dictEntry *dictUnlink(dict *ht, const void *key)
{
	return dictGenericDelete(ht, key, 1);
}

void dictFreeUnlinkedEntry(dict *d, dictEntry *he)
{
	if(he == NULL) return;
	dictFreeKey(d, he);
	dictFreeVal(d, he);
	zfree(he);
}

int _dictClear(dict *d, dictht *ht, void(callback)(void *))
{
	unsigned long i;
	
	for(i = 0; i < ht->size &&  ht->used > 0; i++)
	{
		dictEntry *he, *nextHe;
		
		if(callback && (i & 65535) == 0) callback(d->privdata);
		
		if((he = ht->table[i]) == NULL) continue;
		while(he)
		{
			nextHe = he->next;
			dictFreeKey(d, he);
			dictFreeVal(d, he);
			zfree(he);
			ht->used--;
			he = nextHe;
		}
	}
	
	zfree(ht->table);
	_dictReset(ht);
	return DICT_OK;
}

void dictRelease(dict *d)
{
	_dictClear(d, &d->ht[0], NULL);
	_dictClear(d, &d->ht[1], NULL);
	zfree(d);
}

dictEntry * dictFind(dict *d, const void *key)
{
	dictEntry *he;
	uint64_t h, idx, table;
	
	if(dictSize(d) == 0) return NULL;
	if(dictIsRehashing(d)) _dictRehashStep(d);
	h = dictHashKey(d, key);
	printf("h is %llu, %s, %d\n",h, __func__, __LINE__);
	for(table = 0; table <= 1; table++)
	{
		idx = h & d->ht[table].sizemask;
		printf("idx is %llu, %s, %d\n", idx, __func__, __LINE__);
		he = d->ht[table].table[idx];
		while(he)
		{
			if(key == he->key || dictCompareKeys(d, key, he->key))
				return he;
			he = he->next;
		}
		if(!dictIsRehashing(d)) return NULL;
	}
	return NULL;
}

void *dictFetchValue(dict *d, const void *key)
{
	dictEntry *he;
	
	he = dictFind(d, key);
	return he ? dictGetVal(he) : NULL;
}

int dictResize(dict *d)
{
	unsigned long minimal;
	
	if(!dict_can_resize || dictIsRehashing(d)) return DICT_ERR;
	minimal = d->ht[0].used;
	if(minimal < DICT_HT_INITIAL_SIZE)
		minimal = DICT_HT_INITIAL_SIZE;
	return dictExpand(d, minimal);
}

dictIterator *dictGetIterator(dict *d)
{
	dictIterator *iter = zmalloc(sizeof(*iter));
	
	iter->d = d;
	iter->table = 0;
	iter->index = 0;
	iter->safe = 0;
	iter->entry = NULL;
	iter->nextEntry = NULL;
	return iter;
}

dictIterator *dictGetSafeIterator(dict *d)
{
	dictIterator *i = dictGetIterator(d);
	
	i->safe = 1;
	return i;
}

long long dictFingerprint(dict *d) {
    long long integers[6], hash = 0;
    int j;

    integers[0] = (long) d->ht[0].table;
    integers[1] = d->ht[0].size;
    integers[2] = d->ht[0].used;
    integers[3] = (long) d->ht[1].table;
    integers[4] = d->ht[1].size;
    integers[5] = d->ht[1].used;

    /* We hash N integers by summing every successive integer with the integer
     * hashing of the previous sum. Basically:
     *
     * Result = hash(hash(hash(int1)+int2)+int3) ...
     *
     * This way the same set of integers in a different order will (likely) hash
     * to a different number. */
    for (j = 0; j < 6; j++) {
        hash += integers[j];
        /* For the hashing step we use Tomas Wang's 64 bit integer hash. */
        hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
        hash = hash ^ (hash >> 24);
        hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
        hash = hash ^ (hash >> 14);
        hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
        hash = hash ^ (hash >> 28);
        hash = hash + (hash << 31);
    }
    return hash;
}

dictEntry *dictNext(dictIterator *iter)
{
    while (1) {
        if (iter->entry == NULL) {
            dictht *ht = &iter->d->ht[iter->table];
            if (iter->index == -1 && iter->table == 0) {
                if (iter->safe)
                    iter->d->iterators++;
                else
                    iter->fingerprint = dictFingerprint(iter->d);
            }
            iter->index++;
            if (iter->index >= (long) ht->size) {
                if (dictIsRehashing(iter->d) && iter->table == 0) {
                    iter->table++;
                    iter->index = 0;
                    ht = &iter->d->ht[1];
                } else {
                    break;
                }
            }
            iter->entry = ht->table[iter->index];
        } else {
            iter->entry = iter->nextEntry;
        }
        if (iter->entry) {
            /* We need to save the 'next' here, the iterator user
             * may delete the entry we are returning. */
            iter->nextEntry = iter->entry->next;
            return iter->entry;
        }
    }
    return NULL;
}

void dictReleaseIterator(dictIterator *iter)
{
    if (!(iter->index == -1 && iter->table == 0)) {
        if (iter->safe)
            iter->d->iterators--;
        //else
            //assert(iter->fingerprint == dictFingerprint(iter->d));
    }
    zfree(iter);	
}

dictEntry *dictGetRandomKey(dict *d)
{
    dictEntry *he, *orighe;
    unsigned long h;
    int listlen, listele;

    if (dictSize(d) == 0) return NULL;
    if (dictIsRehashing(d)) _dictRehashStep(d);
    if (dictIsRehashing(d)) {
        do {
            /* We are sure there are no elements in indexes from 0
             * to rehashidx-1 */
            h = d->rehashidx + (random() % (dictSlots(d) - d->rehashidx));
            he = (h >= d->ht[0].size) ? d->ht[1].table[h - d->ht[0].size] :
                                      d->ht[0].table[h];
        } while(he == NULL);
    } else {
        do {
            h = random() & d->ht[0].sizemask;
            he = d->ht[0].table[h];
        } while(he == NULL);
    }

    /* Now we found a non empty bucket, but it is a linked
     * list and we need to get a random element from the list.
     * The only sane way to do so is counting the elements and
     * select a random index. */
    listlen = 0;
    orighe = he;
    while(he) {
        he = he->next;
        listlen++;
    }
    listele = random() % listlen;
    he = orighe;
    while(listele--) he = he->next;
    return he;
}

uint64_t hashCallback(const void *key)
{
	return dictGenHashFunction((unsigned char *)key, strlen((char*)key));
}

int compareCallback(void *privdata, const void *key1, const void *key2)
{
	int l1, l2;
	DICT_NOTUSED(privdata);
	
	l1 = strlen((char *)key1);
	l2 = strlen((char *)key2);
	printf("l1 is %d, l2 is %d, key1 is %s, key2 is %s, %d, %s\n", l1, l2, (char *)key1, (char *)key2, __LINE__, __func__);
	if(l1 != l2) return 0;
	return memcmp(key1, key2, l1) == 0;
}


dictType BenchmarkDictType = {
	hashCallback,
	NULL,
	NULL,
	compareCallback,
	NULL,
	//freeCallback,
	NULL
};

void dictEnableResize(void)
{
	dict_can_resize = 1;
}

void dictDisableResize(void){
	dict_can_resize = 0;
}

void dictEmpty(dict *d, void(callback)(void*))
{
	_dictClear(d, &d->ht[0], callback);
	_dictClear(d, &d->ht[1], callback);
	d->rehashidx = -1;
	d->iterators = 0;
}

/*--------------------------------Debugging-------------------------------*/

#define DICT_STATS_VECTLEN 50
size_t _dictGetStatsHt(char *buf, size_t bufsize, dictht *ht, int tableid){
	
	unsigned long i, slots = 0, chainlen, maxchainlen = 0;
	unsigned long totchainlen = 0;
	unsigned long clvector[DICT_STATS_VECTLEN];
	size_t l = 0;
	
	if(ht->used == 0){
		return snprintf(buf, bufsize, "No stats available for empty dictionaries\n");
	}
	
	for(i = 0; i < DICT_STATS_VECTLEN; i++) clvector[i] = 0;
	for(i = 0; i < ht->size; i++){
		dictEntry *he;
		
		if(ht->table[i] == NULL){
			clvector[0]++;
			continue;
		}
		slots++;
		
		chainlen = 0;
		he = ht->table[i];
		while(he){
			chainlen++;
			he = he->next;
		}
		clvector[(chainlen < DICT_STATS_VECTLEN) ? chainlen : (DICT_STATS_VECTLEN-1)]++;
		if(chainlen > maxchainlen) maxchainlen = chainlen;
		totchainlen += chainlen;
	}
	
	/*Generate human readable stats.*/
    l += snprintf(buf+l,bufsize-l,
        "Hash table %d stats (%s):\n"
        " table size: %ld\n"
        " number of elements: %ld\n"
        " different slots: %ld\n"
        " max chain length: %ld\n"
        " avg chain length (counted): %.02f\n"
        " avg chain length (computed): %.02f\n"
        " Chain length distribution:\n",
        tableid, (tableid == 0) ? "main hash table" : "rehashing target",
        ht->size, ht->used, slots, maxchainlen,
        (float)totchainlen/slots, (float)ht->used/slots);

    for (i = 0; i < DICT_STATS_VECTLEN-1; i++) {
        if (clvector[i] == 0) continue;
        if (l >= bufsize) break;
        l += snprintf(buf+l,bufsize-l,
            "   %s%ld: %ld (%.02f%%)\n",
            (i == DICT_STATS_VECTLEN-1)?">= ":"",
            i, clvector[i], ((float)clvector[i]/ht->size)*100);
    }

    /* Unlike snprintf(), return the number of characters actually written. */
    if (bufsize) buf[bufsize-1] = '\0';
    return strlen(buf);
	
}

void dictGetStats(char *buf, size_t bufsize, dict *d)
{
	size_t l;
	char *orig_buf = buf;
	size_t orig_bufsize = bufsize;
	
	l = _dictGetStatsHt(buf, bufsize, &d->ht[0], 0);
	buf += l;
	bufsize -= l;
	if(dictIsRehashing(d) && bufsize > 0){
		_dictGetStatsHt(buf, bufsize, &d->ht[1], 1);
	}
	
	if(orig_bufsize) orig_buf[orig_bufsize - 1] = '\0';
}

int main(void)
{
	int i;
	int retval;
	dictEntry *de = NULL;
	char input[13] = "hello world1";
	char *array[6] = {"121","123", "124", "125", "126", "127"};
	dict *dict = dictCreate(&BenchmarkDictType, NULL);
	for(i = 0; i < 3; i++)
	{
		retval = dictAdd(dict, array[i], "hello world");
		if(retval != DICT_OK){

			printf("add failed\n");
		}
		else {
	
			printf("add success\n");	
		}
	}
	printf("%s, %d\n", __func__, __LINE__);
	return 0;
}










