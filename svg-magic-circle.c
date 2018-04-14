/* Magic circle generator.
 *
 * Inspiration from the idea is taken from
 * https://www.reddit.com/r/proceduralgeneration/comments/8c0si5/alchemy_circles_procedural_generator/
 * https://github.com/CiaccoDavide/Alchemy-Circles-Generator
 *
 * Each circle is deterministically generated using the SHA-256 hash of the ‘spell string’,
 * producing SVG output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <math.h>

#include <openssl/sha.h>

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif

#define PURE __attribute__((pure))
#define UNUSED __attribute__((unused))
#define ARRAY_SIZE(ar) (sizeof(ar)/sizeof(*ar))

typedef unsigned char uchar;
typedef unsigned int uint;

/* Handle internal errors showing an error message */
#define FATAL(fmt, ...) do { \
	fflush(stdout); fflush(stderr); \
	fprintf(stderr, "\n%s:%u:%s" fmt "\n", __FILE__, __LINE__, \
		__func__, ##__VA_ARGS__); \
	abort(); \
} while(0)

#define SIDES_MASK 0x7 /* 0b111 */
#define MAX_NVERT 8 /* maximum number of vertices */

/* We use a circle subdivision in 840 = 3*5*7*8 parts,
 * so that even divisions by 7 are not an issue */
#define MAX_BEARING 840

struct control {
	int cx;
	int cy;
	int scale;
	int order;
	int bearing; /* 0 to MAX_BEARING = 0 */
};

void new_pos(struct control *dst, struct control const *src, int delta)
{
	double rad = dst->bearing*M_PI/(MAX_BEARING/2);
	dst->cx = src->cx - delta*sin(rad);
	dst->cy = src->cy - delta*cos(rad);
}

static const char * class[] = {
	"essential",
	"primary",
	"secondary",
	"tertiary",
};

static const int thickness[] = {
	80,
	60,
	40,
	20,
};

/* Each geometry can be drawn in one of two ways:
 * (0) a 'full' drawing, achieved by stroking the path twice,
 * once with thickness `thickness` and once (overstrike)
 * with EXTRA_THICKNESS less
 * (1) a hairline (in which case the thickness control
 * parameter is ignored);
 */

#define HAIRLINE (1U<<(CHAR_BIT-1))
#define EXTRA_THICKNESS 2

/* Everything except for the circle can be flip/rotated: when this flag is enabled,
 * the feature will be replicated, but rotated by either one or two straight
 * angles, depending on the number of vertices */
#define FLIPROT (HAIRLINE >> 1)

/* Compute the circle radius/delta to move from cx/cy to find the vertices
 * considering the thickness of the feature to draw */
int delta(struct control const *pos, bool hairline)
{
	return pos->scale - (hairline ? 0 : thickness[pos->order]/2);

}

/* Print the unused flags */
void print_missing_flags(int flags, int used)
{
	if (flags)
		printf("<!-- flags %#x/%#x ignored -->\n",
			flags, flags | used);
}

void poly_path_spec(struct control const *vertex, int sides)
{
	printf("d='M %d %d", vertex[0].cx, vertex[0].cy);
	for (int i = 1; i < sides; ++i) {
		printf(" L %d %d", vertex[i].cx, vertex[i].cy);
	}
	printf("z' ");
}

void eye_path_spec(struct control const *vertex, int r)
{
	printf("d='M %d %d "
		"A %d %d 0 0 1 %d %d"
		"A %d %d 0 0 1 %d %d"
		"z' ",
		vertex[0].cx, vertex[0].cy,
		r, r, vertex[1].cx, vertex[1].cy,
		r, r, vertex[0].cx, vertex[0].cy);
}


void draw_circle(struct control const *pos, int flags)
{
	const bool hairline = flags & HAIRLINE;
	const int used_flags = flags & HAIRLINE;
	const int dx = delta(pos, hairline);
	const int thick = thickness[pos->order];
	flags &= ~used_flags;

	printf("<g class='%s circle'>\n", class[pos->order]);
	print_missing_flags(flags, used_flags);
	if (hairline) {
		printf("<circle cx='%d' cy='%d' r='%d'/>\n",
			pos->cx, pos->cy, dx);
	} else {
		printf(	"<circle cx='%d' cy='%d' r='%d' stroke-width='%d'/>\n"
			"<circle cx='%d' cy='%d' r='%d' stroke-width='%d' class='overstrike'/>\n",
			pos->cx, pos->cy, dx, thick,
			pos->cx, pos->cy, dx, thick - EXTRA_THICKNESS);
	}
	puts("</g>");
}

