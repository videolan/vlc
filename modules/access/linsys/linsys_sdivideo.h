/* sdivideo.h
 *
 * Shared header file for the Linux user-space API for
 * Linear Systems Ltd. SMPTE 292M and SMPTE 259M-C interface boards.
 *
 * Copyright (C) 2009-2010 Linear Systems Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of Linear Systems Ltd. nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY LINEAR SYSTEMS LTD. "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL LINEAR SYSTEMS LTD. OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Linear Systems can be contacted at <http://www.linsys.ca/>.
 *
 */

#ifndef _SDIVIDEO_H
#define _SDIVIDEO_H

/* Driver info */
#define SDIVIDEO_DRIVER_NAME "sdivideo"

#define SDIVIDEO_MAJOR 0	/* Set to 0 for dynamic allocation.
			 * See /usr/src/linux/Documentation/devices.txt */

#define SDIVIDEO_TX_BUFFERS_MIN 2 /* This must be at least 2 */
/* The minimum transmit buffer size must be positive, divisible by 4,
 * and large enough that the buffers aren't transferred to the onboard FIFOs
 * too quickly for the machine to handle the interrupts.
 * This is especially a problem at startup, when the FIFOs are empty.
 * Relevant factors include onboard FIFO size, PCI bus throughput,
 * processor speed, and interrupt latency. */
#define SDIVIDEO_TX_BUFSIZE_MIN 1024
#define SDIVIDEO_RX_BUFFERS_MIN 2 /* This must be at least 2 */
#define SDIVIDEO_RX_BUFSIZE_MIN 8 /* This must be positive and divisible by 4 */

#define SDIVIDEO_TX_BUFFERS 30 /* This must be at least 2 */
#define SDIVIDEO_TX_BUFSIZE 1843200 /* This must be positive and divisible by 4 */
#define SDIVIDEO_RX_BUFFERS 30 /* This must be at least 2 */
#define SDIVIDEO_RX_BUFSIZE 1843200 /* This must be positive and divisible by 4 */

/* Ioctl () definitions */
#define SDIVIDEO_IOC_MAGIC '=' /* This ioctl magic number is currently free. See
			   * /usr/src/linux/Documentation/ioctl-number.txt */

#define SDIVIDEO_IOC_TXGETCAP		_IOR(SDIVIDEO_IOC_MAGIC, 1, unsigned int)
#define SDIVIDEO_IOC_TXGETEVENTS	_IOR(SDIVIDEO_IOC_MAGIC, 2, unsigned int)
#define SDIVIDEO_IOC_TXGETBUFLEVEL	_IOR(SDIVIDEO_IOC_MAGIC, 3, unsigned int)
#define SDIVIDEO_IOC_TXGETTXD		_IOR(SDIVIDEO_IOC_MAGIC, 4, int)
#define SDIVIDEO_IOC_TXGETREF		_IOR(SDIVIDEO_IOC_MAGIC, 5, unsigned int)

#define SDIVIDEO_IOC_RXGETCAP		_IOR(SDIVIDEO_IOC_MAGIC, 65, unsigned int)
#define SDIVIDEO_IOC_RXGETEVENTS	_IOR(SDIVIDEO_IOC_MAGIC, 66, unsigned int)
#define SDIVIDEO_IOC_RXGETBUFLEVEL	_IOR(SDIVIDEO_IOC_MAGIC, 67, unsigned int)
#define SDIVIDEO_IOC_RXGETCARRIER	_IOR(SDIVIDEO_IOC_MAGIC, 68, int)
#define SDIVIDEO_IOC_RXGETSTATUS	_IOR(SDIVIDEO_IOC_MAGIC, 69, int)
#define SDIVIDEO_IOC_RXGETYCRCERROR	_IOR(SDIVIDEO_IOC_MAGIC, 70, unsigned int)
#define SDIVIDEO_IOC_RXGETCCRCERROR	_IOR(SDIVIDEO_IOC_MAGIC, 71, unsigned int)
#define SDIVIDEO_IOC_RXGETVIDSTATUS	_IOR(SDIVIDEO_IOC_MAGIC, 72, unsigned int)

