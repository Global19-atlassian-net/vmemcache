/*
 * Copyright 2018-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * vmemcache_test_basic.c -- basic unit test for libvmemcache
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libvmemcache.h"
#include "test_helpers.h"

#define VMEMCACHE_EXTENT ((int)VMEMCACHE_MIN_EXTENT)
#define LEN (VMEMCACHE_EXTENT)
#define KSIZE LEN /* key size */
#define VSIZE LEN /* value size */
#define DNUM 10 /* number of data */

#define SIZE_1K 1024

/* type of statistics */
typedef unsigned long long stat_t;

/* names of statistics */
static const char *stat_str[VMEMCACHE_STATS_NUM] = {
	"PUTs",
	"GETs",
	"HITs",
	"MISSes",
	"EVICTs",
	"CACHE_ENTRIES",
	"DRAM_SIZE_USED",
	"POOL_SIZE_USED",
	"HEAP_ENTRIES",
};

/* context of callbacks */
struct ctx_cb {
	char vbuf[VSIZE];
	size_t vbufsize;
	size_t vsize;
	stat_t miss_count;
	stat_t evict_count;
};

/* test_put_in_evict callback context */
struct put_evict_cb {
	char *vbuf;
	size_t vsize;
	stat_t n_puts;
	stat_t n_evicts_stack;
	stat_t cb_key;
	stat_t max_evicts_stack;
	stat_t max_puts;
};

/* key bigger than 1kB */
struct big_key {
	char buf[SIZE_1K];
	stat_t n_puts;
};

/*
 * verify_stats -- (internal) verify statistics
 */
static void
verify_stats(VMEMcache *cache, stat_t put, stat_t get, stat_t hit, stat_t miss,
		stat_t evict, stat_t entries, stat_t dram, stat_t pool)
{
	stat_t stat;
	int ret;

	ret = vmemcache_get_stat(cache, VMEMCACHE_STAT_PUT,
			&stat, sizeof(stat));
	if (ret == -1)
		UT_FATAL("vmemcache_get_stat: %s", vmemcache_errormsg());
	if (stat != put)
		UT_FATAL(
			"vmemcache_get_stat: wrong statistic's (%s) value: %llu (should be %llu)",
			stat_str[VMEMCACHE_STAT_PUT], stat, put);

	ret = vmemcache_get_stat(cache, VMEMCACHE_STAT_GET,
			&stat, sizeof(stat));
	if (ret == -1)
		UT_FATAL("vmemcache_get_stat: %s", vmemcache_errormsg());
	if (stat != get)
		UT_FATAL(
			"vmemcache_get_stat: wrong statistic's (%s) value: %llu (should be %llu)",
			stat_str[VMEMCACHE_STAT_GET], stat, get);

	ret = vmemcache_get_stat(cache, VMEMCACHE_STAT_HIT,
			&stat, sizeof(stat));
	if (ret == -1)
		UT_FATAL("vmemcache_get_stat: %s", vmemcache_errormsg());
	if (stat != hit)
		UT_FATAL(
			"vmemcache_get_stat: wrong statistic's (%s) value: %llu (should be %llu)",
			stat_str[VMEMCACHE_STAT_HIT], stat, hit);

	ret = vmemcache_get_stat(cache, VMEMCACHE_STAT_MISS,
			&stat, sizeof(stat));
	if (ret == -1)
		UT_FATAL("vmemcache_get_stat: %s", vmemcache_errormsg());
	if (stat != miss)
		UT_FATAL(
			"vmemcache_get_stat: wrong statistic's (%s) value: %llu (should be %llu)",
			stat_str[VMEMCACHE_STAT_MISS], stat, miss);

	ret = vmemcache_get_stat(cache, VMEMCACHE_STAT_EVICT,
			&stat, sizeof(stat));
	if (ret == -1)
		UT_FATAL("vmemcache_get_stat: %s", vmemcache_errormsg());
	if (stat != evict)
		UT_FATAL(
			"vmemcache_get_stat: wrong statistic's (%s) value: %llu (should be %llu)",
			stat_str[VMEMCACHE_STAT_EVICT], stat, evict);

	ret = vmemcache_get_stat(cache, VMEMCACHE_STAT_ENTRIES,
			&stat, sizeof(stat));
	if (ret == -1)
		UT_FATAL("vmemcache_get_stat: %s", vmemcache_errormsg());
	if (stat != entries)
		UT_FATAL(
			"vmemcache_get_stat: wrong statistic's (%s) value: %llu (should be %llu)",
			stat_str[VMEMCACHE_STAT_ENTRIES], stat, entries);

	ret = vmemcache_get_stat(cache, VMEMCACHE_STAT_DRAM_SIZE_USED,
			&stat, sizeof(stat));
	if (ret == -1)
		UT_FATAL("vmemcache_get_stat: %s", vmemcache_errormsg());
	if (stat != dram)
		UT_FATAL(
			"vmemcache_get_stat: wrong statistic's (%s) value: %llu (should be %llu)",
			stat_str[VMEMCACHE_STAT_DRAM_SIZE_USED], stat, dram);

	ret = vmemcache_get_stat(cache, VMEMCACHE_STAT_POOL_SIZE_USED,
			&stat, sizeof(stat));
	if (ret == -1)
		UT_FATAL("vmemcache_get_stat: %s", vmemcache_errormsg());
	if (stat != pool)
		UT_FATAL(
			"vmemcache_get_stat: wrong statistic's (%s) value: %llu (should be %llu)",
			stat_str[VMEMCACHE_STAT_POOL_SIZE_USED], stat, pool);

	ret = vmemcache_get_stat(cache, VMEMCACHE_STATS_NUM,
					&stat, sizeof(stat));
	if (ret != -1)
		UT_FATAL(
			"vmemcache_get_stat() succeeded for incorrect statistic (-1)");
}

