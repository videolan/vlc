/*****************************************************************************
 * css.h: Structures for DVD authentification and unscrambling
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: css.h,v 1.8 2002/01/23 03:15:31 stef Exp $
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

typedef struct dvd_title_s
{
    int                 i_startlb;
    dvd_key_t           p_key;
    struct dvd_title_s *p_next;
} dvd_title_t;

typedef struct css_s
{
     int             i_agid;      /* Current Authenication Grant ID. */
     dvd_key_t       p_bus_key;   /* Current session key. */
     dvd_key_t       p_disc_key;  /* This DVD disc's key. */
     dvd_key_t       p_unenc_key; /* Current title key before decryption. */    
     dvd_key_t       p_title_key; /* Current title key. */    
} css_t;

/*****************************************************************************
 * Prototypes in css.c
 *****************************************************************************/
struct css_s;

int   CSSTest             ( dvdcss_handle );
int   CSSGetDiscKey       ( dvdcss_handle );
int   CSSGetTitleKey      ( dvdcss_handle, int );
int   CSSDescrambleSector ( u8 * , u8 * );