#define SDIVIDEO_IOC_GETID		_IOR(SDIVIDEO_IOC_MAGIC, 129, unsigned int)
#define SDIVIDEO_IOC_GETVERSION		_IOR(SDIVIDEO_IOC_MAGIC, 130, unsigned int)
/* Provide compatibility with applications compiled for older API */
#define SDIVIDEO_IOC_QBUF_DEPRECATED	_IOW(SDIVIDEO_IOC_MAGIC, 131, unsigned int)
#define SDIVIDEO_IOC_QBUF		_IO(SDIVIDEO_IOC_MAGIC, 131)
/* Provide compatibility with applications compiled for older API */
#define SDIVIDEO_IOC_DQBUF_DEPRECATED	_IOW(SDIVIDEO_IOC_MAGIC, 132, unsigned int)
#define SDIVIDEO_IOC_DQBUF		_IO(SDIVIDEO_IOC_MAGIC, 132)

/* Transmitter event flag bit locations */
#define SDIVIDEO_EVENT_TX_BUFFER_ORDER	0
#define SDIVIDEO_EVENT_TX_BUFFER	(1 << SDIVIDEO_EVENT_TX_BUFFER_ORDER)
#define SDIVIDEO_EVENT_TX_FIFO_ORDER	1
#define SDIVIDEO_EVENT_TX_FIFO		(1 << SDIVIDEO_EVENT_TX_FIFO_ORDER)
#define SDIVIDEO_EVENT_TX_DATA_ORDER	2
#define SDIVIDEO_EVENT_TX_DATA		(1 << SDIVIDEO_EVENT_TX_DATA_ORDER)
#define SDIVIDEO_EVENT_TX_REF_ORDER	3
#define SDIVIDEO_EVENT_TX_REF		(1 << SDIVIDEO_EVENT_TX_REF_ORDER)

/* Receiver event flag bit locations */
#define SDIVIDEO_EVENT_RX_BUFFER_ORDER	0
#define SDIVIDEO_EVENT_RX_BUFFER	(1 << SDIVIDEO_EVENT_RX_BUFFER_ORDER)
#define SDIVIDEO_EVENT_RX_FIFO_ORDER	1
#define SDIVIDEO_EVENT_RX_FIFO		(1 << SDIVIDEO_EVENT_RX_FIFO_ORDER)
#define SDIVIDEO_EVENT_RX_CARRIER_ORDER	2
#define SDIVIDEO_EVENT_RX_CARRIER	(1 << SDIVIDEO_EVENT_RX_CARRIER_ORDER)
#define SDIVIDEO_EVENT_RX_DATA_ORDER	3
#define SDIVIDEO_EVENT_RX_DATA		(1 << SDIVIDEO_EVENT_RX_DATA_ORDER)
#define SDIVIDEO_EVENT_RX_STD_ORDER	4
#define SDIVIDEO_EVENT_RX_STD		(1 << SDIVIDEO_EVENT_RX_STD_ORDER)

/* Interface capabilities */
#define SDIVIDEO_CAP_RX_CD		0x00000001
#define SDIVIDEO_CAP_RX_DATA		0x00000002
#define SDIVIDEO_CAP_RX_ERR_COUNT	0x00000004
#define SDIVIDEO_CAP_RX_VBI		0x00000008
#define SDIVIDEO_CAP_RX_RAWMODE		0x00000010
#define SDIVIDEO_CAP_RX_DEINTERLACING	0x00000020