void draw_eye(struct control const *pos, int flags)
{
	const bool hairline = flags & HAIRLINE;
	const bool fliprot = flags & FLIPROT;
	const int used_flags = flags & (HAIRLINE | FLIPROT);
	const int dx = delta(pos, hairline);
	const int thick = thickness[pos->order];
	const int r = 3*pos->scale/2;
	flags &= ~used_flags;

	struct control vertex[2];
	vertex[0].bearing = pos->bearing - MAX_BEARING/4;
	vertex[1].bearing = pos->bearing + MAX_BEARING/4;
	new_pos(vertex+0, pos, dx);
	new_pos(vertex+1, pos, dx);

	printf("<g class='%s eye'>\n", class[pos->order]);
	print_missing_flags(flags, used_flags);
	printf("<path "); eye_path_spec(vertex, r);
	if (hairline) {
		puts("/>");
	} else {
		printf("stroke-width='%d' />", thick);
		printf("<path "); eye_path_spec(vertex, r);
		printf("stroke-width='%d' class='overstrike' />\n",
			thick - EXTRA_THICKNESS);
	}
	puts("</g>");

	/* TODO flag to put eyeball in the eye */

	if (fliprot) {
		struct control rot = *pos;
		rot.bearing += MAX_BEARING/4;
		draw_eye(&rot, (flags | used_flags) & ~FLIPROT);
	}
}

void draw_polygon(struct control const *pos, int sides, int flags)
{
	const bool hairline = flags & HAIRLINE;
	const bool fliprot = flags & FLIPROT;
	const int used_flags = flags & (HAIRLINE | FLIPROT);
	const int dx = delta(pos, hairline);
	const int thick = thickness[pos->order];
	const bool odd = sides & 1;
	flags &= ~used_flags;

	/* TODO exploit symmetries */
	struct control vertex[MAX_NVERT];
	const int vb = MAX_BEARING/sides;
	for (int i = 0; i < sides; ++i) {
		struct control *v = vertex + i;
		v->bearing = pos->bearing + vb*(i - odd*sides/2);
		new_pos(v, pos, dx);
		v->order = pos->order + 1;
		v->scale -= thick;
	}

	printf("<g class='polygon %s'>\n", class[pos->order]);
	print_missing_flags(flags, hairline);
	printf("<path "); poly_path_spec(vertex, sides);
	if (hairline) {
		puts("/>");
	} else {
		printf("stroke-width='%d' />", thick);
		printf("<path "); poly_path_spec(vertex, sides);
		printf("stroke-width='%d' class='overstrike' />\n",
			thick - EXTRA_THICKNESS);
	}
	puts("</g>");

	if (fliprot) {
		struct control rot = *pos;
		rot.bearing += odd ? MAX_BEARING/2 : vb/2;
		draw_polygon(&rot, sides, (flags | used_flags) & ~FLIPROT);
	}
}



void feature(struct control const *pos, uchar* val)
{
	/* A major feature is encoded as a polygon with up to 8 sides
	 * in the lower 3 bits, and a number of flags
	 * on the higher 5 bits */
	/* If the number of sides is 1, then we assume a circle */
	/* If the number of sides is 2, then we assume an eye */
	int sides = (*val & SIDES_MASK) + 1;
	int flags = *val & ~SIDES_MASK;

	switch (sides) {
	case 1:
		draw_circle(pos, flags);
		break;
	case 2:
		draw_eye(pos, flags);
		break;
	default:
		draw_polygon(pos, sides, flags);
	}
}

int main(int argc, char *argv[])
{
	const bool has_arg = (argc > 1);

	uchar pool[SHA256_DIGEST_LENGTH];

	SHA256((uchar*)argv[has_arg], has_arg ? strlen(argv[1]) : 0, pool);

	puts("<svg "
#if 0
		"style='background-color: darkgray' "
#endif
		"xmlns='http://www.w3.org/2000/svg' "
		"xmlns:xlink='http://www.w3.org/1999/xlink' "
		"viewBox='-850 -850 1700 1700'>");
	puts("<style>");
	puts("* { stroke: black; fill: none }");
	puts(".overstrike { stroke: white }");
	puts("</style>");

	struct control pos = {
		.cx = 0, .cy = 0,
		.scale = 840,
		.order = 0,
		.bearing = 0 };

	/* Primary circle: always there, for the time being */
	draw_circle(&pos, 0);

	pos.scale -= thickness[pos.order];
	pos.order += 1;

	/* Primary feature */
	feature(&pos, pool);

	puts("</svg>");
	return 0;
}
