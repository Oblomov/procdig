/* Basic example: generate a sequence of heights based off a seed.
 * Illustrate for 257 seeds (null string, and unsigned values 0 to 255),
 * using multiple height generation functions (scaling, modulus) and
 * multiple smoothing functions (none, weighted, modulus).
 *
 * TODO:
 *   * produce 4, 8, 16 or 64 heights by decoding the hash as a sequence
 *     of uint64_t, uint32_t, uint16_t and nibbles;
 *   * low-pass filter: still produce 32 heights, but only using the
 *     lower nibbles, reserving the upper ones for something else (e.g.
 *     color).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>

#include <openssl/sha.h>

#define PURE __attribute__((pure))
#define UNUSED __attribute__((unused))
#define ARRAY_SIZE(ar) (sizeof(ar)/sizeof(*ar))

#define NIBBLE_SHIFT (CHAR_BIT/2)
#define NIBBLE_MAX ((1 << NIBBLE_SHIFT) - 1)
#define NIBBLE_MASK NIBBLE_MAX

typedef unsigned char uchar;
typedef unsigned int uint;

/* Handle internal errors showing an error message */
#define FATAL(fmt, ...) do { \
	fflush(stdout); fflush(stderr); \
	fprintf(stderr, "\n%s:%u:%s" fmt "\n", __FILE__, __LINE__, \
		__func__, ##__VA_ARGS__); \
	abort(); \
} while(0)

/* An encmap is a sequence of data. For simplicity, we limit ourselves
 * to data that fits within an unsigned char, although we might actually
 * use less than the full width of the type. A maxval property tell us
 * how much we're actually using, and a count property tells us
 * how many elements are in the data array
 */
struct encmap {
	uchar *data;
	size_t count; // number of elements
	size_t maxval; //  maximum value in the data range
};

#define ENC_ALLOC(encptr, cnt) do { \
	(encptr)->count = cnt; \
	(encptr)->data = calloc(cnt, sizeof(uchar)); \
	if ((encptr)->data == NULL) \
		FATAL("failed to allocate output data"); \
} while(0)
#define ENC_FREE(encptr) free((encptr)->data)

/* Ultimately, we want to visualize our results as a set of height.
 * Space and Unicode blocks U+2581 to U+2588 to show height in console
 */
static const char * const sparktable[]=
{
	" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"
};
static const size_t num_sparks = ARRAY_SIZE(sparktable);
static const size_t sparks_max = ARRAY_SIZE(sparktable) - 1;

/* Print an encmap with compatible maxvals using sparklines */
static void fspark_encmap(FILE *io, struct encmap const *map)
{
	if (map->maxval > sparks_max)
		FATAL("cannot show oversized map (%zu > %zu)",
			map->maxval, sparks_max);
	for (size_t i = 0; i < map->count; ++i)
		fputs(sparktable[map->data[i]], io);
}

/* Assume stdout as output stream */
static inline void spark_encmap(struct encmap const *map)
{
	fspark_encmap(stdout, map);
}

/* A filter function is just a function that reads an encmap and
 * produces a new encmap. No condition are posed on the kind of
 * transformations allowed. Note that the data field in the output
 * encmap will be allocated by the filter function, without freeing it
 * beforehand. The count and maxval field may be initialized by the
 * caller to pass information to the filter.
 */
typedef void (*filter_fn)(struct encmap *out, struct encmap const *in);

/* A filter has a filter function and a name */
struct filter
{
	filter_fn func;
	char *name;
};

/*
 * Filters to map hash values to height values
 */

/* Linear scaling: assumes out->maxval was set by the caller */
static void linear_scale(
	struct encmap *out,
	struct encmap const *in)
{
	size_t count = in->count;
	ENC_ALLOC(out, count);

	for (size_t i = 0; i < count; ++i)
		out->data[i] = (in->data[i]*out->maxval)/in->maxval;
}

/* Modular map: assumes out->maxval was set by the caller */
static void mod_map(
	struct encmap *out,
	struct encmap const *in)
{
	size_t count = in->count;
	ENC_ALLOC(out, count);

