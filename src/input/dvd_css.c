/*****************************************************************************
 * dvd_css.c: Functions for DVD authentification and unscrambling
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <malloc.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
//#if defined(__NetBSD__) || defined(__OpenBSD__)
//# include <sys/dvdio.h>
//#elif defined(__linux__)
#include <linux/cdrom.h>
//#else
//# error "Need the DVD ioctls"
//#endif


#include "common.h"

#include "intf_msg.h"
#include "dvd_css.h"
#include "css_table.h"

/*****************************************************************************
 * CSS tables
 *****************************************************************************/

unsigned int pi_css_tab0[11]={5,0,1,2,3,4,0,1,2,3,4};

unsigned char pi_css_tab1[256]=
{   0x33, 0x73, 0x3b, 0x26, 0x63, 0x23, 0x6b, 0x76,
    0x3e, 0x7e, 0x36, 0x2b, 0x6e, 0x2e, 0x66, 0x7b,
    0xd3, 0x93, 0xdb, 0x06, 0x43, 0x03, 0x4b, 0x96,
    0xde, 0x9e, 0xd6, 0x0b, 0x4e, 0x0e, 0x46, 0x9b,
    0x57, 0x17, 0x5f, 0x82, 0xc7, 0x87, 0xcf, 0x12,
    0x5a, 0x1a, 0x52, 0x8f, 0xca, 0x8a, 0xc2, 0x1f,
    0xd9, 0x99, 0xd1, 0x00, 0x49, 0x09, 0x41, 0x90,
    0xd8, 0x98, 0xd0, 0x01, 0x48, 0x08, 0x40, 0x91,
    0x3d, 0x7d, 0x35, 0x24, 0x6d, 0x2d, 0x65, 0x74,
    0x3c, 0x7c, 0x34, 0x25, 0x6c, 0x2c, 0x64, 0x75,
	0xdd, 0x9d, 0xd5, 0x04, 0x4d, 0x0d, 0x45, 0x94,
    0xdc, 0x9c, 0xd4, 0x05, 0x4c, 0x0c, 0x44, 0x95,
    0x59, 0x19, 0x51, 0x80, 0xc9, 0x89, 0xc1, 0x10,
    0x58, 0x18, 0x50, 0x81, 0xc8, 0x88, 0xc0, 0x11,
    0xd7, 0x97, 0xdf, 0x02, 0x47, 0x07, 0x4f, 0x92,
    0xda, 0x9a, 0xd2, 0x0f, 0x4a, 0x0a, 0x42, 0x9f,
    0x53, 0x13, 0x5b, 0x86, 0xc3, 0x83, 0xcb, 0x16,
    0x5e, 0x1e, 0x56, 0x8b, 0xce, 0x8e, 0xc6, 0x1b,
    0xb3, 0xf3, 0xbb, 0xa6, 0xe3, 0xa3, 0xeb, 0xf6,
    0xbe, 0xfe, 0xb6, 0xab, 0xee, 0xae, 0xe6, 0xfb,
    0x37, 0x77, 0x3f, 0x22, 0x67, 0x27, 0x6f, 0x72,
    0x3a, 0x7a, 0x32, 0x2f, 0x6a, 0x2a, 0x62, 0x7f,
    0xb9, 0xf9, 0xb1, 0xa0, 0xe9, 0xa9, 0xe1, 0xf0,
    0xb8, 0xf8, 0xb0, 0xa1, 0xe8, 0xa8, 0xe0, 0xf1,
    0x5d, 0x1d, 0x55, 0x84, 0xcd, 0x8d, 0xc5, 0x14,
    0x5c, 0x1c, 0x54, 0x85, 0xcc, 0x8c, 0xc4, 0x15,
    0xbd, 0xfd, 0xb5, 0xa4, 0xed, 0xad, 0xe5, 0xf4,
    0xbc, 0xfc, 0xb4, 0xa5, 0xec, 0xac, 0xe4, 0xf5,
    0x39, 0x79, 0x31, 0x20, 0x69, 0x29, 0x61, 0x70,
    0x38, 0x78, 0x30, 0x21, 0x68, 0x28, 0x60, 0x71,
    0xb7, 0xf7, 0xbf, 0xa2, 0xe7, 0xa7, 0xef, 0xf2,
    0xba, 0xfa, 0xb2, 0xaf, 0xea, 0xaa, 0xe2, 0xff };

unsigned char pi_css_tab2[256]=
{   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x09, 0x08, 0x0b, 0x0a, 0x0d, 0x0c, 0x0f, 0x0e,
    0x12, 0x13, 0x10, 0x11, 0x16, 0x17, 0x14, 0x15,
    0x1b, 0x1a, 0x19, 0x18, 0x1f, 0x1e, 0x1d, 0x1c,
    0x24, 0x25, 0x26, 0x27, 0x20, 0x21, 0x22, 0x23,
    0x2d, 0x2c, 0x2f, 0x2e, 0x29, 0x28, 0x2b, 0x2a,
    0x36, 0x37, 0x34, 0x35, 0x32, 0x33, 0x30, 0x31,
    0x3f, 0x3e, 0x3d, 0x3c, 0x3b, 0x3a, 0x39, 0x38,
    0x49, 0x48, 0x4b, 0x4a, 0x4d, 0x4c, 0x4f, 0x4e,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x5b, 0x5a, 0x59, 0x58, 0x5f, 0x5e, 0x5d, 0x5c,
    0x52, 0x53, 0x50, 0x51, 0x56, 0x57, 0x54, 0x55,
    0x6d, 0x6c, 0x6f, 0x6e, 0x69, 0x68, 0x6b, 0x6a,
    0x64, 0x65, 0x66, 0x67, 0x60, 0x61, 0x62, 0x63,
    0x7f, 0x7e, 0x7d, 0x7c, 0x7b, 0x7a, 0x79, 0x78,
    0x76, 0x77, 0x74, 0x75, 0x72, 0x73, 0x70, 0x71,
    0x92, 0x93, 0x90, 0x91, 0x96, 0x97, 0x94, 0x95,
    0x9b, 0x9a, 0x99, 0x98, 0x9f, 0x9e, 0x9d, 0x9c,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x89, 0x88, 0x8b, 0x8a, 0x8d, 0x8c, 0x8f, 0x8e,
    0xb6, 0xb7, 0xb4, 0xb5, 0xb2, 0xb3, 0xb0, 0xb1,
    0xbf, 0xbe, 0xbd, 0xbc, 0xbb, 0xba, 0xb9, 0xb8,
    0xa4, 0xa5, 0xa6, 0xa7, 0xa0, 0xa1, 0xa2, 0xa3,
    0xad, 0xac, 0xaf, 0xae, 0xa9, 0xa8, 0xab, 0xaa,
    0xdb, 0xda, 0xd9, 0xd8, 0xdf, 0xde, 0xdd, 0xdc,
    0xd2, 0xd3, 0xd0, 0xd1, 0xd6, 0xd7, 0xd4, 0xd5,
    0xc9, 0xc8, 0xcb, 0xca, 0xcd, 0xcc, 0xcf, 0xce,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8,
    0xf6, 0xf7, 0xf4, 0xf5, 0xf2, 0xf3, 0xf0, 0xf1,
    0xed, 0xec, 0xef, 0xee, 0xe9, 0xe8, 0xeb, 0xea,
    0xe4, 0xe5, 0xe6, 0xe7, 0xe0, 0xe1, 0xe2, 0xe3
};

