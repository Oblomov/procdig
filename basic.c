/* Basic example: generate a sequence of heights based off a seed.
 * Illustrate for 257 seeds (null string, and unsigned values 0 to 255).
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <openssl/sha.h>

#define PURE __attribute__((pure))
#define UNUSED __attribute__((unused))

typedef unsigned char uchar;
typedef unsigned int uint;

/* Space and Unicode blocks U+2581 to U+2588 to show
 * height in console */
static const char * const sparktable[]=
{
	" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"
};
static const size_t num_sparks = sizeof(sparktable)/sizeof(*sparktable);
static const size_t num_uchars = (size_t)UCHAR_MAX + 1;

/* Map a value from 0 to 255 to a sparktable address */
static const char * PURE spark(uchar val)
{
	return sparktable[(val*num_sparks)/num_uchars];
}

/* Render via sparklines the SHA256 hash of the given sequence of
 * unsigned bytes
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

	for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i)
		fputs(spark(sha256[i]), stdout);
}

int main(int argc UNUSED, char *argv[] UNUSED)
{
	uchar src[] = { 0 };

	printf("---- ");
	render(src, 0);
	for (uint v = 0; v <= UCHAR_MAX; ++v)
	{
		src[0] = v;
		printf("\n%4u ", v);
		render(src, 1);
	}
	puts("");

	return 0;
}