	for (size_t i = 0; i < count; ++i)
		out->data[i] = (in->data[i] % out->maxval);
}

/* Collection of height filters */

static const struct filter height_filters[] = {
	{ linear_scale, "Linear scaling" },
	{ mod_map, "Modular map" }
};

static const size_t num_height_filters = ARRAY_SIZE(height_filters);

/*
 * Filters to pre-process hashes or post-process heights
 */

/* Identity */
static void identity(
	struct encmap *out,
	struct encmap const *in)
{
	size_t count = in->count;
	ENC_ALLOC(out, count);
	out->maxval = in->maxval;

	memcpy(out->data, in->data, count*sizeof(uchar));
}


/* Low-pass: take only the lower nibble of a char */
static void lower_nibble(
	struct encmap *out,
	struct encmap const *in)
{
	size_t count = in->count;
	ENC_ALLOC(out, count);
	out->maxval = NIBBLE_MAX;

	for (size_t i = 0; i < count; ++i)
		out->data[i] = (in->data[i] & NIBBLE_MASK);
}

/* High-pass: take only the upper nibble of a char */
static void upper_nibble(
	struct encmap *out,
	struct encmap const *in)
{
	size_t count = in->count;
	ENC_ALLOC(out, count);
	out->maxval = NIBBLE_MAX;

	for (size_t i = 0; i < count; ++i)
		out->data[i] = ((in->data[i] >> NIBBLE_SHIFT) & NIBBLE_MASK);
}

/* Nibble sum: add upper and lower nibble of a char */
static void nibble_sum(
	struct encmap *out,
	struct encmap const *in)
{
	size_t count = in->count;
	ENC_ALLOC(out, count);
	out->maxval = 2*NIBBLE_MAX - 1;

	for (size_t i = 0; i < count; ++i)
	{
		uchar d = in->data[i];
		uchar n = d & NIBBLE_MASK;
		n += ((d >> NIBBLE_SHIFT) & NIBBLE_MASK);
		out->data[i] = n;
	}
}

/* Three-point add and modulus: add the current value to the previous
 * and next (wrapping around the domain) and take the result modulus the
 * maxval
 */
static void three_pt_addmod(
	struct encmap *out,
	struct encmap const *in)
{
	size_t count = in->count;
	ENC_ALLOC(out, count);
	out->maxval = in->maxval;

	for (size_t i = 0; i < count; ++i) {
		size_t prev = (i == 0 ? count - 1 : i - 1);
		size_t next = (i == count - 1 ? 0 : i + 1);
		/* add as uint to avoid overflows */
		uint val = in->data[prev];
		val += in->data[i];
		val += in->data[next];
		out->data[i] = val % out->maxval;
	}
}

/* Three-point average: take the average of the current, previous and
 * next value (wrapping around the domain)
 */
static void three_pt_avg(
	struct encmap *out,
	struct encmap const *in)
{
	size_t count = in->count;
	ENC_ALLOC(out, count);
	out->maxval = in->maxval;

	for (size_t i = 0; i < count; ++i) {
		size_t prev = (i == 0 ? count - 1 : i - 1);
		size_t next = (i == count - 1 ? 0 : i + 1);
		/* add as uint to avoid overflows */
		uint val = in->data[prev];
		val += in->data[i];
		val += in->data[next];
		out->data[i] = val/3;
	}
}

/* Three-point average 2: take the average of the current, previous and
 * next value (wrapping around the domain), weighting the current value
 * double the others.
 */
static void three_pt_avg2(
	struct encmap *out,
	struct encmap const *in)
{
	size_t count = in->count;
	ENC_ALLOC(out, count);
	out->maxval = in->maxval;

	for (size_t i = 0; i < count; ++i) {
		size_t prev = (i == 0 ? count - 1 : i - 1);
		size_t next = (i == count - 1 ? 0 : i + 1);
		/* add as uint to avoid overflows */
		uint val = in->data[prev];
		val += in->data[i];
		val += in->data[i];
		val += in->data[next];
		out->data[i] = val/4;
	}
}

