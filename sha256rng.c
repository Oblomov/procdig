/* Pseudo-random number generator that uses SHA256 hashing to produce
 * random byte. Starting from an initial (possibly empty) seed(s), it
 * generates new bytes by hashing the pool so far.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/sha.h>

typedef unsigned char uchar;

uchar *pool;
size_t pool_size;
size_t pool_use;
size_t pool_cursor;
static const size_t min_sz = SHA256_DIGEST_LENGTH;

/* Make sure the pool has room for at least another digest */
void prepare_pool()
{
	/* If we have enough room for another hash, do nothing */
	if (pool_size - pool_use > min_sz)
		return;
	/* We need to enlarge the pool; first make sure there is room */
	if (SIZE_MAX - min_sz < pool_size)
	{
		fprintf(stderr, "out of size");
		abort();
	}
	size_t increment = pool_size ? min_sz : min_sz*min_sz;
	while (increment < pool_size && SIZE_MAX - increment < pool_size)
	{
		increment *= 2;
	}
	uchar *pnew = realloc(pool, pool_size + min_sz);
	if (pnew == NULL)
	{
		fprintf(stderr, "out of memory");
		abort();
	}
	pool = pnew;
	pool_size += min_sz;
}

/* Shift the pool backwards to start from the pool_cursor */
void shift_pool()
{
#ifdef DEBUG
	fprintf(stderr, "pre-shift: %zu %zu %zu | %u | %u\n",
		pool_cursor, pool_use, pool_size,
		pool[pool_cursor], pool[pool_use - 1]);
#endif
	/* Assumes pool_cursor >= pool_size/2 */
	memcpy(pool, pool + pool_cursor, pool_size - pool_cursor);
	pool_use -= pool_cursor;
	pool_cursor = 0;
#ifdef DEBUG
	fprintf(stderr, "post-shift: %zu %zu %zu | %u | %u\n",
		pool_cursor, pool_use, pool_size,
		pool[pool_cursor], pool[pool_use - 1]);
#endif
}

/* Add the pool hash to the pool itself */
void repool()
{
#ifdef DEBUG
	fputs("repooling\n", stderr);
#endif
	prepare_pool();
	SHA256(pool, pool_use, pool + pool_use);
	pool_use += min_sz;
}

void pool_str(const char *arg)
{
#ifdef DEBUG
	fprintf(stderr, "pooling '%s'", arg);
#endif
	prepare_pool();
	SHA256((const uchar *)arg, strlen(arg), pool + pool_use);
	pool_use += min_sz;
}

/* produce a random byte from the pool, optionally enlarging the pool
 * if necessary */
void consume()
{
	if (pool_use - pool_cursor < min_sz)
		repool();
	fwrite(pool + (pool_cursor++), sizeof(uchar), 1, stdout);
	if (pool_cursor > pool_size/2)
		shift_pool();
}


int main(int argc, char *argv[])
{
	if (argc == 1)
		repool();
	else while (--argc)
	{
		pool_str(*(++argv));
	}

	long long limit = SIZE_MAX;
	const char *limit_env = getenv("SHA256RNG_LIMIT");
	if (limit_env && *limit_env) {
		limit = atoll(limit_env);
		fprintf(stderr, "SHA256 RNG limited to %llu bytes\n",
			limit);
		fflush(stderr);
	}

	while (limit--)
		consume();
}
