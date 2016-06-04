/* Basic example: generate a sequence of heights based off a seed.
 * Illustrate for 257 seeds (null string, and unsigned values 0 to 255).
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <openssl/sha.h>

#define PURE __attribute__((pure))
#define UNUSED __attribute__((unused))
#define ARRAY_SIZE(ar) (sizeof(ar)/sizeof(*ar))

typedef unsigned char uchar;
typedef unsigned int uint;

/* Space and Unicode blocks U+2581 to U+2588 to show
 * height in console */
static const char * const sparktable[]=
{
	" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"
};
static const size_t num_sparks = ARRAY_SIZE(sparktable);
static const size_t sparks_max = ARRAY_SIZE(sparktable) - 1;

/* Function type for a map from a value in the [0, 255] range to a
 * sparktable address */
typedef const char * (*spark_fn)(uchar);

/* Spark function using scaling */
static const char * PURE scalespark(uchar val)
{
	return sparktable[(val*sparks_max)/UCHAR_MAX];
}

/* Spark function using modular math */
static const char * PURE modspark(uchar val)
{
	return sparktable[val % num_sparks];
}

/* A spark function and its name */

struct sparkmode
{
	spark_fn func;
	char *name;
};

/* Collection of spark functions */

static const struct sparkmode sparkmodes[] = {
	{ scalespark, "Linear scaling" },
	{ modspark, "Modulus scaling" }
};

static const size_t num_sparkmodes = ARRAY_SIZE(sparkmodes);

/* Render via sparklines the SHA256 hash of the given sequence of
 * unsigned bytes, using the given spark function
 */
static void render(uchar *src, size_t len)
{
	uchar sha256[SHA256_DIGEST_LENGTH];
	 SHA256(src, len, sha256);
#if 0 /* debug */
	for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i)
		printf("| %2x ", sha256[i]);
	puts("|\n");
#endif

	for (size_t s = 0; s < num_sparkmodes; ++s)
	{
		spark_fn spark = sparkmodes[s].func;
		for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i)
			fputs(spark(sha256[i]), stdout);
		if (s < num_sparkmodes - 1)
			fputs("\t", stdout);
	}
}

int main(int argc UNUSED, char *argv[] UNUSED)
{
	uchar src[] = { 0 };

	/* Header */
	printf("    \t");
	for (size_t s = 0; s < num_sparkmodes; ++s)
	{
		printf("%-*s", SHA256_DIGEST_LENGTH, sparkmodes[s].name);
		if (s < num_sparkmodes - 1)
			fputs("\t", stdout);
		else
			fputs("\n", stdout);
	}

	printf("----\t");
	render(src, 0);
	printf("\t");
	for (uint v = 0; v <= UCHAR_MAX; ++v)
	{
		src[0] = v;
		printf("\n%4u\t", v);
		render(src, 1);
	}
	puts("");

	return 0;
}
