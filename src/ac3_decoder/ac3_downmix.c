#include "int_types.h"
#include "ac3_decoder.h"
#include "ac3_internal.h"

#define NORM 16384

typedef struct prefs_s {
    u16 use_dolby_surround;
    u16 dual_mono_channel_select;
} prefs_t;

prefs_t global_prefs = {0,0};

/* Pre-scaled downmix coefficients */
static float cmixlev_lut[4] = { 0.2928, 0.2468, 0.2071, 0.2468 };
static float smixlev_lut[4] = { 0.2928, 0.2071, 0.0   , 0.2071 };

/* Downmix into _two_ channels...other downmix modes aren't implemented
 * to reduce complexity. Realistically, there aren't many machines around
 * with > 2 channel output anyways */

void downmix (ac3dec_t * p_ac3dec, s16 * out_buf)
{
    int j;
    float right_tmp;
    float left_tmp;
    float clev,slev;
    float *centre = 0, *left = 0, *right = 0, *left_sur = 0, *right_sur = 0;

    /*
    if (p_ac3dec->bsi.acmod > 7)
        dprintf("(downmix) invalid acmod number\n");
    */

    /* There are two main cases, with or without Dolby Surround */
    if (global_prefs.use_dolby_surround) {
        switch(p_ac3dec->bsi.acmod) {
	case 7:	/* 3/2 */
	    left      = p_ac3dec->samples.channel[0];
	    centre    = p_ac3dec->samples.channel[1];
	    right     = p_ac3dec->samples.channel[2];
	    left_sur  = p_ac3dec->samples.channel[3];
	    right_sur = p_ac3dec->samples.channel[4];

	    for (j = 0; j < 256; j++) {
		right_tmp = 0.2265f * *left_sur++ + 0.2265f * *right_sur++;
		left_tmp  = -1 * right_tmp;
		right_tmp += 0.3204f * *right++ + 0.2265f * *centre;
		left_tmp  += 0.3204f * *left++  + 0.2265f * *centre++;

		*(out_buf++) = left_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;

	case 6:	/* 2/2 */
	    left      = p_ac3dec->samples.channel[0];
	    right     = p_ac3dec->samples.channel[1];
	    left_sur  = p_ac3dec->samples.channel[2];
	    right_sur = p_ac3dec->samples.channel[3];

	    for (j = 0; j < 256; j++) {
		right_tmp = 0.2265f * *left_sur++ + 0.2265f * *right_sur++;
		left_tmp  = -1 * right_tmp;
		right_tmp += 0.3204f * *right++;
		left_tmp  += 0.3204f * *left++ ;

		*(out_buf++) = left_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;

	case 5:	/* 3/1 */
	    left      = p_ac3dec->samples.channel[0];
	    centre    = p_ac3dec->samples.channel[1];
	    right     = p_ac3dec->samples.channel[2];
	    /* Mono surround */
	    right_sur = p_ac3dec->samples.channel[3];

	    for (j = 0; j < 256; j++) {
		right_tmp =  0.2265f * *right_sur++;
		left_tmp  = - right_tmp;
		right_tmp += 0.3204f * *right++ + 0.2265f * *centre;
		left_tmp  += 0.3204f * *left++  + 0.2265f * *centre++;

		*(out_buf++) = left_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;

	case 4:	/* 2/1 */
	    left      = p_ac3dec->samples.channel[0];
	    right     = p_ac3dec->samples.channel[1];
	    /* Mono surround */
	    right_sur = p_ac3dec->samples.channel[2];

	    for (j = 0; j < 256; j++) {
		right_tmp =  0.2265f * *right_sur++;
		left_tmp  = - right_tmp;
		right_tmp += 0.3204f * *right++;
		left_tmp  += 0.3204f * *left++;

		*(out_buf++) = left_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;

	case 3:	/* 3/0 */
	    left      = p_ac3dec->samples.channel[0];
	    centre    = p_ac3dec->samples.channel[1];
	    right     = p_ac3dec->samples.channel[2];

	    for (j = 0; j < 256; j++) {
		right_tmp = 0.3204f * *right++ + 0.2265f * *centre;
		left_tmp  = 0.3204f * *left++  + 0.2265f * *centre++;

		*(out_buf++) = left_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;

	case 2:	/* 2/0 */
	    left = p_ac3dec->samples.channel[0];
	    right = p_ac3dec->samples.channel[1];

	    for (j = 0; j < 256; j++) {
		*(out_buf++) = *(left++) * NORM;
		*(out_buf++) = *(right++) * NORM;
	    }
	    break;

	case 1:	/* 1/0 */
	    /* Mono program! */
	    right = p_ac3dec->samples.channel[0];

	    for (j = 0; j < 256; j++) {
		right_tmp = 0.7071f * *right++;

		*(out_buf++) = right_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;

	case 0:	/* 1+1 */
	    /* Dual mono, output selected by user */
	    right = p_ac3dec->samples.channel[global_prefs.dual_mono_channel_select];

	    for (j = 0; j < 256; j++) {
		right_tmp = 0.7071f * *right++;

		*(out_buf++) = right_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;
        }
    } else {
        /* Non-Dolby surround downmixes */
        switch(p_ac3dec->bsi.acmod) {
	case 7:	/* 3/2 */
	    left      = p_ac3dec->samples.channel[0];
	    centre    = p_ac3dec->samples.channel[1];
	    right     = p_ac3dec->samples.channel[2];
	    left_sur  = p_ac3dec->samples.channel[3];
	    right_sur = p_ac3dec->samples.channel[4];

	    clev = cmixlev_lut[p_ac3dec->bsi.cmixlev];
	    slev = smixlev_lut[p_ac3dec->bsi.surmixlev];

	    for (j = 0; j < 256; j++) {
		right_tmp= 0.4142f * *right++ + clev * *centre   + slev * *right_sur++;
		left_tmp = 0.4142f * *left++  + clev * *centre++ + slev * *left_sur++;

		*(out_buf++) = left_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;

	case 6:	/* 2/2 */
	    left      = p_ac3dec->samples.channel[0];
	    right     = p_ac3dec->samples.channel[1];
	    left_sur  = p_ac3dec->samples.channel[2];
	    right_sur = p_ac3dec->samples.channel[3];

	    slev = smixlev_lut[p_ac3dec->bsi.surmixlev];

	    for (j = 0; j < 256; j++) {
		right_tmp= 0.4142f * *right++ + slev * *right_sur++;
		left_tmp = 0.4142f * *left++  + slev * *left_sur++;

		*(out_buf++) = left_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;

	case 5:	/* 3/1 */
	    left      = p_ac3dec->samples.channel[0];
	    centre    = p_ac3dec->samples.channel[1];
	    right     = p_ac3dec->samples.channel[2];
	    /* Mono surround */
	    right_sur = p_ac3dec->samples.channel[3];

	    clev = cmixlev_lut[p_ac3dec->bsi.cmixlev];
	    slev = smixlev_lut[p_ac3dec->bsi.surmixlev];
	    
	    for (j = 0; j < 256; j++) {
		right_tmp= 0.4142f * *right++ + clev * *centre   + slev * *right_sur;
		left_tmp = 0.4142f * *left++  + clev * *centre++ + slev * *right_sur++;

		*(out_buf++) = left_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;

	case 4:	/* 2/1 */
	    left      = p_ac3dec->samples.channel[0];
	    right     = p_ac3dec->samples.channel[1];
	    /* Mono surround */
	    right_sur = p_ac3dec->samples.channel[2];

	    slev = smixlev_lut[p_ac3dec->bsi.surmixlev];

	    for (j = 0; j < 256; j++) {
		right_tmp= 0.4142f * *right++ + slev * *right_sur;
		left_tmp = 0.4142f * *left++  + slev * *right_sur++;

		*(out_buf++) = left_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;

	case 3:	/* 3/0 */
	    left      = p_ac3dec->samples.channel[0];
	    centre    = p_ac3dec->samples.channel[1];
	    right     = p_ac3dec->samples.channel[2];

	    clev = cmixlev_lut[p_ac3dec->bsi.cmixlev];

	    for (j = 0; j < 256; j++) {
		right_tmp= 0.4142f * *right++ + clev * *centre;
		left_tmp = 0.4142f * *left++  + clev * *centre++;

		*(out_buf++) = left_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;

	case 2:	/* 2/0 */
	    left = p_ac3dec->samples.channel[0];
	    right = p_ac3dec->samples.channel[1];

	    for (j = 0; j < 256; j++) {
		*(out_buf++) = *(left++) * NORM;
		*(out_buf++) = *(right++) * NORM;
	    }
	    break;

	case 1:	/* 1/0 */
	    /* Mono program! */
	    right = p_ac3dec->samples.channel[0];

	    for (j = 0; j < 256; j++) {
		right_tmp = 0.7071f * *right++;

		*(out_buf++) = right_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;

	case 0:	/* 1+1 */
	    /* Dual mono, output selected by user */
	    right = p_ac3dec->samples.channel[global_prefs.dual_mono_channel_select];

	    for (j = 0; j < 256; j++) {
		right_tmp = 0.7071f * *right++;

		*(out_buf++) = right_tmp * NORM;
		*(out_buf++) = right_tmp * NORM;
	    }
	    break;
        }
    }
}