/*
 * verify_stat_entries -- (internal) verify the statistic
 *                                   'current number of cache entries'
 */
static void
verify_stat_entries(VMEMcache *cache, stat_t entries)
{
	stat_t stat;
	int ret;

	ret = vmemcache_get_stat(cache, VMEMCACHE_STAT_ENTRIES,
			&stat, sizeof(stat));
	if (ret == -1)
		UT_FATAL("vmemcache_get_stat: %s", vmemcache_errormsg());
	if (stat != entries)
		UT_FATAL(
			"vmemcache_get_stat: wrong statistic's (%s) value: %llu (should be %llu)",
			stat_str[VMEMCACHE_STAT_ENTRIES], stat, entries);
}

/*
 * verify_heap_entries -- (internal) verify the statistic
 *                                   'current number of heap entries'
 */
static void
verify_heap_entries(VMEMcache *cache, stat_t entries)
{
	stat_t stat;
	int ret;

	ret = vmemcache_get_stat(cache, VMEMCACHE_STAT_HEAP_ENTRIES,
			&stat, sizeof(stat));
	if (ret == -1)
		UT_FATAL("vmemcache_get_stat: %s", vmemcache_errormsg());
	if (stat != entries)
		UT_FATAL(
			"vmemcache_get_stat: wrong statistic's (%s) value: %llu (should be %llu)",
			stat_str[VMEMCACHE_STAT_HEAP_ENTRIES], stat, entries);
}

/*
 * test_new_delete -- (internal) test _new() and _delete()
 */
static void
test_new_delete(const char *dir, const char *file,
		enum vmemcache_replacement_policy replacement_policy)
{
	VMEMcache *cache;

	/* TEST #1 - minimum values of max_size and extent_size */
	cache = vmemcache_new(dir, VMEMCACHE_MIN_POOL, VMEMCACHE_MIN_EXTENT,
				replacement_policy);
	if (cache == NULL)
		UT_FATAL("vmemcache_new: %s", vmemcache_errormsg());

	vmemcache_delete(cache);

