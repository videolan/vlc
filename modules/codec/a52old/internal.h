/*****************************************************************************
 * internals.h: needed by the A52 decoder
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: internal.h,v 1.1 2002/08/04 17:23:42 sam Exp $
 *
 * Authors: Michel Lespinasse <walken@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/* Exponent strategy constants */
#define EXP_REUSE       (0)
#define EXP_D15         (1)
#define EXP_D25         (2)
#define EXP_D45         (3)

/* Delta bit allocation constants */
#define DELTA_BIT_REUSE         (0)
#define DELTA_BIT_NEW           (1)
#define DELTA_BIT_NONE          (2)
#define DELTA_BIT_RESERVED      (3)

/* bit_allocate.c */
void bit_allocate (a52dec_t *);

/* exponent.c */
int exponent_unpack (a52dec_t *);

/* imdct.c */
void E_( imdct_init )(imdct_t * p_imdct);
void imdct (a52dec_t * p_a52dec, s16 * buffer);

/* mantissa.c */
void mantissa_unpack (a52dec_t *);

/* parse.c */
int parse_bsi (a52dec_t *);
int parse_audblk (a52dec_t *, int);
void parse_auxdata (a52dec_t *);

/* rematrix.c */
void rematrix (a52dec_t *);