unsigned char pi_css_tab3[512]=
{
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 
	0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff, 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff
};

unsigned char pi_css_tab4[256]=
{
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0, 
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4, 
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc, 
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2, 
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa, 
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6, 
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1, 
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9, 
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd, 
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3, 
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, 
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7, 
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

unsigned char pi_css_tab5[256]=
{
	0xff, 0x7f, 0xbf, 0x3f, 0xdf, 0x5f, 0x9f, 0x1f, 0xef, 0x6f, 0xaf, 0x2f, 0xcf, 0x4f, 0x8f, 0x0f, 
	0xf7, 0x77, 0xb7, 0x37, 0xd7, 0x57, 0x97, 0x17, 0xe7, 0x67, 0xa7, 0x27, 0xc7, 0x47, 0x87, 0x07, 
	0xfb, 0x7b, 0xbb, 0x3b, 0xdb, 0x5b, 0x9b, 0x1b, 0xeb, 0x6b, 0xab, 0x2b, 0xcb, 0x4b, 0x8b, 0x0b, 
	0xf3, 0x73, 0xb3, 0x33, 0xd3, 0x53, 0x93, 0x13, 0xe3, 0x63, 0xa3, 0x23, 0xc3, 0x43, 0x83, 0x03, 
	0xfd, 0x7d, 0xbd, 0x3d, 0xdd, 0x5d, 0x9d, 0x1d, 0xed, 0x6d, 0xad, 0x2d, 0xcd, 0x4d, 0x8d, 0x0d, 
	0xf5, 0x75, 0xb5, 0x35, 0xd5, 0x55, 0x95, 0x15, 0xe5, 0x65, 0xa5, 0x25, 0xc5, 0x45, 0x85, 0x05, 
	0xf9, 0x79, 0xb9, 0x39, 0xd9, 0x59, 0x99, 0x19, 0xe9, 0x69, 0xa9, 0x29, 0xc9, 0x49, 0x89, 0x09, 
	0xf1, 0x71, 0xb1, 0x31, 0xd1, 0x51, 0x91, 0x11, 0xe1, 0x61, 0xa1, 0x21, 0xc1, 0x41, 0x81, 0x01, 
	0xfe, 0x7e, 0xbe, 0x3e, 0xde, 0x5e, 0x9e, 0x1e, 0xee, 0x6e, 0xae, 0x2e, 0xce, 0x4e, 0x8e, 0x0e, 
	0xf6, 0x76, 0xb6, 0x36, 0xd6, 0x56, 0x96, 0x16, 0xe6, 0x66, 0xa6, 0x26, 0xc6, 0x46, 0x86, 0x06, 
	0xfa, 0x7a, 0xba, 0x3a, 0xda, 0x5a, 0x9a, 0x1a, 0xea, 0x6a, 0xaa, 0x2a, 0xca, 0x4a, 0x8a, 0x0a, 
	0xf2, 0x72, 0xb2, 0x32, 0xd2, 0x52, 0x92, 0x12, 0xe2, 0x62, 0xa2, 0x22, 0xc2, 0x42, 0x82, 0x02, 
	0xfc, 0x7c, 0xbc, 0x3c, 0xdc, 0x5c, 0x9c, 0x1c, 0xec, 0x6c, 0xac, 0x2c, 0xcc, 0x4c, 0x8c, 0x0c, 
	0xf4, 0x74, 0xb4, 0x34, 0xd4, 0x54, 0x94, 0x14, 0xe4, 0x64, 0xa4, 0x24, 0xc4, 0x44, 0x84, 0x04, 
	0xf8, 0x78, 0xb8, 0x38, 0xd8, 0x58, 0x98, 0x18, 0xe8, 0x68, 0xa8, 0x28, 0xc8, 0x48, 0x88, 0x08, 
	0xf0, 0x70, 0xb0, 0x30, 0xd0, 0x50, 0x90, 0x10, 0xe0, 0x60, 0xa0, 0x20, 0xc0, 0x40, 0x80, 0x00
};


/*
 * Local functions
 */

/*****************************************************************************
 * CSSGetASF : Get Authentification success flag
 *****************************************************************************/
int CSSGetASF( int i_fd )
{
#if 0
	dvd_authinfo    auth;

	auth.type = DVD_LU_SEND_ASF;
	auth.lsasf.asf = 0;

	for( auth.lsasf.agid = 0 ; auth.lsasf.agid < 4 ; auth.lsasf.agid++ )
    {
		if( ioctl( i_fd, DVD_AUTH, &auth ) )
        {
            intf_ErrMsg( "DVD ioctl error in CSSGetASF" );
			return -1;
		}
	}

// ("%sAuthenticated\n", (ai.lsasf.asf) ? "" : "not");
	return 0;
#else

	dvd_authinfo ai;

	ai.type = DVD_LU_SEND_ASF;
	ai.lsasf.asf = 0;

	for (ai.lsasf.agid = 0; ai.lsasf.agid < 4; ai.lsasf.agid++) {
		if (!(ioctl(i_fd, DVD_AUTH, &ai))) {
			fprintf( stderr, "%sAuthenticated\n", (ai.lsasf.asf) ? "" : "not");
			return 0;
		}
	}
	intf_ErrMsg( "CSS Error : GetASF" );
	return -1;

#endif
}

/*****************************************************************************
 * CSSCryptKey : shuffles bits and unencrypt keys.
 * ---
 * i_key_type : 0->key1, 1->key2, 2->buskey.
 * i_varient : between 0 and 31.
 *****************************************************************************/
#if 0
static void CSSCryptKey( int i_key_type, int i_varient,
                         u8* pi_challenge, u8* pi_key )
{
    /* Permutation table for challenge */
    u8      ppi_perm_challenge[3][10] = 
            { { 1, 3, 0, 7, 5, 2, 9, 6, 4, 8 },
              { 6, 1, 9, 3, 8, 5, 7, 4, 0, 2 },
              { 4, 0, 3, 5, 7, 2, 8, 6, 1, 9 } };
    /* Permutation table for varient table */
    u8      ppi_perm_varient[2][32] =
            { { 0x0a, 0x08, 0x0e, 0x0c, 0x0b, 0x09, 0x0f, 0x0d,
                0x1a, 0x18, 0x1e, 0x1c, 0x1b, 0x19, 0x1f, 0x1d,
                0x02, 0x00, 0x06, 0x04, 0x03, 0x01, 0x07, 0x05,
                0x12, 0x10, 0x16, 0x14, 0x13, 0x11, 0x17, 0x15 },
              { 0x12, 0x1a, 0x16, 0x1e, 0x02, 0x0a, 0x06, 0x0e,
                0x10, 0x18, 0x14, 0x1c, 0x00, 0x08, 0x04, 0x0c,
                0x13, 0x1b, 0x17, 0x1f, 0x03, 0x0b, 0x07, 0x0f,
                0x11, 0x19, 0x15, 0x1d, 0x01, 0x09, 0x05, 0x0d } };
    u8      pi_css_secret[5] = { 0xE2,0xA3,0x45,0x10,0xF4 };
    u8      pi_css_varients[32] =
              { 0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15,
                0x20, 0x21, 0x24, 0x25, 0x30, 0x31, 0x34, 0x35,
                0x80, 0x81, 0x84, 0x85, 0x90, 0x91, 0x94, 0x95,
                0xA0, 0xA1, 0xA4, 0xA5, 0xB0, 0xB1, 0xB4, 0xB5 };
    u8      pi_bits[30];
    u8      pi_scratch[10];
    u8      pi_tmp1[5], pi_tmp2[2];
    u8      i_lfsr0_o, i_lfsr1_o;
    u32     i_lfsr0, i_lfsr1;
    u8     i_css_varient;
    int     i_val = 0;
    int     i_term = 0;
    int     i, i_index;

    for( i=0 ; i<10 ; i++ )
    {
        pi_scratch[i] = pi_challenge[ppi_perm_challenge[i_key_type][i]];
    }
    i_css_varient = i_key_type == 0 ? i_varient
                    : ppi_perm_varient[i_key_type-1][i_varient];
    i_css_varient = pi_css_varients[i_css_varient];

    for( i=0 ; i<5 ; i++ )
    {
        pi_tmp1[i] = pi_scratch[5+i] ^ pi_css_secret[i];
    }

    /* In order to ensure that the LFSR works we need to ensure that the
	 * initial values are non-zero.  Thus when we initialise them from
	 * the seed,  we ensure that a bit is set.
	 */
	i_lfsr0 = ( pi_key[0] << 17 ) | ( pi_key[1] << 9 ) |
              ( ( pi_key[2] & ~7 ) << 1 ) | 8 | (pi_key[2] & 7);

	/*
	 * reverse lfsr0/1 to simplify calculation in loop
	 */
	i_lfsr0 = ( pi_reverse[i_lfsr0 & 0xff] << 17 ) |
              ( pi_reverse[( i_lfsr0 >> 8 ) & 0xff] << 9 ) |
              ( pi_reverse[( i_lfsr0 >> 16 ) & 0xff] << 1) |
              ( i_lfsr0 >> 24 );

	i_lfsr1 = ( pi_reverse[pi_key[4]] << 9 ) | 0x100 |
              ( pi_reverse[pi_key[3]] );

	for( i=0 ; i<30 ; i++ )
    {
		i_lfsr0_o = ( i_lfsr0 >> 12) ^ ( i_lfsr0 >> 4) ^
                    ( i_lfsr0 >> 3) ^ i_lfsr0;

		i_lfsr1_o = ( ( i_lfsr1 >> 14 ) & 7) ^ i_lfsr1;
		i_lfsr1_o ^= ( i_lfsr1_o << 3 ) ^ ( i_lfsr1_o << 6 );

		i_lfsr1 = ( i_lfsr1 >> 8) ^ ( i_lfsr1_o << 9);
		i_lfsr0 = ( i_lfsr0 >> 8 ) ^ ( i_lfsr0_o << 17);

		i_lfsr0_o = ~i_lfsr0_o;
		i_lfsr1_o = ~i_lfsr1_o;

		i_val += i_lfsr0_o + i_lfsr1_o;
	
		pi_bits[i] = i_val & 0xFF;
		i_val >>= 8;
	}

    /* 
     * Mangling
     */
    for( i = 5, i_term = 0 ; --i >= 0 ; i_term = pi_scratch[i] )
    {
		i_index = pi_bits[25+i] ^ pi_scratch[i];
		i_index = pi_css_mangle1[i_index] ^ i_css_varient;
		pi_tmp1[i] = pi_css_mangle2[i_index] ^ i_term;
    }
    pi_tmp1[4] ^= pi_tmp1[0];
    for( i = 5, i_term = 0 ; --i >= 0 ; i_term = pi_tmp1[i] )
    {
		i_index = pi_bits[20+i] ^ pi_tmp1[i];
		i_index = pi_css_mangle1[i_index] ^ i_css_varient;
		pi_tmp2[i] = pi_css_mangle2[i_index] ^ i_term;
    }
    pi_tmp2[4] ^= pi_tmp2[0];
    for( i = 5, i_term = 0 ; --i >= 0 ; i_term = pi_tmp2[i] )
    {
		i_index = pi_bits[15+i] ^ pi_tmp2[i];
		i_index = pi_css_mangle1[i_index] ^ i_css_varient;
		i_index = pi_css_mangle2[i_index] ^ i_term;
        pi_tmp1[i] = pi_css_mangle0[i_index];
    }
    pi_tmp1[4] ^= pi_tmp1[0];
    for( i = 5, i_term = 0 ; --i >= 0 ; i_term = pi_tmp1[i] )
    {
		i_index = pi_bits[10+i] ^ pi_tmp1[i];
		i_index = pi_css_mangle1[i_index] ^ i_css_varient;
		i_index = pi_css_mangle2[i_index] ^ i_term;
        pi_tmp2[i] = pi_css_mangle0[i_index];
    }
    pi_tmp2[4] ^= pi_tmp2[0];
    for( i = 5, i_term = 0 ; --i >= 0 ; i_term = pi_tmp2[i] )
    {
		i_index = pi_bits[5+i] ^ pi_tmp2[i];
		i_index = pi_css_mangle1[i_index] ^ i_css_varient;
		pi_tmp1[i] = pi_css_mangle2[i_index] ^ i_term;
    }
    pi_tmp1[4] ^= pi_tmp1[0];
    for( i = 5, i_term = 0 ; --i >= 0 ; i_term = pi_tmp1[i] )
    {
		i_index = pi_bits[i] ^ pi_tmp1[i];
		i_index = pi_css_mangle1[i_index] ^ i_css_varient;
		pi_key[i] = pi_css_mangle2[i_index] ^ i_term;
    }

    return;
}
#else
typedef struct {
	uint8_t Challenge[10];
	uint8_t Key1[KEY_SIZE];
	uint8_t Key2[KEY_SIZE];
	uint8_t KeyCheck[KEY_SIZE];
	uint8_t varient;
} DiscKeys;

DiscKeys discKeys;



static void _generate_bits (u8 *output, int len,  const u8 *s)
{
 	u32 lfsr0, lfsr1;
	int val;
	u8 o_lfsr0, o_lfsr1;

	/* In order to ensure that the LFSR works we need to ensure that the
	 * initial values are non-zero.  Thus when we initialise them from
	 * the seed,  we ensure that a bit is set.
	 */

	lfsr0 = (s[0] << 17) | (s[1] << 9) | ((s[2] & ~7) << 1) | 8 | (s[2] & 7);

	/*
	 	reverse lfsr0/1 to simplify calculation in loop
	*/
	lfsr0 = (pi_reverse[lfsr0&0xff]<<17) | (pi_reverse[(lfsr0>>8)&0xff] << 9)
		  | (pi_reverse[(lfsr0>>16)&0xff]<<1) |(lfsr0>>24);

	lfsr1 = (pi_reverse[s[4]] << 9) | 0x100 | ( pi_reverse[ s[3]] );

	output++;
	val = 0;

	do {
		o_lfsr0 = (lfsr0 >> 12) ^ (lfsr0 >> 4) ^  (lfsr0 >> 3) ^ lfsr0;

		o_lfsr1 = ((lfsr1 >> 14) & 7) ^ lfsr1;
		o_lfsr1 ^= (o_lfsr1 << 3) ^ (o_lfsr1 << 6);

		lfsr1 = (lfsr1 >> 8) ^ (o_lfsr1 << 9);
		lfsr0 = (lfsr0 >> 8) ^ (o_lfsr0 << 17);

		o_lfsr0 = ~o_lfsr0;
		o_lfsr1 = ~o_lfsr1;

		val += o_lfsr0 + o_lfsr1;
	
		*--output = val & 0xFF;
		val >>= 8;
		fprintf(stderr, "lfsr0=%08x lfsr1=%08x\n", lfsr0, lfsr1);

	} while (--len > 0);
}


/**
 * This encryption engine implements one of 32 variations
 * one the same theme depending upon the choice in the
 * varient parameter (0 - 31).
 *
 * The algorithm itself manipulates a 40 bit input into
 * a 40 bit output.
 * The parameter 'input' is 80 bits.  It consists of
 * the 40 bit input value that is to be encrypted followed
 * by a 40 bit seed value for the pseudo random number
 * generators.
 **/

static inline void mangle1( u8 *bs, u8 cse, const u8 *ip, u8 *op)
{
	int i, term, index;
	for (i = 5, term = 0; --i >= 0; term = ip[i]) {
		index = bs[i] ^ ip[i];
		index = pi_css_mangle1[index] ^ cse;
		op[i] = pi_css_mangle2[index] ^ term;
        }	
}


/**
 *
 **/

static inline void mangle2( u8 *bs, u8 cse, const u8 *ip, u8 *op)
{
	int i, term, index;
	for (i = 5, term = 0; --i >= 0; term = ip[i]) {
		index = bs[i] ^ ip[i];
		index = pi_css_mangle1[index] ^ cse;
		index = pi_css_mangle2[index] ^ term;
                op[i] = pi_css_mangle0[index];
        }	
}


/**
 *
 **/

static inline void engine (int varient, u8 const *input, u8 *output)
{
	u8 cse;
	u8 tmp1[5];
	u8 tmp2[5];
	u8 bits[30];
    u8      pi_css_varients[32] =
              { 0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15,
                0x20, 0x21, 0x24, 0x25, 0x30, 0x31, 0x34, 0x35,
                0x80, 0x81, 0x84, 0x85, 0x90, 0x91, 0x94, 0x95,
                0xA0, 0xA1, 0xA4, 0xA5, 0xB0, 0xB1, 0xB4, 0xB5 };
    u8      pi_css_secret[5] = { 0xE2,0xA3,0x45,0x10,0xF4 };


	int i;

	/* Feed the secret into the input values such that
	 * we alter the seed to the LFSR's used above,  then
	 * generate the bits to play with.
	 */
	for (i=5; --i>=0; )
		tmp1[i] = input[5+i] ^ pi_css_secret[i];

	_generate_bits (&bits[29], sizeof bits, &tmp1[0]);

	/* This term is used throughout the following to
	 * select one of 32 different variations on the
	 * algorithm.
	 */
	cse = pi_css_varients[varient];

	/* Now the actual blocks doing the encryption.  Each
	 * of these works on 40 bits at a time and are quite
	 * similar.
	 */
	mangle1( &bits[25], cse, input, tmp1);
	tmp1[4] ^= tmp1[0];

	mangle1( &bits[20],  cse, tmp1, tmp2);
	tmp2[4] ^= tmp2[0];

	mangle2( &bits[15],  cse, tmp2, tmp1);
	tmp1[4] ^= tmp1[0];

	mangle2( &bits[10],  cse, tmp1, tmp2);
	tmp2[4] ^= tmp2[0];

	mangle1( &bits[5],  cse, tmp2, tmp1);
	tmp1[4] ^= tmp1[0];
	mangle1( &bits[0],  cse, tmp1, output);
}


/**
 *
 **/

void CryptKey1 (int varient, u8 const *challenge, u8 *key)
{
	u8 perm_challenge[] = {1,3,0,7,5, 2,9,6,4,8};

	u8 scratch[10];
	int i;

	for (i=9; i>=0; --i)
		scratch[i] = challenge[perm_challenge[i]];

	engine (varient, scratch, key);
}


/**
 * This shuffles the bits in varient to make perm_varient such that
 *                4 -> !3
 *                3 ->  4
 * varient bits:  2 ->  0  perm_varient bits
 *                1 ->  2
 *                0 -> !1
 **/

void CryptKey2 (int varient, u8 const *challenge, u8 *key)
{
	static u8 perm_challenge[] = {
		0x06,0x01,0x09,0x03,0x08,0x05,0x07,0x04,
		0x00,0x02
	};
	static u8 perm_varient[] = {
		0x0a,0x08,0x0e,0x0c,0x0b,0x09,0x0f,0x0d,
		0x1a,0x18,0x1e,0x1c,0x1b,0x19,0x1f,0x1d,
		0x02,0x00,0x06,0x04,0x03,0x01,0x07,0x05,
		0x12,0x10,0x16,0x14,0x13,0x11,0x17,0x15
	};
	u8 scratch[10];
	int i;

	for (i=9; i>=0; --i)
		scratch[i] = challenge[perm_challenge[i]];

	engine (perm_varient[varient], scratch, key);
}


/**
 * This shuffles the bits in varient to make perm_varient such that
 *                4 ->  0
 *                3 -> !1
 * varient bits:  2 -> !4  perm_varient bits
 *                1 ->  2
 *                0 ->  3
 **/

void CryptBusKey (int varient, u8 const *challenge, u8 *key)
{
	static u8 perm_challenge[] = {
		0x04,0x00,0x03,0x05,0x07,0x02,0x08,0x06,
		0x01,0x09
	};
	static u8 perm_varient[] = {
		0x12,0x1a,0x16,0x1e,0x02,0x0a,0x06,0x0e,
		0x10,0x18,0x14,0x1c,0x00,0x08,0x04,0x0c,
		0x13,0x1b,0x17,0x1f,0x03,0x0b,0x07,0x0f,
		0x11,0x19,0x15,0x1d,0x01,0x09,0x05,0x0d
	};
	u8 scratch[10];
	int i;

	for (i=9; i>=0; --i)
		scratch[i] = challenge[perm_challenge[i]];

	engine (perm_varient[varient], scratch, key);
}

static int _CSSAuthDrive (const u8 *key, DiscKeys *disc)
{
	int i;

	for (i=0; i<KEY_SIZE; i++)
		disc->Key1[i] = key[4-i];

	for (i=0; i<32; ++i) {
		CryptKey1( i, disc->Challenge, disc->KeyCheck);

		if (memcmp (disc->KeyCheck, disc->Key1, KEY_SIZE ) == 0) {
			intf_ErrMsg( "Drive Authentic - using varient %d", i);
			disc->varient = i;
			return i;
		}
	}

	intf_ErrMsg( "Drive would not Authenticate" );

	return -1;
}


static int _CSSAuthHost (dvd_authinfo *ai, DiscKeys *disc)
{
	int i;

	switch (ai->type) {
	/* Host data receive (host changes state) */
	case DVD_LU_SEND_AGID:
//		_CSSPrintDebug ("AGID %d\n", ai->lsa.agid);

		ai->type = DVD_HOST_SEND_CHALLENGE;
		break;

	case DVD_LU_SEND_KEY1:
//		_CSSPrintDebugBytes ("LU sent key1", ai->lsk.key, LEN_KEY);

		if (_CSSAuthDrive (ai->lsk.key, disc)<0) {
			ai->type = DVD_AUTH_FAILURE;
			return -22;
		}
		ai->type = DVD_LU_SEND_CHALLENGE;
		break;

	case DVD_LU_SEND_CHALLENGE:
		for (i = 0; i < 10; ++i)
			disc->Challenge[i] = ai->hsc.chal[9-i];

//		_CSSPrintDebugBytes ("LU sent challenge", ai->hsc.chal, 10);

		CryptKey2 (disc->varient, disc->Challenge, disc->Key2);
		ai->type = DVD_HOST_SEND_KEY2;
		break;

	/* Host data send */
	case DVD_HOST_SEND_CHALLENGE:
		for (i = 0; i < 10; ++i)
			ai->hsc.chal[9-i] = disc->Challenge[i];

//		_CSSPrintDebugBytes ("Host sending challenge", ai->hsc.chal, 10);

		/* Returning data, let LU change state */
		break;

	case DVD_HOST_SEND_KEY2:
		for (i = 0; i < KEY_SIZE; ++i)
			ai->hsk.key[4-i] = disc->Key2[i];

//		_CSSPrintDebugBytes ("Host sending key 2", &disc->Key2[0], LEN_KEY);

		/* Returning data, let LU change state */
		break;

	default:
		intf_ErrMsg( "Got invalid state %d", ai->type );

		return -22;
	}

	return 0;
}



#endif

/*****************************************************************************
 * CSSCracker : title key decryption by cracking
 *****************************************************************************/

// FIXME : adapt to vlc
#define KEYSTREAMBYTES 10

static unsigned char invtab4[256];

static int CSScracker(int StartVal,unsigned char* pCrypted,unsigned char* pDecrypted,DVD40bitKey *StreamKey,DVD40bitKey *pkey)
{
    unsigned char MyBuf[10];
    unsigned int t1,t2,t3,t4,t5,t6;
    unsigned int nTry;
    unsigned int vCandidate;
    int i;
    unsigned int j;
    int i_exit = -1;


    for (i=0;i<10;i++)
    {
        MyBuf[i] = pi_css_tab1[pCrypted[i]] ^ pDecrypted[i]; 
    }

    /* Test that CSStab4 is a permutation */
    memset( invtab4, 0, 256 );
    for( i = 0 ; i < 256 ; i++ )
    {
        invtab4[ pi_css_tab4[i] ] = 1; 
    }

    for (i = 0 ; i < 256 ; i++)
    {
        if (invtab4[ i ] != 1)
        {
            printf( "Permutation error\n" );
            exit( -1 );
        }
	}

    /* initialize the inverse of table4 */
    for( i = 0 ; i < 256 ; i++ )
    {
        invtab4[ pi_css_tab4[i] ] = i;
    }

    for( nTry = StartVal ; nTry < 65536 ; nTry++ )
    {
        t1 = nTry >> 8 | 0x100;
        t2 = nTry & 0xff;
        t3 = 0;   /* not needed */
        t5 = 0;

        /* iterate cipher 4 times to reconstruct LFSR2 */
        for( i = 0 ; i < 4 ; i++ )
        {
            /* advance LFSR1 normaly */
            t4=pi_css_tab2[t2]^pi_css_tab3[t1];
            t2=t1>>1;
            t1=((t1&1)<<8)^t4;
            t4=pi_css_tab5[t4];
            /* deduce t6 & t5 */
            t6 = MyBuf[ i ];    
            if( t5 )
            {
                t6 = ( t6 + 0xff )&0x0ff;
            }
            if( t6 < t4 )
            {
                t6 += 0x100;
            }
            t6 -= t4;
            t5 += t6 + t4;
            t6 = invtab4[ t6 ];
            /* printf( "%02x/%02x ", t4, t6 ); */
            /* feed / advance t3 / t5 */
            t3 = (t3 << 8) | t6;
            t5 >>= 8;
        }

        vCandidate = t3;

        /* iterate 6 more times to validate candidate key */
        for( ; i < KEYSTREAMBYTES ; i++ )
        {
            t4=pi_css_tab2[t2]^pi_css_tab3[t1];
            t2=t1>>1;
            t1=((t1&1)<<8)^t4;
            t4=pi_css_tab5[t4];
            t6=(((((((t3>>3)^t3)>>1)^t3)>>8)^t3)>>5)&0xff;
            t3=(t3<<8)|t6;
            t6=pi_css_tab4[t6];
            t5+=t6+t4;
            if( (t5 & 0xff) != MyBuf[i] ) break;
            t5>>=8;
        }

        if( i == KEYSTREAMBYTES )
        {
            /* Do 4 backwards steps of iterating t3 to deduce initial state */
            t3 = vCandidate;
            for( i = 0 ; i < 4 ; i++ )
            {
                t1 = t3 & 0xff;
                t3 = ( t3 >> 8 );
                /* easy to code, and fast enough bruteforce
                 * search for byte shifted in */
                for( j=0 ; j < 256 ; j++ )
                {
                    t3 = (t3 & 0x1ffff) | ( j << 17 );
                    t6=(((((((t3>>3)^t3)>>1)^t3)>>8)^t3)>>5)&0xff;
                    if( t6 == t1 ) break;  
                }
            }
//          printf( "Candidate: t1=%03x t2=%02x t3=%08x\n", 0x100|(nTry>>8),nTry&0x0ff, t3 );

            t4 = (t3>>1) - 4;
            for(t5=0;t5<8;t5++)
            {
                if ( ((t4+t5)*2 + 8 - ((t4+t5)&7))==t3 )
                {
                    (*pkey)[0] = nTry>>8;
                    (*pkey)[1] = nTry & 0xFF;
                    (*pkey)[2] = ((t4+t5) >> 0) & 0xFF;
                    (*pkey)[3] = ((t4+t5) >> 8) & 0xFF;
                    (*pkey)[4] = ((t4+t5) >> 16) & 0xFF;
                    i_exit = nTry+1;
                }
            }
        }
    }

    if (i_exit>=0)
    {
        (*pkey)[0] ^= (*StreamKey)[0];
        (*pkey)[1] ^= (*StreamKey)[1];
        (*pkey)[2] ^= (*StreamKey)[2];
        (*pkey)[3] ^= (*StreamKey)[3];
        (*pkey)[4] ^= (*StreamKey)[4];
    }

    return i_exit;
}

/*
 * Authentication and keys
 */

/*****************************************************************************
 * CSSInit : CSS Structure initialisation and DVD authentication.
 *****************************************************************************/

css_t CSSInit( int i_fd )
{
#if 0
    css_t           css;
    dvd_authinfo    auth;
    int             i;

    css.i_fd = i_fd;
    css.b_error = 0;
    memset( &auth, 0, sizeof(auth) );
fprintf( stderr, "0\n" );
#if 0
	if( CSSGetASF( i_fd ) )
    {
        css.b_error = 1;
		return css;
    }
#endif
fprintf( stderr, "1\n" );
    /* Get AGID From drive */
    auth.type = DVD_LU_SEND_AGID;
    auth.lsa.agid = 0;

    if( ioctl( i_fd, DVD_AUTH, &auth ) < 0 )
    {
        intf_ErrMsg( "DVD ioctl error in CSSInit (AGID)" );
        auth.type = DVD_INVALIDATE_AGID;
        auth.lsa.agid = 0;
        ioctl( i_fd, DVD_AUTH, &auth );
        css.b_error = 1;
        return css;
    }
    css.i_agid = auth.lsa.agid;

fprintf( stderr, "2\n" );
    /* Get drive keys */
    for( i = 0 ; i < 10 ; ++i )
    {
		css.keys.pi_challenge[i] = i;
    }
fprintf( stderr, "3\n" );

    if( ioctl( i_fd, DVD_AUTH, &auth ) < 0 )
    {
        intf_ErrMsg( "DVD ioctl error in CSSInit (Drive keys)" );
        css.b_error = 1;
        return css;
    }
fprintf( stderr, "4\n" );

    if( ioctl( i_fd, DVD_AUTH, &auth ) < 0 )
    {
        intf_ErrMsg( "DVD ioctl error in CSSInit (Drive keys)" );
        css.b_error = 1;
        return css;
    }
fprintf( stderr, "5\n" );

    for( i=0 ; i<KEY_SIZE ; i++ )
    {
        css.keys.pi_key1[i] = auth.lsk.key[KEY_SIZE -1 -i];
    }
fprintf( stderr, "6\n" );

    i=0;
    do {
        CSSCryptKey( 0, i, css.keys.pi_challenge, css.keys.pi_key_check );
        i++;

        intf_Msg( "Drive Authentic - using varient %d", i-1 );
            
    } while( (i<32) &&
      ( memcmp( css.keys.pi_key_check, css.keys.pi_key1, KEY_SIZE ) != 0 ) );
fprintf( stderr, "1\n" );

    css.keys.i_version = i-1;

    auth.type = DVD_LU_SEND_CHALLENGE;
    if( ioctl( i_fd, DVD_AUTH, &auth ) < 0 )
    {
		intf_ErrMsg( "DVD ioctl error in CSSInit (drive Keys)" );
        css.b_error = 1;
		return css;
    }
fprintf( stderr, "7\n" );

    for( i = 0; i < 2*KEY_SIZE; ++i )
    {
        css.keys.pi_challenge[i] = auth.hsc.chal[2*KEY_SIZE -1 -i];
    }
    CSSCryptKey( 1, css.keys.i_version,
                    css.keys.pi_challenge, css.keys.pi_key2 );
    
    auth.type = DVD_HOST_SEND_KEY2;
    for( i = 0 ; i < KEY_SIZE ; ++i )
    {
        auth.hsk.key[4-i] = css.keys.pi_key2[i];
    }
fprintf( stderr, "8\n" );

    if( ioctl( i_fd, DVD_AUTH, &auth ) < 0)
    {
        intf_ErrMsg( "Send key2 to LU failed (expected)" );
        css.b_error = 1;
        return css;
    }
fprintf( stderr, "9\n" );

	memcpy( css.keys.pi_challenge, css.keys.pi_key1, KEY_SIZE );
	memcpy( css.keys.pi_challenge+KEY_SIZE, css.keys.pi_key2, KEY_SIZE );
fprintf( stderr, "10\n" );
	CSSCryptKey( 2, css.keys.i_version,
                    css.keys.pi_challenge, css.keys.pi_key_check );

    intf_WarnMsg( 3, "DVD agid : %d", css.i_agid );
fprintf( stderr, "11\n" );
#else

	DiscKeys *disc = &discKeys;

 	dvd_authinfo ai;
	int i, agid;
	dvd_struct s;
    int fd=i_fd;
	int tries;
	int rv=-1;
    css_t   css;



	memset (&ai, 0, sizeof (ai));

	if (CSSGetASF (fd))
		return css;


	/* Init sequence, request AGID */
	for (tries = 1; (tries < 4)&&(rv== -1) ; ++tries) {
		intf_ErrMsg( "Request AGID [%d]...", tries );
		ai.type = DVD_LU_SEND_AGID;
		ai.lsa.agid = 0;
		rv =  ioctl (fd, DVD_AUTH, &ai);
		if (rv == -1) {
       		intf_ErrMsg( "N/A, invalidating" );
	    	ai.type = DVD_INVALIDATE_AGID;
		    ai.lsa.agid = 0;
    		ioctl (fd, DVD_AUTH, &ai);
        }
	}
    if (rv != -1) {
    	intf_ErrMsg("Cannot get AGID\n");
    }

	for (i = 0; i < 10; ++i)
		disc->Challenge[i] = i;

	/* Send AGID to host */
	if (_CSSAuthHost(&ai, disc) < 0) {
		intf_ErrMsg("CSS Error :Send AGID to host failed\n");
		return css;
	}
	/* Get challenge from host */
	if (_CSSAuthHost(&ai, disc) < 0) {
		intf_ErrMsg("CSS Error :Get challenge from host failed\n");
		return css;
	}
	agid = ai.lsa.agid;
	/* Send challenge to LU */
	if (ioctl(fd, DVD_AUTH, &ai) < 0) {
		intf_ErrMsg("CSS Error :Send challenge to LU failed\n");
		return css;
	}
	/* Get key1 from LU */
	if (ioctl(fd, DVD_AUTH, &ai) < 0) {
		intf_ErrMsg("CSS Error :Get key1 from LU failed\n");
		return css;
	}
	/* Send key1 to host */
	if (_CSSAuthHost(&ai, disc) < 0) {
		intf_ErrMsg("CSS Error :Send key1 to host failed\n");
		return css;
	}
	/* Get challenge from LU */
	if (ioctl(fd, DVD_AUTH, &ai) < 0) {
		intf_ErrMsg("CSS Error :Get challenge from LU failed\n");
		return css;
	}
	/* Send challenge to host */
	if (_CSSAuthHost (&ai, disc) < 0) {
		intf_ErrMsg("CSS Error :Send challenge to host failed\n");
		return css;
	}
	/* Get key2 from host */
	if (_CSSAuthHost (&ai, disc) < 0) {
		intf_ErrMsg("CSS Error :Get key2 from host failed\n");
		return css;
	}
	/* Send key2 to LU */
	if (ioctl(fd, DVD_AUTH, &ai) < 0) {
		intf_ErrMsg("CSS Error :Send key2 to LU failed (expected)\n");
		return css;
	}

	if (ai.type == DVD_AUTH_ESTABLISHED)
		intf_ErrMsg("CSS Error :DVD is authenticated\n");
	else if (ai.type == DVD_AUTH_FAILURE)
		intf_ErrMsg("CSS Error :DVD authentication failed\n");

	memcpy (disc->Challenge, disc->Key1, KEY_SIZE);
	memcpy (disc->Challenge+KEY_SIZE, disc->Key2, KEY_SIZE);
	CryptBusKey (disc->varient, disc->Challenge, disc->KeyCheck);

        intf_Msg( "Received Session Key\n" );
    for( i=0 ; i<KEY_SIZE ; i++ )
    {
        intf_Msg( "%02x", &disc->KeyCheck[i] );
    }
    intf_Msg( "\n" );

	if (agid < 0)
		return css;

	if (CSSGetASF (fd) < 0)
		return css;

	s.type = DVD_STRUCT_DISCKEY;
	s.disckey.agid = agid;
	memset (s.disckey.value, 0, 2048);

	if (ioctl(fd, DVD_READ_STRUCT, &s)<0) {
		intf_ErrMsg( "Error : Could not read Disc Key" );
        css.b_error = 1;
		return css;
	}

	for (i=0; i<sizeof s.disckey.value; i++)
		s.disckey.value[i] ^= disc->KeyCheck[4 - (i % KEY_SIZE)];

//	_CSSPrintDebugBytes ("Received Disc Key", s.disckey.value, 10);

	memcpy (disc->KeyCheck, s.disckey.value, 2048);


	if (CSSGetASF (fd) < 0)
    {
        css.b_error = 1;
		return css;
    }



#endif

    return css;
}

/*****************************************************************************
 * CSSGetKeys : get the disc key of the media en the dvd device, and then
 * the title keys.
 * The DVD should have been opened before.
 *****************************************************************************/

#define MaxKeys 1000

struct
{
  int verbosity ; /* 0-9 */
	int repetitions ;
	int scanall ;
	char *infile ;
} params ;

typedef struct
{
	int				occ;
	DVD40bitKey		key;
} KeyOcc;


int CSSGetKeys( css_t * p_css )
{
#if 0

    /* 
     * css_auth/libcss method from Derek Fawcus.
     * Get encrypted keys with ioctls and decrypt them
     * with cracked player keys
     */
    dvd_struct      dvd;
    dvd_authinfo    auth;
    int             i, j;
    if( CSSGetASF( p_css->i_fd ) < 0 )
    {
        return 1;
    }

    /* Disk key */
    dvd.type = DVD_STRUCT_DISCKEY;
    dvd.disckey.agid = p_css->i_agid;
    memset( dvd.disckey.value, 0, 2048 );
    
    if( ioctl( p_css->i_fd, DVD_READ_STRUCT, &dvd )<0 )
    {
        intf_ErrMsg( "DVD ioctl error in CSSGetKeys" );
        p_css->b_error = 1;
        return 1;
    }

    for( i=0 ; i<sizeof(dvd.disckey.value) ; i++ )
    {
        dvd.disckey.value[i] ^=
                    p_css->keys.pi_key_check[4 - (i % KEY_SIZE )];
    }

    memcpy( p_css->pi_disc_key, dvd.disckey.value, 2048 );

    if( CSSGetASF( p_css->i_fd ) < 0 )
    {
        return 1;
    }

    /* Title keys */
    auth.type = DVD_LU_SEND_TITLE_KEY;
    auth.lstk.agid = p_css->i_agid;

    for( j=0 ; j<p_css->i_title_nb ; j++ )
    {
        auth.lstk.lba = p_css->p_title_key[j].i_lba;

        if( ioctl( p_css->i_fd, DVD_AUTH, &auth )<0 )
        {
            intf_ErrMsg( "DVD ioctl error in CSSGetKeys" );
            p_css->b_error = 1;
            return 1;
        }
    
        for( i=0 ; i<KEY_SIZE ; ++i )
        {
            auth.lstk.title_key[i] ^=
                    p_css->keys.pi_key_check[4 - (i % KEY_SIZE)];
            memcpy( p_css->p_title_key[j].key, auth.lstk.title_key, KEY_SIZE );
        }
    }

    if( CSSGetASF( p_css->i_fd ) < 0 )
    {
        return 1;
    }

#else

    /* 
     * Cracking method from Ethan Hawke.
     * Does not use key tables and ioctls.
     */ 
    FILE*   in;
    unsigned 	 		 char	buf[0x800] ;
	DVD40bitKey		 MyKey ;
	int						 pos ;
	int						 BytesRead ;
	int						 BestPLen ;
	int						 BestP ;
	int						 i,j,k ;
	int            encrypted=0 ;
	int 					 filsize ;
	KeyOcc				 PosKey[MaxKeys] ;
	int						 RegisteredKeys = 0 ;
	int						 TotalKeysFound = 0 ;
	int						 StopScanning   = 0 ;
	int						 highest=0 ;


if( (in = fopen( "/dvd/video_ts/vts_01_1.vob", "rb") ) )
{
fprintf( stderr, "CSS:File opened\n" );
pos = 0;
fseek(in,0,SEEK_END);
filsize = ftell(in);
fseek(in,0,SEEK_SET);
do
{
if (params.verbosity >= 1 && filsize > 1024*1024)
{
fprintf(stderr, "%.2f of file read, %i keys found...\r",
pos*100.0/filsize,
TotalKeysFound);
}
BytesRead = fread(buf,1,0x800,in);
if (buf[0x14] & 0x30) // PES_scrambling_control
{
encrypted = 1;
BestPLen = 0;
BestP = 0;
for(i=2;i<0x30;i++)
{
for(j=i; (j < 0x80) && (buf[0x7F - (j%i)] == buf[0x7F-j]); j++);
if ((j>BestPLen) && (j>i))
{
BestPLen = j;
BestP = i;
}
}
if ((BestPLen > 20) && (BestPLen / BestP >= 2))
{
i = CSScracker(0,&buf[0x80],&buf[0x80-(BestPLen/BestP)*BestP],(DVD40bitKey*)&buf[0x54],&MyKey);
while (i>=0)
{
k = 0;
for(j=0;j<RegisteredKeys;j++)
{
if (memcmp(&(PosKey[j].key),&MyKey,sizeof(DVD40bitKey))==0)
{
PosKey[j].occ++ ;
TotalKeysFound++ ;
k = 1 ;
}
}
if (k==0)
{
memcpy(&(PosKey[RegisteredKeys].key), &MyKey, sizeof(DVD40bitKey)) ;
PosKey[RegisteredKeys++].occ = 1 ;
TotalKeysFound++ ;
}
if (params.verbosity >= 2)
{
fprintf(stderr, "\nOfs:%08X - Key: %02X %02X %02X %02X %02X\n",
pos, MyKey[0] ,MyKey[1], MyKey[2],
MyKey[3], MyKey[4]) ;
}
i = CSScracker(i,&buf[0x80],&buf[0x80-(BestPLen/BestP)*BestP],(DVD40bitKey*)&buf[0x54],&MyKey);





}
if (params.scanall==0 && RegisteredKeys==1 && PosKey[0].occ >= params.repetitions)
{
StopScanning = 1;
}
}
}

pos += BytesRead;
} while (BytesRead==0x800 && !StopScanning);
fclose(in);
if (params.verbosity >= 1 && StopScanning)
{
fprintf(stderr, "Found enough occurancies of the same key. Scan stopped.");
}
if (params.verbosity >= 1)
{
fprintf(stderr, "\n\n") ;
}
}
else
{
fprintf(stderr, "FILE ERROR: File could not be opened. [Check if file is readable]\n");
return(1);
}
if (!encrypted)
{
fprintf(stderr, "This file was _NOT_ encrypted!\n");
return(0);
}
if (encrypted && RegisteredKeys==0)
{
fprintf(stderr, "Sorry... Unable to determine keys from file.\n");
return(1);
}
for(i=0;i<RegisteredKeys-1;i++)
{
for(j=i+1;j<RegisteredKeys;j++)
{
if (PosKey[j].occ>PosKey[i].occ)
{
memcpy(&MyKey,&(PosKey[j].key),sizeof(DVD40bitKey));
k = PosKey[j].occ;
memcpy(&(PosKey[j].key),&(PosKey[i].key),sizeof(DVD40bitKey));
PosKey[j].occ = PosKey[i].occ;
memcpy(&(PosKey[i].key),&MyKey,sizeof(DVD40bitKey));
PosKey[i].occ = k;
}
}
}
if (params.verbosity >= 1)
{
fprintf(stderr, " Key(s) & key probability\n--------------------------\n");
highest=0 ;
for(i=0; i<RegisteredKeys; i++)
{
fprintf(stderr, "%d) %02X %02X %02X %02X %02X - %3.2f%%\n", i,
PosKey[i].key[0],PosKey[i].key[1],PosKey[i].key[2],
PosKey[i].key[3],PosKey[i].key[4],
PosKey[i].occ*100.0/TotalKeysFound) ;
if (PosKey[highest].occ*100.0/TotalKeysFound
    <= PosKey[i].occ*100.0/TotalKeysFound)
{
highest=i ;
}
}
fprintf(stderr, "\n");
}
/* The "find the key with the highest probability" code
is untested, as I haven't been able to find a VOB that
produces multiple keys (RT)
*/
printf("%02X %02X %02X %02X %02X\n", 
PosKey[highest].key[0],PosKey[highest].key[1],PosKey[highest].key[2],
PosKey[highest].key[3],PosKey[highest].key[4]) ;

    for( i=0 ; i<5 ; i++ )
    {
//        p_css->p_title_key->key[i] = PosKey[highest].key[i];
    }

#endif
    return 0;
}

/*****************************************************************************
 * CSSDescrambleSector
 *****************************************************************************/
int CSSDescrambleSector( DVD40bitKey* key, unsigned char * sec )
{
#if 0
    FILE					*in,*out ;
	unsigned 	 		 char	buf[0x800] ;
	int						 pos ;
	int						 BytesRead ;
	int						 BytesWritten ;
	int 					 filsize ;

    do {
    BytesRead = fread(&buf,1,0x800,in);
    if (buf[0x14] & 0x30) // PES_scrambling_control
    {
        unsigned int t1,t2,t3,t4,t5,t6;
        unsigned char *end=sec+0x800;

        t1=((*key)[0] ^ sec[0x54]) | 0x100;
        t2=(*key)[1] ^ sec[0x55];
        t3= (((*key)[2]) | ((*key)[3]<<8) |
            ((*key)[4]<<16)) ^ ((sec[0x56]) |
            (sec[0x57]<<8) | (sec[0x58]<<16));
        t4=t3&7;
        t3=t3*2+8-t4;
        sec+=0x80;
        t5=0;

        while(sec!=end)
        {
            t4 = pi_css_tab2[t2] ^ pi_css_tab3[t1];
            t2=t1>>1;
            t1=((t1&1)<<8)^t4;
            t4 = pi_css_tab5[t4];
            t6=(((((((t3>>3)^t3)>>1)^t3)>>8)^t3)>>5)&0xff;
            t3=(t3<<8)|t6;
            t6 = pi_css_tab4[t6];
            t5+=t6+t4;
            *sec++= pi_css_tab1[*sec]^(t5&0xff);
            t5>>=8;
        }

        buf[0x14] &= 0x8F;
    }

    BytesWritten = fwrite(&buf,1,BytesRead,out);
    if (BytesWritten!=BytesRead)
    {
        fprintf(stderr, "Could not write to output file.\n");
        return(1);
    }
    pos += BytesRead;
    } while (BytesRead==0x800);

#endif
	return(0);
}