	/* TEST #2 - extent_size = max_size = VMEMCACHE_MIN_POOL */
	cache = vmemcache_new(dir, VMEMCACHE_MIN_POOL, VMEMCACHE_MIN_POOL,
				replacement_policy);
	if (cache == NULL)
		UT_FATAL("vmemcache_new: %s", vmemcache_errormsg());

	vmemcache_delete(cache);

	/* TEST #3 - extent_size == 0 */
	cache = vmemcache_new(dir, VMEMCACHE_MIN_POOL, 0,
				replacement_policy);
	if (cache != NULL)
		UT_FATAL("vmemcache_new did not fail with extent_size == 0");

	/* TEST #4 - extent_size == -1 */
	cache = vmemcache_new(dir, VMEMCACHE_MIN_POOL, (size_t)-1,
				replacement_policy);
	if (cache != NULL)
		UT_FATAL("vmemcache_new did not fail with extent_size == -1");

	/* TEST #5 - extent_size == VMEMCACHE_MIN_EXTENT - 1 */
	cache = vmemcache_new(dir, VMEMCACHE_MIN_POOL, VMEMCACHE_MIN_EXTENT - 1,
				replacement_policy);
	if (cache != NULL)
		UT_FATAL(
			"vmemcache_new did not fail with extent_size == VMEMCACHE_MIN_EXTENT - 1");

	/* TEST #6 - extent_size == max_size + 1 */
	cache = vmemcache_new(dir, VMEMCACHE_MIN_POOL, VMEMCACHE_MIN_POOL + 1,
				replacement_policy);
	if (cache != NULL)
		UT_FATAL(
			"vmemcache_new did not fail with extent_size == max_size + 1");

	/* TEST #7 - size == VMEMCACHE_MIN_POOL - 1 */
	cache = vmemcache_new(dir, VMEMCACHE_MIN_POOL - 1, VMEMCACHE_MIN_EXTENT,
				replacement_policy);
	if (cache != NULL)
		UT_FATAL(
			"vmemcache_new did not fail with size == VMEMCACHE_MIN_POOL - 1");

	/* TEST #8 - size == 0 */
	cache = vmemcache_new(dir, 0, VMEMCACHE_MIN_EXTENT,
				replacement_policy);
	if (cache != NULL)
		UT_FATAL(
			"vmemcache_new did not fail with size == 0");

	/* TEST #9 - size == -1 */
	cache = vmemcache_new(dir, (size_t)-1, VMEMCACHE_MIN_EXTENT,
				replacement_policy);
	if (cache != NULL)
		UT_FATAL(
			"vmemcache_new did not fail with size == -1");

	/* TEST #10 - not a directory, but a file */
	cache = vmemcache_new(file, VMEMCACHE_MIN_POOL, VMEMCACHE_MIN_EXTENT,
				replacement_policy);
	if (cache != NULL)
		UT_FATAL(
			"vmemcache_new did not fail with a file instead of a directory");

	/* TEST #11 - NULL directory path */
	cache = vmemcache_new(NULL, VMEMCACHE_MIN_POOL, VMEMCACHE_MIN_EXTENT,
				replacement_policy);
	if (cache != NULL)
		UT_FATAL(
			"vmemcache_new did not fail with a NULL directory path");

	/* TEST #12 - nonexistent directory path */
	char nonexistent[PATH_MAX];
	strcpy(nonexistent, dir);
	strcat(nonexistent, "/nonexistent_dir");
	cache = vmemcache_new(nonexistent, VMEMCACHE_MIN_POOL,
				VMEMCACHE_MIN_EXTENT, replacement_policy);
	if (cache != NULL)
		UT_FATAL(
			"vmemcache_new did not fail with a nonexistent directory path");
}

/*
 * test_put_get_evict -- (internal) test _put(), _get() and _evict()
 */
static void
test_put_get_evict(const char *dir,
			enum vmemcache_replacement_policy replacement_policy)
{
	VMEMcache *cache;

	cache = vmemcache_new(dir, VMEMCACHE_MIN_POOL, VMEMCACHE_EXTENT,
				replacement_policy);
	if (cache == NULL)
		UT_FATAL("vmemcache_new: %s", vmemcache_errormsg());

