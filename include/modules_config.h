/*****************************************************************************
 * modules_config.h : Module configuration tools.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * Module capabilities.
 *****************************************************************************/
#define MODULE_CAPABILITY_NULL     0       /* The Module can't do anything */
#define MODULE_CAPABILITY_INTF     1<<0    /* Interface */
#define MODULE_CAPABILITY_INPUT    1<<1    /* Input */
#define MODULE_CAPABILITY_DECAPS   1<<2    /* Decaps */
#define MODULE_CAPABILITY_ADEC     1<<3    /* Audio decoder */
#define MODULE_CAPABILITY_VDEC     1<<4    /* Video decoder */
#define MODULE_CAPABILITY_AOUT     1<<5    /* Audio output */
#define MODULE_CAPABILITY_VOUT     1<<6    /* Video output */
#define MODULE_CAPABILITY_YUV      1<<7    /* YUV colorspace conversion */
#define MODULE_CAPABILITY_AFX      1<<8    /* Audio effects */
#define MODULE_CAPABILITY_VFX      1<<9    /* Video effects */

/*****************************************************************************
 * Macros used to build the configuration structure.
 *****************************************************************************/

/* Mandatory first and last parts of the structure */
#define MODULE_CONFIG_ITEM_START       0x01    /* The main window */
#define MODULE_CONFIG_ITEM_END         0x00    /* End of the window */

/* Configuration widgets */
#define MODULE_CONFIG_ITEM_PANE        0x02    /* A notebook pane */
#define MODULE_CONFIG_ITEM_FRAME       0x03    /* A frame */
#define MODULE_CONFIG_ITEM_COMMENT     0x04    /* A comment text */
#define MODULE_CONFIG_ITEM_STRING      0x05    /* A string */
#define MODULE_CONFIG_ITEM_FILE        0x06    /* A file selector */
#define MODULE_CONFIG_ITEM_CHECK       0x07    /* A checkbox */
#define MODULE_CONFIG_ITEM_CHOOSE      0x08    /* A choose box */
#define MODULE_CONFIG_ITEM_RADIO       0x09    /* A radio box */
#define MODULE_CONFIG_ITEM_SCALE       0x0a    /* A horizontal ruler */
#define MODULE_CONFIG_ITEM_SPIN        0x0b    /* A numerical selector */

