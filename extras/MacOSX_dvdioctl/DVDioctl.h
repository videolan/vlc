/*****************************************************************************
 * DVDioctl.h: Linux-like DVD driver for Darwin and MacOS X
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: DVDioctl.h,v 1.1 2001/04/02 23:30:41 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

struct sum { int a, b, r; };
#define IODVD_READ_STRUCTURE _IOWR('B', 1, struct sum)
#define IODVD_SEND_KEY       _IOWR('B', 2, struct sum)
#define IODVD_REPORT_KEY     _IOWR('B', 3, struct sum)