	const char *key = "KEY";
	const char *value = "VALUE";

	size_t key_size = strlen(key) + 1;
	size_t val_size = strlen(value) + 1;

	if (vmemcache_put(cache, key, key_size, value, val_size))
		UT_FATAL("vmemcache_put: %s", vmemcache_errormsg());

	verify_stat_entries(cache, 1);

	char vbuf[VMEMCACHE_EXTENT]; /* user-provided buffer */
	size_t vbufsize = VMEMCACHE_EXTENT; /* size of vbuf */
	size_t vsize = 0; /* real size of the object */
	ssize_t ret;

	/* get the only one element */
	ret = vmemcache_get(cache, key, key_size, vbuf, vbufsize, 0, &vsize);
	if (ret < 0)
		UT_FATAL("vmemcache_get: %s", vmemcache_errormsg());

	if ((size_t)ret != val_size)
		UT_FATAL(
			"vmemcache_get: wrong return value: %zi (should be %zu)",
			ret, val_size);

	if (vsize != val_size)
		UT_FATAL(
			"vmemcache_get: wrong size of value: %zi (should be %zu)",
			vsize, val_size);

	if (strncmp(vbuf, value, vsize))
		UT_FATAL("vmemcache_get: wrong value: %s (should be %s)",
			vbuf, value);

	/* evict the only one element */
	switch (replacement_policy) {
	case VMEMCACHE_REPLACEMENT_NONE:
		ret = vmemcache_evict(cache, key, key_size);
		break;
	case VMEMCACHE_REPLACEMENT_LRU:
		ret = vmemcache_evict(cache, NULL, 0);
		break;
	default:
		UT_FATAL("unknown policy: %u", replacement_policy);
		break;
	}

	if (ret == -1)
		UT_FATAL("vmemcache_evict: %s", vmemcache_errormsg());

	/* getting the evicted element should return -1 (no such element) */
	ret = vmemcache_get(cache, key, key_size, vbuf, vbufsize, 0, &vsize);
	if (ret != -1 || errno != ENOENT)
		UT_FATAL("vmemcache_get did not return -1 (no such element)");

	vmemcache_delete(cache);
}

/*
 * on_evict_test_evict_cb -- (internal) 'on evict' callback for test_evict
 */
static void
on_evict_test_evict_cb(VMEMcache *cache, const void *key, size_t key_size,
		void *arg)
{
	struct ctx_cb *ctx = arg;
	ssize_t ret;

	ctx->evict_count++;

	ret = vmemcache_get(cache, key, key_size, ctx->vbuf, ctx->vbufsize, 0,
				&ctx->vsize);
	if (ret < 0)
		UT_FATAL("vmemcache_get");

	if ((size_t)ret != VSIZE)
		UT_FATAL(
			"vmemcache_get: wrong return value: %zi (should be %i)",
			ret, VSIZE);
}

/*
 * on_miss_test_evict_cb -- (internal) 'on miss' callback for test_evict
 */
static void
on_miss_test_evict_cb(VMEMcache *cache, const void *key, size_t key_size,
		void *arg)
{
	struct ctx_cb *ctx = arg;

	ctx->miss_count++;

	size_t size = (key_size <= ctx->vbufsize) ? key_size : ctx->vbufsize;

	memcpy(ctx->vbuf, key, size);
	ctx->vsize = size;
}

/*
 * test_evict -- (internal) test _evict()
 */
static void
test_evict(const char *dir,
		enum vmemcache_replacement_policy replacement_policy)
{
	VMEMcache *cache;
	char vbuf[VSIZE];
	size_t vsize = 0;
	ssize_t ret;

	struct ctx_cb ctx = {"", VSIZE, 0, 0, 0};

	struct kv {
		char key[KSIZE];
		char value[VSIZE];
	} data[DNUM];

	cache = vmemcache_new(dir, VMEMCACHE_MIN_POOL, VMEMCACHE_EXTENT,
				replacement_policy);
	if (cache == NULL)
		UT_FATAL("vmemcache_new: %s", vmemcache_errormsg());

