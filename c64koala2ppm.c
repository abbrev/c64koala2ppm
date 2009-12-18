/*

$Id$

c64koala2ppm, convert a Commodore 64 KoalaPaint image to Portable Pixmap
Copyright 2009 Christopher Williams

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <getopt.h>

/*
 * The Commodore 64 version of Koala Painter used a fairly simple file format:
 * A two-byte load address, followed immediately by
 * 8000 bytes of raw bitmap data,
 * 1000 bytes of raw "Video Matrix" data,
 * 1000 bytes of raw "Color RAM" data,
 * and a one-byte Background Color field.
 *
 * The screen is divided into 4x8 pixel areas called "cards". Cards are arranged
 * left-to-right, top-to-bottom, in row-major order.
 *
 * The video matrix data is arranged in card order, with one byte per card.
 * Each byte defines two colors that can be used in that card, one in the high
 * nibble, and one in the low nibble.
 *
 * The color RAM is similar to the video matrix data, except that each byte
 * contains only one color that can be used in that card. Only the low nibble
 * is used.
 *
 * The bitmap data is arranged in card order, such that it fills up one card at
 * a time. Each byte of bitmap data consists of four two-bit pixels, with each
 * pixel being one of 4 colors:
 *    00: global background color
 *    01: upper nibble of video RAM for this card
 *    10: lower nibble of video RAM for this card
 *    11: lower nibble of color RAM for this card
 */

/* http://www.pepto.de/projects/colorvic/ defines both USCALE and VSCALE
 * as 34.0081334493 / 255 (0.13337)
 * it also uses the following formula to convert YUV to RGB:
 * R = Y             + 1,140 * V
 * G = Y - 0,396 * U - 0,581 * V
 * B = Y + 2,029 * U
 *
 * while I'm using this formula from wikipedia:
 * R = Y               + 1.13983 * V
 * G = Y - 0.39465 * U - 0.58060 * V
 * B = Y + 2.03211 * U
 */
#define USCALE 0.1331
#define VSCALE 0.1331
#define SATURATION 1.0

float saturation;

/* colors are from http://www.pepto.de/projects/colorvic/ */
struct c64_color {
	 /* angle is [0..15], luma is [0..32], saturation is [0..1] */
	int angle, luma, saturation;
} c64_colors[16] = {
	{  0,  0, 0 }, /* black */
	{  0, 32, 0 }, /* white */

	{  5, 10, 1 }, /* red */
	{ 13, 20, 1 }, /* cyan */

	{  2, 12, 1 }, /* purple */
	{ 10, 16, 1 }, /* green */

	{  0,  8, 1 }, /* blue */
	{  8, 24, 1 }, /* yellow */

	{  6, 12, 1 }, /* orange */
	{  7,  8, 1 }, /* brown */
	{  5, 16, 1 }, /* light red */
	{  0, 10, 0 }, /* dark grey */
	{  0, 15, 0 }, /* grey */
	{ 10, 24, 1 }, /* light green */
	{  0, 15, 1 }, /* light blue */
	{  0, 20, 0 }, /* light grey */
};

/* note: the y component in yuv_color is luma (Y'), which is gamma-compressed */
struct yuv_color {
	float y, u, v;
};

struct rgb_color {
	int r, g, b;
};

void c64_to_yuv(struct c64_color *c64, struct yuv_color *yuv, float uscale, float vscale)
{
	yuv->y = c64->luma / 32.;
	yuv->u = c64->saturation * saturation * uscale * cos(c64->angle*M_PI/8);
	yuv->v = c64->saturation * saturation * vscale * sin(c64->angle*M_PI/8);
}

/* from http://en.wikipedia.org/wiki/YUV */
void yuv_to_rgb(struct yuv_color *yuv, struct rgb_color *rgb)
{
	float r,g,b;
	r = (yuv->y +                      1.13983 * yuv->v);
	g = (yuv->y + -0.39465 * yuv->u + -0.58060 * yuv->v);
	b = (yuv->y +  2.03211 * yuv->u);
	
#define BOUND(x) do {                      \
if (x < 0) {                               \
	x = 0;                             \
	/*fprintf(stderr, #x " < 0!\n");*/ \
} else if (x > 1) {                        \
	x = 1;                             \
	/*fprintf(stderr, #x " > 1!\n");*/ \
}} while (0)

	BOUND(r);
	BOUND(g);
	BOUND(b);
	rgb->r = 255*r + 0.5;
	rgb->g = 255*g + 0.5;
	rgb->b = 255*b + 0.5;
}

void c64_to_rgb(struct c64_color *c64, struct rgb_color *rgb, float uscale, float vscale)
{
	struct yuv_color yuv;
	c64_to_yuv(c64, &yuv, uscale, vscale);
	yuv_to_rgb(&yuv, rgb);
}

struct rgb_color c64colors[16];

void init_colors()
{
	int i;
	for (i = 0; i < 16; ++i)
		c64_to_rgb(&c64_colors[i], &c64colors[i], USCALE, VSCALE);
}

