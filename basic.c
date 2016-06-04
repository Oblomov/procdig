/* Basic example: generate a sequence of heights based off a seed.
 * Illustrate for 257 seeds (null string, and unsigned values 0 to 255),
 * using multiple height generation functions (scaling, modulus) and
 * multiple smoothing functions (none, weighted, modulus).
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>

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

/* Function type to extract a value in the range [0,255] from an
 * array of values, at the given location. */
typedef uchar (*extract_fn)(uchar*, size_t array_size, size_t index);

/* The simplest extract function: get the requested value */
static uchar PURE extract_value(uchar *ar, size_t array_size,
	size_t index)
{
	/* the `% array_size` is for safety, in this sample code we know
	 * we don't go out of bounds so we could just use `index`
	 */
	return ar[index % array_size];
}

/* A function that adds previous, self and next —note that the result
 * here depends on the machine, but normally this would be just modulus.
 * This could be made explicit by adding up in a higher-sized value and
 * then doing the modulus manually
 */
static uchar PURE extract_plus(uchar *ar, size_t array_size,
	size_t index)
{
	size_t prev = (index > 0 ?  index - 1 : array_size - 1);
	size_t next = index + 1;
	return ar[prev % array_size] +
		ar[index % array_size] +
		ar[(index+1) % array_size];
}

/* A function that takes the three-points average of the previous,
 * current and next value.
 */
static uchar PURE extract_avg(uchar *ar, size_t array_size,
	size_t index)
{
	size_t prev = (index > 0 ?  index - 1 : array_size - 1);
	size_t next = index + 1;
	/* note that we add in a uint to avoid overflows
	 * (this assumes sizeof(uint) > sizeof(uchar) */
	uint val = ar[prev % array_size];
	val += ar[index % array_size];
	val += ar[(index+1) % array_size];
	val /= 3;

	/* note that val is now guaranteed to be <= UCHAR_MAX */

	return val;
}

/* A function that takes the three-points average of the previous,
 * current and next value, but weighting the central value more than
 * the other two.
 */
static uchar PURE extract_avg2(uchar *ar, size_t array_size,
	size_t index)
{
	size_t prev = (index > 0 ?  index - 1 : array_size - 1);
	size_t next = index + 1;
	/* note that we add in a uint to avoid overflows
	 * (this assumes sizeof(uint) > sizeof(uchar) */
	uint val = ar[prev % array_size];
	val += ar[index % array_size];
	val += ar[index % array_size]; // add the central value twice
	val += ar[(index+1) % array_size];
	val /= 4;

	/* note that val is now guaranteed to be <= UCHAR_MAX */

	return val;
}

/* An extract function and its name */

struct extractmode
{
	extract_fn func;
	char *name;
};

/* Collection of extraction functions */

static const struct extractmode extractmodes[] = {
	{ extract_value, "Self value" },
	{ extract_plus, "Sum of -1 0 +1" },
	{ extract_avg, "3-point avg weight (1, 1, 1)" },
	{ extract_avg2, "3-point avg weight (1, 2, 1)" }
};

static const size_t num_extractmodes = ARRAY_SIZE(extractmodes);

/* Render via sparklines the given hash of the given length,
 * using the given spark function and the given extract mode
 */
static void render(uchar *sha, size_t len,
	spark_fn spark, extract_fn ext)
{
	for (size_t i = 0; i < len; ++i) {
		uchar val = ext(sha, len, i);
		fputs(spark(val), stdout);
	}
}


/* Render via sparklines the SHA256 hash of the given sequence of
 * unsigned bytes, using each available spark function and extraction
 * mode
 */
static void render_all(uchar *src, size_t len)
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
		for (size_t e = 0; e < num_extractmodes; ++e) {
			extract_fn ext = extractmodes[e].func;
			render(sha256, SHA256_DIGEST_LENGTH,
				spark, ext);
			const bool last = (s == num_sparkmodes - 1 &&
				e == num_extractmodes - 1);
			if (!last)
				fputs("\t", stdout);
		}
	}
}

int main(int argc UNUSED, char *argv[] UNUSED)
{
	uchar src[] = { 0 };

	/* Header */
	printf("    \t");
	for (size_t s = 0; s < num_sparkmodes; ++s)
	{
		int toplen = (SHA256_DIGEST_LENGTH + 8)*
			num_extractmodes;
		printf("%-*s", toplen, sparkmodes[s].name);
		if (s == num_sparkmodes - 1)
			fputs("\n", stdout);
	}
	printf("    \t");
	for (size_t s = 0; s < num_sparkmodes; ++s)
	{
		for (size_t e = 0; e < num_extractmodes; ++e) {
			printf("%-*s", SHA256_DIGEST_LENGTH,
				extractmodes[e].name);
			const bool last = (s == num_sparkmodes - 1 &&
				e == num_extractmodes - 1);
			if (!last)
				fputs("\t", stdout);
			else
				fputs("\n", stdout);
		}
	}

	printf("\n----\t");
	render_all(src, 0);
	printf("\t");
	for (uint v = 0; v <= UCHAR_MAX; ++v)
	{
		src[0] = v;
		printf("\n\n%4u\t", v);
		render_all(src, 1);
	}
	puts("");

	return 0;
}