	vmemcache_callback_on_evict(cache, on_evict_test_evict_cb, &ctx);
	vmemcache_callback_on_miss(cache, on_miss_test_evict_cb, &ctx);

	for (int i = 0; i < DNUM; i++) {
		data[i].key[0] = 'k';
		memset(&data[i].key[1], '0' + i, KSIZE - 2);
		data[i].key[KSIZE - 1] = 0;

		data[i].value[0] = 'v';
		memset(&data[i].value[1], '0' + i, VSIZE - 2);
		data[i].value[VSIZE - 1] = 0;

		if (vmemcache_put(cache, data[i].key, KSIZE,
					data[i].value, VSIZE))
			UT_FATAL("vmemcache_put: %s", vmemcache_errormsg());
	}

	verify_stat_entries(cache, DNUM);

	/* TEST #1 - evict the element with index #5 */
	/* stats: evict:1 (get:1 hit:1) */
	ret = vmemcache_evict(cache, data[5].key, KSIZE);
	if (ret == -1)
		UT_FATAL("vmemcache_evict: %s", vmemcache_errormsg());

	if (ctx.vsize != VSIZE)
		UT_FATAL(
			"vmemcache_get: wrong size of value: %zi (should be %i)",
			ctx.vsize, VSIZE);

	/* check if the evicted element is #5 */
	if (strncmp(ctx.vbuf, data[5].value, ctx.vsize))
		UT_FATAL("vmemcache_get: wrong value: %s (should be %s)",
			ctx.vbuf, data[5].value);

	/* TEST #2 - evict the LRU element */
	/* stats: evict:1 (get:1 hit:1) */
	ret = vmemcache_evict(cache, NULL, 0);
	if (ret == -1)
		UT_FATAL("vmemcache_evict: %s", vmemcache_errormsg());

	if (ctx.vsize != VSIZE)
		UT_FATAL(
			"vmemcache_get: wrong size of value: %zi (should be %i)",
			ctx.vsize, VSIZE);

	/* check if the evicted LRU element is #0 */
	if (strncmp(ctx.vbuf, data[0].value, ctx.vsize))
		UT_FATAL("vmemcache_get: wrong value: %s (should be %s)",
			ctx.vbuf, data[0].value);

	/* TEST #3 - get the element with index #1 (to change LRU one to #2) */
	/* stats: get:1 hit:1 */
	ret = vmemcache_get(cache, data[1].key, KSIZE, vbuf, VSIZE,
			0, &vsize);
	if (ret < 0)
		UT_FATAL("vmemcache_get");

	if ((size_t)ret != VSIZE)
		UT_FATAL(
			"vmemcache_get: wrong return value: %zi (should be %i)",
			ret, VSIZE);

	if (vsize != VSIZE)
		UT_FATAL(
			"vmemcache_get: wrong size of value: %zi (should be %i)",
			ctx.vsize, VSIZE);

	/* check if the got element is #1 */
	if (strncmp(vbuf, data[1].value, vsize))
		UT_FATAL("vmemcache_get: wrong value: %s (should be %s)",
			vbuf, data[1].value);

	/* TEST #4 - evict the LRU element (it should be #2 now) */
	/* stats: evict:1 (get:1 hit:1) */
	ret = vmemcache_evict(cache, NULL, 0);
	if (ret == -1)
		UT_FATAL("vmemcache_evict: %s", vmemcache_errormsg());

	if (ctx.vsize != VSIZE)
		UT_FATAL(
			"vmemcache_get: wrong size of value: %zi (should be %i)",
			ctx.vsize, VSIZE);

	/* check if the evicted LRU element is #2 */
	if (strncmp(ctx.vbuf, data[2].value, ctx.vsize))
		UT_FATAL("vmemcache_get: wrong value: %s (should be %s)",
			ctx.vbuf, data[2].value);

	/* TEST #5 - get the evicted element with index #2 */
	/* stats: get:1 miss:1 */
	ret = vmemcache_get(cache, data[2].key, KSIZE, vbuf, VSIZE,
			0, &vsize);

