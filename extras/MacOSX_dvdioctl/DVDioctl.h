/*****************************************************************************
 * DVDioctl.h: Linux-like DVD driver for Darwin and MacOS X
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: DVDioctl.h,v 1.4 2001/06/25 11:34:08 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Eugenio Jarosiewicz <ej0@cise.ufl.edu>
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *****************************************************************************/

#if defined(KERNEL)
/* Everything has already been defined */
#else
enum DVDKeyFormat
{
    kCSSAGID        = 0x00,
    kChallengeKey   = 0x01,
    kKey1           = 0x02,
    kKey2           = 0x03,
    kTitleKey       = 0x04,
    kASF            = 0x05,
    kSetRegion      = 0x06,
    kRPCState       = 0x08,
    kCSS2AGID       = 0x10,
    kCPRMAGID       = 0x11,
    kInvalidateAGID = 0x3f
};

enum DVDKeyClass
{
    kCSS_CSS2_CPRM  = 0x00,
    kRSSA           = 0x01
};
#endif

typedef struct dvdioctl_data
{
    void         *p_buffer;

#if defined(KERNEL)
    UInt32        i_size;
    UInt32        i_lba;
    UInt8         i_agid;
#else
    u32           i_size;
    u32           i_lba;
    u8            i_agid;
#endif

    int           i_keyclass;
    int           i_keyformat;

} dvdioctl_data_t;

#define IODVD_READ_STRUCTURE _IOWR('B', 1, dvdioctl_data_t)
#define IODVD_SEND_KEY       _IOWR('B', 2, dvdioctl_data_t)
#define IODVD_REPORT_KEY     _IOWR('B', 3, dvdioctl_data_t)