/* Transmitter clock source settings */
#define SDIVIDEO_CTL_TX_CLKSRC_ONBOARD		0
#define SDIVIDEO_CTL_TX_CLKSRC_NTSC		1
#define SDIVIDEO_CTL_TX_CLKSRC_PAL		2
#define SDIVIDEO_CTL_TX_CLKSRC_525P		3
#define SDIVIDEO_CTL_TX_CLKSRC_625P		4
#define SDIVIDEO_CTL_TX_CLKSRC_720P_60		5
#define SDIVIDEO_CTL_TX_CLKSRC_720P_59_94	6
#define SDIVIDEO_CTL_TX_CLKSRC_720P_50		7
#define SDIVIDEO_CTL_TX_CLKSRC_720P_30		8
#define SDIVIDEO_CTL_TX_CLKSRC_720P_29_97	9
#define SDIVIDEO_CTL_TX_CLKSRC_720P_25		10
#define SDIVIDEO_CTL_TX_CLKSRC_720P_24		11
#define SDIVIDEO_CTL_TX_CLKSRC_720P_23_98	12
#define SDIVIDEO_CTL_TX_CLKSRC_1080P_60		13
#define SDIVIDEO_CTL_TX_CLKSRC_1080P_59_94	14
#define SDIVIDEO_CTL_TX_CLKSRC_1080P_50		15
#define SDIVIDEO_CTL_TX_CLKSRC_1080P_30		16
#define SDIVIDEO_CTL_TX_CLKSRC_1080P_29_97	17
#define SDIVIDEO_CTL_TX_CLKSRC_1080P_25		18
#define SDIVIDEO_CTL_TX_CLKSRC_1080P_24		19
#define SDIVIDEO_CTL_TX_CLKSRC_1080P_23_98	20
#define SDIVIDEO_CTL_TX_CLKSRC_1080I_60		21
#define SDIVIDEO_CTL_TX_CLKSRC_1080I_59_94	22
#define SDIVIDEO_CTL_TX_CLKSRC_1080I_50		23

/* Mode settings */
#define SDIVIDEO_CTL_MODE_UYVY			0
#define SDIVIDEO_CTL_MODE_V210			1
#define SDIVIDEO_CTL_MODE_V210_DEINTERLACE	2
#define SDIVIDEO_CTL_MODE_RAW			3

/* Frame mode settings */
#define SDIVIDEO_CTL_UNLOCKED				0
#define SDIVIDEO_CTL_SMPTE_125M_486I_59_94HZ		1
#define SDIVIDEO_CTL_BT_601_576I_50HZ			2
#define SDIVIDEO_CTL_SMPTE_260M_1035I_60HZ		5
#define SDIVIDEO_CTL_SMPTE_260M_1035I_59_94HZ		6
#define SDIVIDEO_CTL_SMPTE_295M_1080I_50HZ		7
#define SDIVIDEO_CTL_SMPTE_274M_1080I_60HZ		8
#define SDIVIDEO_CTL_SMPTE_274M_1080PSF_30HZ		9
#define SDIVIDEO_CTL_SMPTE_274M_1080I_59_94HZ		10
#define SDIVIDEO_CTL_SMPTE_274M_1080PSF_29_97HZ		11
#define SDIVIDEO_CTL_SMPTE_274M_1080I_50HZ		12
#define SDIVIDEO_CTL_SMPTE_274M_1080PSF_25HZ		13
#define SDIVIDEO_CTL_SMPTE_274M_1080PSF_24HZ		14
#define SDIVIDEO_CTL_SMPTE_274M_1080PSF_23_98HZ		15
#define SDIVIDEO_CTL_SMPTE_274M_1080P_30HZ		16
#define SDIVIDEO_CTL_SMPTE_274M_1080P_29_97HZ		17
#define SDIVIDEO_CTL_SMPTE_274M_1080P_25HZ		18
#define SDIVIDEO_CTL_SMPTE_274M_1080P_24HZ		19
#define SDIVIDEO_CTL_SMPTE_274M_1080P_23_98HZ		20
#define SDIVIDEO_CTL_SMPTE_296M_720P_60HZ		21
#define SDIVIDEO_CTL_SMPTE_296M_720P_59_94HZ		22
#define SDIVIDEO_CTL_SMPTE_296M_720P_50HZ		23
#define SDIVIDEO_CTL_SMPTE_296M_720P_30HZ		24
#define SDIVIDEO_CTL_SMPTE_296M_720P_29_97HZ		25
#define SDIVIDEO_CTL_SMPTE_296M_720P_25HZ		26
#define SDIVIDEO_CTL_SMPTE_296M_720P_24HZ		27
#define SDIVIDEO_CTL_SMPTE_296M_720P_23_98HZ		28

#endif