	if (ret != -1)
		UT_FATAL("vmemcache_get succeeded when it shouldn't");

	if (errno != ENOENT)
		UT_FATAL("vmemcache_get: errno %d should be ENOENT", errno);

	/* check if the 'on_miss' callback got key #2 */
	if (strncmp(ctx.vbuf, data[2].key, ctx.vsize))
		UT_FATAL("vmemcache_get: wrong value: %s (should be %s)",
			ctx.vbuf, data[2].key);

	/* TEST #6 - null output arguments */
	/* stats: get:1 hit:1 */
	vmemcache_get(cache, data[2].key, KSIZE, NULL, VSIZE, 0, NULL);

	/* TEST #7 - too large put */
	if (!vmemcache_put(cache, data[2].key, KSIZE, vbuf,
		VMEMCACHE_MIN_POOL + 1)) {
		UT_FATAL("vmemcache_put: too large put didn't fail");
	}

	if (errno != ENOSPC) {
		UT_FATAL(
			"vmemcache_put: too large put returned \"%s\" \"%s\" instead of ENOSPC",
			strerror(errno), vmemcache_errormsg());
	}

	/* TEST #8 - evict nonexistent key */
	const char *non_existent_key = "non_existent";
	ret = vmemcache_evict(cache, non_existent_key,
			strlen(non_existent_key));
	if (ret == 0)
		UT_FATAL(
			"vmemcache_evict: return value for nonexistent key equals 0");
	else if (errno != ENOENT)
		UT_FATAL(
			"vmemcache_evict: nonexistent key: errno %d (should be %d)",
				errno, ENOENT);

	/* free all the memory */
	/* stats: evict:DNUM+1 -3 already evicted, miss:1 */
	while (vmemcache_evict(cache, NULL, 0) == 0)
		;

	/* check statistics */
	verify_stats(cache,
			DNUM, /* put */
			3 + ctx.evict_count, /* get */
			1 + ctx.evict_count, /* hit */
			ctx.miss_count,
			ctx.evict_count, 0, 0, 0);
	UT_ASSERTeq(ctx.miss_count, 2);
	UT_ASSERTeq(ctx.evict_count, DNUM);

	vmemcache_delete(cache);
}

/*
 * on_evict_test_memory_leaks_cb -- (internal) 'on evict' callback for
 *                                   test_memory_leaks
 */
static void
on_evict_test_memory_leaks_cb(VMEMcache *cache,
				const void *key, size_t key_size, void *arg)
{
	stat_t *counter = arg;

	(*counter)++;
}

/*
 * test_memory_leaks -- (internal) test if there are any memory leaks
 */
static void
test_memory_leaks(const char *dir, int key_gt_1K,
			enum vmemcache_replacement_policy replacement_policy)
{
	VMEMcache *cache;
	char *vbuf;
	char *get_buf;
	size_t size;
	size_t vsize;
	int ret;
	ssize_t get_ret;
	struct big_key bk;

	srand((unsigned)time(NULL));

	stat_t n_puts = 0;
	stat_t n_evicts = 0;
	stat_t n_gets = 0;

	size_t min_size = VMEMCACHE_MIN_EXTENT / 2;
	size_t max_size = VMEMCACHE_MIN_POOL / 16;

	cache = vmemcache_new(dir, VMEMCACHE_MIN_POOL, VMEMCACHE_MIN_EXTENT,
				replacement_policy);
	if (cache == NULL)
		UT_FATAL("vmemcache_new: %s", vmemcache_errormsg());

	vmemcache_callback_on_evict(cache, on_evict_test_memory_leaks_cb,
					&n_evicts);

