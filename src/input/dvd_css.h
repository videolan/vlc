/*****************************************************************************
 * dvd_css.h: Structures for DVD authentification and unscrambling
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
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

typedef unsigned char DVD40bitKey[KEY_SIZE];

typedef struct drive_key_s
{
    u8              pi_challenge[2*KEY_SIZE];
    u8              pi_key1[KEY_SIZE];
    u8              pi_key2[KEY_SIZE];
    u8              pi_key_check[KEY_SIZE];
    u8              i_version;
} drive_key_t;

typedef struct title_key_s
{
    u32             i_lba;
    u8              key[KEY_SIZE];
} title_key_t;

typedef struct css_s
{
    int             i_fd;
    boolean_t       b_error;
    int             i_agid;
    drive_key_t     keys;
    u8              pi_disc_key[2048];
    int             i_title_nb;
    title_key_t*    p_title_key;
} css_t;

/*****************************************************************************
 * Prototypes in dvd_css.c
 *****************************************************************************/
struct css_s    CSSInit     ( int );
int             CSSGetKeys  ( struct css_s* );
