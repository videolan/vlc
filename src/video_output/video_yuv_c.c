/*******************************************************************************
 * video_yuv_c.c: YUV transformation, optimized 
 * (c)1999 VideoLAN
 *******************************************************************************
 * Provides optimized functions to perform the YUV conversion. 
 *******************************************************************************/

#include <stdlib.h>	/* malloc */

//#include "convert.h"

static int binaryLog (int i)
{
    int log;

    log = 0;
    if (i & 0xffff0000) log = 16;
    if (i & 0xff00ff00) log += 8;
    if (i & 0xf0f0f0f0) log += 4;
    if (i & 0xcccccccc) log += 2;
    if (i & 0xaaaaaaaa) log++;
    if (i != (1 << log))
	return -1;

    return log;
}

static int colorMaskToShift (int * right, int * left, int mask)
{
    int low;
    int high;

    low = mask & (- mask);	/* lower bit of the mask */
    high = mask + low;		/* higher bit of the mask */

    low = binaryLog (low);
    high = binaryLog (high);
    if ((low == -1) || (high == -1))
	return 1;

    *left = low;
    *right = (8 - high + low);

    return 0;
}


/*
 * YUV to RGB routines.
 *
 * these routines calculate r, g and b values from each pixel's y, u and v.
 * these r, g an b values are then passed thru a table lookup to take the
 * gamma curve into account and find the corresponding pixel value.
 *
 * the table must store more than 3*256 values because of the possibility
 * of overflow in the yuv->rgb calculation. actually the calculated r,g,b
 * values are in the following intervals :
 * -176 to 255+176 for red
 * -133 to 255+133 for green
 * -222 to 255+222 for blue
 *
 * If the input y,u,v values are right, the r,g,b results are not expected
 * to move out of the 0 to 255 interval but who knows what will happen in
 * real use...
 *
 * the red, green and blue conversion tables are stored in a single 1935-entry
 * array. The respective positions of each component in the array have been
 * calculated to minimize the cache interactions of the 3 tables.
 */

int rgbTable16 (short table [1935],
		       int redMask, int greenMask, int blueMask,
		       unsigned char gamma[256])
{
    int redRight;
    int redLeft;
    int greenRight;
    int greenLeft;
    int blueRight;
    int blueLeft;
    short * redTable;
    short * greenTable;
    short * blueTable;
    int i;
    int y;

    if (colorMaskToShift (&redRight, &redLeft, redMask) ||
	colorMaskToShift (&greenRight, &greenLeft, greenMask) ||
	colorMaskToShift (&blueRight, &blueLeft, blueMask))
	return 1;

    /*
     * green blue red +- 2 just to be sure
     * green = 0-525 [151-370]
     * blue = 594-1297 [834-1053] <834-29>
     * red = 1323-1934 [1517-1736] <493-712>
     */

    redTable = table + 1501;
    greenTable = table + 135;
    blueTable = table + 818;

    for (i = 0; i < 178; i++) {
	redTable[i-178] = 0;
	redTable[i+256] = redMask;
    }
    for (i = 0; i < 135; i++) {
	greenTable[i-135] = 0;
	greenTable[i+256] = greenMask;
    }
    for (i = 0; i < 224; i++) {
	blueTable[i-224] = 0;
	blueTable[i+256] = blueMask;
    }

    for (i = 0; i < 256; i++) {
	y = gamma[i];
	redTable[i] = ((y >> redRight) << redLeft);
	greenTable[i] = ((y >> greenRight) << greenLeft);
	blueTable[i] = ((y >> blueRight) << blueLeft);
    }

    return 0;
}

static int rgbTable32 (int table [1935],
		       int redMask, int greenMask, int blueMask,
		       unsigned char gamma[256])
{
    int redRight;
    int redLeft;
    int greenRight;
    int greenLeft;
    int blueRight;
    int blueLeft;
    int * redTable;
    int * greenTable;
    int * blueTable;
    int i;
    int y;

    if (colorMaskToShift (&redRight, &redLeft, redMask) ||
	colorMaskToShift (&greenRight, &greenLeft, greenMask) ||
	colorMaskToShift (&blueRight, &blueLeft, blueMask))
	return 1;

    /*
     * green blue red +- 2 just to be sure
     * green = 0-525 [151-370]
     * blue = 594-1297 [834-1053] <834-29>
     * red = 1323-1934 [1517-1736] <493-712>
     */

    redTable = table + 1501;
    greenTable = table + 135;
    blueTable = table + 818;

    for (i = 0; i < 178; i++) {
	redTable[i-178] = 0;
	redTable[i+256] = redMask;
    }
    for (i = 0; i < 135; i++) {
	greenTable[i-135] = 0;
	greenTable[i+256] = greenMask;
    }
    for (i = 0; i < 224; i++) {
	blueTable[i-224] = 0;
	blueTable[i+256] = blueMask;
    }

    for (i = 0; i < 256; i++) {
	y = gamma[i];
	redTable[i] = ((y >> redRight) << redLeft);
	greenTable[i] = ((y >> greenRight) << greenLeft);
	blueTable[i] = ((y >> blueRight) << blueLeft);
    }

    return 0;
}

#define SHIFT 20
#define U_GREEN_COEF ((int)(-0.391 * (1<<SHIFT) / 1.164))
#define U_BLUE_COEF ((int)(2.018 * (1<<SHIFT) / 1.164))
#define V_RED_COEF ((int)(1.596 * (1<<SHIFT) / 1.164))
#define V_GREEN_COEF ((int)(-0.813 * (1<<SHIFT) / 1.164))

 void yuvToRgb16 (unsigned char * Y,
			unsigned char * U, unsigned char * V,
			short * dest, short table[1935], int width)
{
    int i;
    int u;
    int v;
    int uvRed;
    int uvGreen;
    int uvBlue;
    short * tableY;

    i = width >> 4;
    while (i--) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
    }

    i = (width & 15) >> 1;
    while (i--) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
    }

    if (width & 1) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
    }
}

static void yuvToRgb24 (unsigned char * Y,
			unsigned char * U, unsigned char * V,
			char * dest, int table[1935], int width)
{
    int i;
    int u;
    int v;
    int uvRed;
    int uvGreen;
    int uvBlue;
    int * tableY;
    int tmp24;

    i = width >> 3;
    while (i--) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;
    }

    i = (width & 7) >> 1;
    while (i--) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;
    }

    if (width & 1) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;
    }
}

static void yuvToRgb32 (unsigned char * Y,
			unsigned char * U, unsigned char * V,
			int * dest, int table[1935], int width)
{
    int i;
    int u;
    int v;
    int uvRed;
    int uvGreen;
    int uvBlue;
    int * tableY;

    i = width >> 4;
    while (i--) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
    }

    i = (width & 15) >> 1;
    while (i--) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
    }

    if (width & 1) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
    }
}

/* API routines */