	while (n_evicts < 1000) {
		size = min_size + (size_t)rand() % (max_size - min_size + 1);

		vbuf = malloc(size);
		if (vbuf == NULL)
			UT_FATAL("out of memory");
		memset(vbuf, 42, size - 1);
		vbuf[size - 1] = '\0';

		if (key_gt_1K) {
			memset(bk.buf, 42 /* arbitrary */, sizeof(bk.buf));
			bk.n_puts = n_puts;

			ret = vmemcache_put(cache, &bk, sizeof(bk), vbuf, size);
		} else {
			ret = vmemcache_put(cache, &n_puts, sizeof(n_puts),
						vbuf, size);
		}

		if (ret)
			UT_FATAL(
				"vmemcache_put(n_puts: %llu n_evicts: %llu): %s",
				n_puts, n_evicts, vmemcache_errormsg());

		get_buf = malloc(size);
		if (get_buf == NULL)
			UT_FATAL("out of memory");
		if (key_gt_1K)
			get_ret = vmemcache_get(cache, &bk, sizeof(bk), get_buf,
							size, 0, &vsize);
		else
			get_ret = vmemcache_get(cache, &n_puts, sizeof(n_puts),
							get_buf, size, 0,
							&vsize);
		if (get_ret < 0)
			UT_FATAL("vmemcache_get: %s", vmemcache_errormsg());

		if ((size_t)get_ret != size)
			UT_FATAL(
				"vmemcache_get: wrong return value: %zi (should be %zu)",
				get_ret, size);

		if (size != vsize)
			UT_FATAL(
				"vmemcache_get: wrong size of value: %zi (should be %zu)",
				vsize, size);

		if (strcmp(vbuf, get_buf))
			UT_FATAL(
				"vmemcache_get: wrong value: %s (should be %s)",
				get_buf, vbuf);

		free(vbuf);
		free(get_buf);

		n_gets++;
		n_puts++;
	}

	verify_stat_entries(cache, n_puts - n_evicts);

	/* free all the memory */
	while (vmemcache_evict(cache, NULL, 0) == 0)
		;

	/* check statistics */
	verify_stats(cache, n_puts, n_gets, n_gets, 0, n_evicts, 0, 0, 0);

	vmemcache_delete(cache);

	if (n_evicts != n_puts)
		UT_FATAL("memory leak detected");
}

/*
 * test_merge_allocations -- (internal) test merging allocations
 */
static void
test_merge_allocations(const char *dir,
			enum vmemcache_replacement_policy replacement_policy)
{
	VMEMcache *cache;
	ssize_t ret;

#define N_KEYS 5
	const char *key[N_KEYS] = {
			"KEY_1",
			"KEY_2",
			"KEY_3",
			"KEY_4",
			"KEY_5",
	};

	const char *value = "VALUE";

	size_t key_size = strlen(key[0]) + 1;
	size_t val_size = strlen(value) + 1;

	cache = vmemcache_new(dir, VMEMCACHE_MIN_POOL, VMEMCACHE_EXTENT,
				replacement_policy);
	if (cache == NULL)
		UT_FATAL("vmemcache_new: %s", vmemcache_errormsg());

	verify_stat_entries(cache, 0);
	verify_heap_entries(cache, 1);

	for (int i = 0; i < N_KEYS; i++)
		if (vmemcache_put(cache, key[i], key_size, value, val_size))
			UT_FATAL("vmemcache_put: %s", vmemcache_errormsg());

	verify_stat_entries(cache, N_KEYS);
	verify_heap_entries(cache, 1);

	/* order of evicting the keys */
	const unsigned i_key[N_KEYS] = {1, 3, 0, 4, 2};

	for (int i = 0; i < N_KEYS; i++) {
		ret = vmemcache_evict(cache, key[i_key[i]], key_size);
		if (ret == -1)
			UT_FATAL("vmemcache_evict: %s", vmemcache_errormsg());
	}

	verify_stat_entries(cache, 0);
	verify_heap_entries(cache, 1);

	if (vmemcache_put(cache, key[0], key_size, value, val_size))
		UT_FATAL("vmemcache_put: %s", vmemcache_errormsg());

	verify_stat_entries(cache, 1);
	verify_heap_entries(cache, 1);

	vmemcache_delete(cache);
}

/*
 * on_evict_test_put_in_evict_cb -- (internal) 'on evict' callback
 * for test_put_in_evict
 */