/* Collection of pre- and post-processing filters */

static const struct filter process_filters[] = {
	{ identity, "Identity"},
#if 0
	/* Nibble filters are commented because they only make sense for
	 * preprocessing, so we need a way to specify pre- or post-
	 * processing-only filters
	 */
	{ lower_nibble, "Lower nibble"},
	{ upper_nibble, "Upper nibble"},
	{ nibble_sum, "Nibble sum"},
#endif
	{ three_pt_addmod, "3-point add+mod"},
	{ three_pt_avg, "3-point average (1, 1, 1)"},
	{ three_pt_avg2, "3-point average (1, 2, 1)"}
};

static const size_t num_process_filters = ARRAY_SIZE(process_filters);

/* Create (and show) every combination of preprocess + height +
 * postprocess filter, starting with the SHA256 of the given byte
 * sequence `src` of given length `len`.
 */
static void render_all(uchar *src, size_t len)
{
	/* We are going to need the base hash (fixed),
	 * and three more encmaps: the result of the preprocessing,
	 * the result of the height calculation, and the result
	 * of the post-processing: these will be allocated by the
	 * filters, but we need to free them ourselves.
	 */
	struct encmap base_hash, preprocessed, heights, postprocessed;
	ENC_ALLOC(&base_hash, SHA256_DIGEST_LENGTH);
	base_hash.maxval = UCHAR_MAX;

	SHA256(src, len, base_hash.data);
#if 0 /* debug */
	for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i)
		printf("| %2x ", base_hash.data[i]);
	puts("|\n");
#endif

	for (size_t pre = 0; pre < num_process_filters; ++pre)
	{
		process_filters[pre].func(&preprocessed, &base_hash);
		for (size_t hf = 0; hf < num_height_filters; ++hf)
		{
			/* The only thing we need to set for the intermediate encmaps
			 * is the maxval we want in the heights
			 */
			heights.maxval = sparks_max;
			height_filters[hf].func(&heights, &preprocessed);
			for (size_t post = 0; post < num_process_filters; ++post)
			{
				process_filters[post].func(
					&postprocessed, &heights);
				spark_encmap(&postprocessed);
				const bool last = (
					post == num_process_filters - 1 &&
					hf == num_height_filters - 1 &&
					pre == post);
				if (!last)
					fputs("\t", stdout);
				fflush(stdout);
				ENC_FREE(&postprocessed);
			}
			ENC_FREE(&heights);
		}
		ENC_FREE(&preprocessed);
	}
	ENC_FREE(&base_hash);
}

int main(int argc UNUSED, char *argv[] UNUSED)
{
	uchar src[] = { 0 };

	/* Header */
	printf("    \t");
	for (size_t s = 0; s < num_process_filters; ++s)
	{
		int toplen = (SHA256_DIGEST_LENGTH + 8)*
			num_height_filters*num_process_filters;
		printf("%-*s", toplen, process_filters[s].name);
		if (s == num_process_filters - 1)
			fputs("\n", stdout);
	}
	printf("    \t");
	for (size_t s = 0; s < num_process_filters; ++s)
	{
		for (size_t h = 0; h < num_height_filters; ++h)
		{
			int toplen = (SHA256_DIGEST_LENGTH + 8)*
				num_process_filters;
			printf("%-*s", toplen, height_filters[h].name);
		}
		if (s == num_process_filters - 1)
			fputs("\n", stdout);
	}
	printf("    \t");
	for (size_t s = 0; s < num_process_filters; ++s)
	{
		for (size_t h = 0; h < num_height_filters; ++h)
		{
			for (size_t t = 0; t < num_process_filters; ++t)
			{
				printf("%-*s", SHA256_DIGEST_LENGTH,
					process_filters[t].name);
				const bool last = (
					t == num_process_filters - 1 &&
					h == num_height_filters - 1 &&
					s == t);
				if (!last)
					fputs("\t", stdout);
				else
					fputs("\n", stdout);
			}
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