char *argv0;

void usage(void)
{
	fprintf(stderr,
	        "Usage: %s [-hL] [-s saturation] [koala_file]\n"
		"  -h             Show this help message and exit\n"
	        "  -L             Show license information and exit\n"
	        "  -s saturation  Set the output saturation. Value must be >= 0\n",
	        argv0);
}

void license(void)
{
	fprintf(stderr,
	        "c64koala2ppm, convert a Commodore 64 KoalaPaint image to Portable Pixmap\n"
	        "Copyright 2009 Christopher Williams\n"
	        "\n"
	        "This program is free software; you can redistribute it and/or\n"
	        "modify it under the terms of the GNU General Public License\n"
	        "as published by the Free Software Foundation; either version 2\n"
	        "of the License, or (at your option) any later version.\n"
	        "\n"
	        "This program is distributed in the hope that it will be useful,\n"
	        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
	        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
	        "GNU General Public License for more details.\n");
}

char *koalafilename = NULL;

int getargs(int argc, char *argv[])
{
	int opt;
	saturation = SATURATION;
	while ((opt = getopt(argc, argv, "hLs:")) != -1) {
		switch (opt) {
		case 'h':
			usage();
			exit(0);
		case 'L':
			license();
			exit(0);
		case 's':
			saturation = atof(optarg);
			if (saturation < 0) {
				fprintf(stderr, "%s: saturation must be >= 0\n",
				 argv0);
				exit(1);
			}
			break;
		default: /* '?' */
			usage();
			exit(1);
		}
	}
	if (optind < argc - 1) {
		fprintf(stderr, "%s: too many filenames\n", argv0);
		usage();
		exit(1);
	} else if (optind == argc - 1 && strcmp("-", argv[optind])) {
		koalafilename = argv[optind];
	}
	return 0;
}

int main(int argc, char *argv[])
{
	FILE *koalafile;
	unsigned char loadaddr[2];
	char bitmap[25][40][8];
	char video[25][40];
	char color[25][40];
	char bg;
	char outbitmap[200][40]; /* bitmap, re-arranged by scan order */
	struct rgb_color colors[4];
	struct rgb_color outimage[200][160];
	struct rgb_color outcolor;
	int x, y;
	int cardx, cardy;
	
	argv0 = argv[0];
	getargs(argc, argv);
	if (koalafilename) {
		koalafile = fopen(koalafilename, "rb");
		if (!koalafile) {
			fprintf(stderr, "%s: could not open \"%s\" for reading\n", argv0, koalafilename);
			return -1;
		}
	} else {
		koalafile = stdin;
	}
	
	init_colors();
	
	/* set the colors and bitmap to indicate short files */
	memset(bitmap, 0x1b, sizeof(bitmap));
	memset(video, 0x25, sizeof(video));
	memset(color, 0x06, sizeof(color));
	bg = 0x00;
	
	if (!(fread(loadaddr, 2, 1, koalafile) &&
	      fread(bitmap, 8000, 1, koalafile) &&
	      fread(video, 1000, 1, koalafile) &&
	      fread(color, 1000, 1, koalafile) &&
	      fread(&bg, 1, 1, koalafile))) {
		fprintf(stderr, "%s: koala file is too short. Output may be corrupt.\n", argv0);
	}
	
#if 0
	fprintf(stderr, "load address: 0x%02x%02x\n", loadaddr[1], loadaddr[0]);
	fprintf(stderr, "background color: 0x%02x\n", (unsigned char)bg);
#endif
	colors[0] = c64colors[(unsigned char)bg & 0x0f];
	
	for (cardy = 0; cardy < 25; ++cardy) {
		for (cardx = 0; cardx < 40; ++cardx) {
			/* 1 = upper nibble of video ram */
			colors[1] = c64colors[(video[cardy][cardx]>>4) & 0x0f];
			/* 2 = lower nibble of video ram */
			colors[2] = c64colors[(video[cardy][cardx]) & 0x0f];
			/* 3 = color ram */
			colors[3] = c64colors[(color[cardy][cardx]) & 0x0f];
			
			for (y = 0; y < 8; ++y) {
				int outx = cardx;
				int outy = 8*cardy+y;
				
				int b, c;
				c = bitmap[cardy][cardx][y];
				outbitmap[outy][outx] = c;
				for (b = 0; b < 8; b += 2) {
					outcolor = colors[(c>>b) & 03];
					outimage[outy][4*outx+(6-b)/2] = outcolor;
				}
			}
		}
	}
	
	printf("P6\n"
	       "%d %d\n"
	       "%d\n", 160, 200, 255);
	for (y = 0; y < 200; ++y) {
		for (x = 0; x < 160; ++x) {
			printf("%c%c%c",
			        outimage[y][x].r,
			        outimage[y][x].g,
			        outimage[y][x].b);
		}
	}
	return 0;
}