static void
on_evict_test_put_in_evict_cb(VMEMcache *cache, const void *key,
				size_t key_size, void *arg)
{
	struct put_evict_cb *ctx = arg;

	/*
	 * restrict the 'on evict' callbacks stack size to mitigate the risk
	 * of stack overflow
	 */
	if (++ctx->n_evicts_stack > ctx->max_evicts_stack)
		return;

	/*
	 * keys provided by callback should not overlap with keys provided
	 * in main loop
	 */
	ctx->cb_key++;
	int ret = vmemcache_put(cache, &ctx->cb_key, sizeof(ctx->cb_key),
					ctx->vbuf, ctx->vsize);

	if (ret && errno != ENOSPC)
		UT_FATAL("vmemcache_put: %s, key: %d, errno: %d (should be %d)",
			vmemcache_errormsg(), *(int *)key, errno, ENOSPC);
}

/*
 * test_put_in_evict -- (internal) test valid library behaviour for making
 * a put in 'on evict' callback function
 */
static void
test_put_in_evict(const char *dir, enum vmemcache_replacement_policy policy,
					unsigned seed)
{
	size_t min_size = VMEMCACHE_MIN_EXTENT / 2;
	size_t max_size = VMEMCACHE_MIN_POOL / 16;
	stat_t max_puts = 1000;
	stat_t max_evicts_stack = 500;

	srand(seed);
	printf("seed: %u\n", seed);

	VMEMcache *cache = vmemcache_new(dir, VMEMCACHE_MIN_POOL,
		VMEMCACHE_MIN_EXTENT, policy);
	if (cache == NULL)
		UT_FATAL("vmemcache_new: %s", vmemcache_errormsg());

	struct put_evict_cb ctx =
		{NULL, 0, 0, 0, max_puts, max_evicts_stack, max_puts};
	vmemcache_callback_on_evict(cache, on_evict_test_put_in_evict_cb,
					&ctx);

	while (ctx.n_puts < ctx.max_puts) {
		ctx.n_puts++;
		ctx.n_evicts_stack = 0;

		ctx.vsize = get_granular_rand_size(max_size, min_size);
		ctx.vbuf = malloc(ctx.vsize);
		if (ctx.vbuf == NULL)
			UT_FATAL("out of memory");

		int ret = vmemcache_put(cache, &ctx.n_puts, sizeof(ctx.n_puts),
					ctx.vbuf, ctx.vsize);
		if (ret)
			UT_FATAL("vmemcache_put(n_puts: %llu): %s",
						ctx.n_puts,
						vmemcache_errormsg());

		free(ctx.vbuf);
	}

	vmemcache_delete(cache);
}


int
main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s dir-name\n", argv[0]);
		exit(-1);
	}
	const char *dir = argv[1];

	unsigned seed;
	if (argc == 3) {
		if (str_to_unsigned(argv[2], &seed) || seed < 1)
			UT_FATAL("incorrect value of seed: %s", argv[2]);
	} else {
		seed = (unsigned)time(NULL);
	}

	test_new_delete(dir, argv[0], VMEMCACHE_REPLACEMENT_NONE);
	test_new_delete(dir, argv[0], VMEMCACHE_REPLACEMENT_LRU);

	test_put_get_evict(dir, VMEMCACHE_REPLACEMENT_NONE);
	test_put_get_evict(dir, VMEMCACHE_REPLACEMENT_LRU);

	test_evict(dir, VMEMCACHE_REPLACEMENT_LRU);

	/* '0' means: key size < 1kB */
	test_memory_leaks(dir, 0, VMEMCACHE_REPLACEMENT_LRU);

	/* '1' means: key size > 1kB */
	test_memory_leaks(dir, 1, VMEMCACHE_REPLACEMENT_LRU);

	test_merge_allocations(dir, VMEMCACHE_REPLACEMENT_NONE);
	test_merge_allocations(dir, VMEMCACHE_REPLACEMENT_LRU);

	test_put_in_evict(dir, VMEMCACHE_REPLACEMENT_LRU, seed);

	return 0;
}
