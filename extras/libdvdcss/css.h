/*****************************************************************************
 * css.h: Structures for DVD authentification and unscrambling
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: css.h,v 1.6 2001/10/13 15:34:21 stef Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
 *
 * based on:
 *  - css-auth by Derek Fawcus <derek@spider.com>
 *  - DVD CSS ioctls example program by Andrew T. Veliath <andrewtv@usa.net>
 *  - DeCSSPlus by Ethan Hawke
 *  - The Divide and conquer attack by Frank A. Stevenson <frank@funcom.com>
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
#define KEY_SIZE 5

typedef u8 dvd_key_t[KEY_SIZE];

typedef struct disc_s
{
    u8              p_challenge[2*KEY_SIZE];
    dvd_key_t       p_key1;
    dvd_key_t       p_key2;
    dvd_key_t       p_key_check;
    u8              i_varient;
    u8              p_disc_key[2048];
} disc_t;

typedef struct dvd_title_s
{
    int                 i_startlb;
    dvd_key_t           p_key;
    struct dvd_title_s *p_next;
} dvd_title_t;

typedef struct css_s
{
    int             i_agid;
    disc_t          disc;
    dvd_key_t       p_title_key;
} css_t;

/*****************************************************************************
 * Prototypes in css.c
 *****************************************************************************/
struct css_s;

int   CSSTest             ( dvdcss_handle );
int   CSSAuth             ( dvdcss_handle );
int   CSSGetDiscKey       ( dvdcss_handle );
int   CSSGetTitleKey      ( dvdcss_handle, int );
int   CSSDescrambleSector ( u8 * , u8 * );

