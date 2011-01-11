/* sdi.h
 *
 * Shared header file for the Linux user-space API for
 * Linear Systems Ltd. SMPTE 259M-C interface boards.
 *
 * Copyright (C) 2004-2010 Linear Systems Ltd.
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

#ifndef _SDI_H
#define _SDI_H

/* Driver info */
#define SDI_DRIVER_NAME "sdi"

#define SDI_MAJOR 121	/* Set to 0 for dynamic allocation.
			 * Otherwise, 121 is available.
			 * See /usr/src/linux/Documentation/devices.txt */

#define SDI_TX_BUFFERS_MIN 2 /* This must be at least 2 */
/* The minimum transmit buffer size must be positive, divisible by 4,
 * and large enough that the buffers aren't transferred to the onboard FIFOs
 * too quickly for the machine to handle the interrupts.
 * This is especially a problem at startup, when the FIFOs are empty.
 * Relevant factors include onboard FIFO size, PCI bus throughput,
 * processor speed, and interrupt latency. */
#define SDI_TX_BUFSIZE_MIN 1024
#define SDI_RX_BUFFERS_MIN 2 /* This must be at least 2 */
#define SDI_RX_BUFSIZE_MIN 8 /* This must be positive and divisible by 4 */

#define SDI_TX_BUFFERS 25 /* This must be at least 2 */
#define SDI_TX_BUFSIZE 1235520 /* This must be positive and divisible by 4 */
#define SDI_RX_BUFFERS 25 /* This must be at least 2 */
#define SDI_RX_BUFSIZE 1235520 /* This must be positive and divisible by 4 */

/* Ioctl () definitions */
#define SDI_IOC_MAGIC '=' /* This ioctl magic number is currently free. See
			   * /usr/src/linux/Documentation/ioctl-number.txt */

#define SDI_IOC_TXGETCAP	_IOR(SDI_IOC_MAGIC, 1, unsigned int)
#define SDI_IOC_TXGETEVENTS	_IOR(SDI_IOC_MAGIC, 2, unsigned int)
#define SDI_IOC_TXGETBUFLEVEL	_IOR(SDI_IOC_MAGIC, 3, unsigned int)
#define SDI_IOC_TXGETTXD	_IOR(SDI_IOC_MAGIC, 4, int)

#define SDI_IOC_RXGETCAP	_IOR(SDI_IOC_MAGIC, 65, unsigned int)
#define SDI_IOC_RXGETEVENTS	_IOR(SDI_IOC_MAGIC, 66, unsigned int)
#define SDI_IOC_RXGETBUFLEVEL	_IOR(SDI_IOC_MAGIC, 67, unsigned int)
#define SDI_IOC_RXGETCARRIER	_IOR(SDI_IOC_MAGIC, 68, int)
#define SDI_IOC_RXGETSTATUS	_IOR(SDI_IOC_MAGIC, 69, int)

#define SDI_IOC_GETID		_IOR(SDI_IOC_MAGIC, 129, unsigned int)
#define SDI_IOC_GETVERSION	_IOR(SDI_IOC_MAGIC, 130, unsigned int)
/* Provide compatibility with applications compiled for older API */
#define SDI_IOC_QBUF_DEPRECATED		_IOR(SDI_IOC_MAGIC, 131, unsigned int)
#define SDI_IOC_QBUF_DEPRECATED2	_IOW(SDI_IOC_MAGIC, 131, unsigned int)
#define SDI_IOC_QBUF		_IO(SDI_IOC_MAGIC, 131)
/* Provide compatibility with applications compiled for older API */
#define SDI_IOC_DQBUF_DEPRECATED	_IOR(SDI_IOC_MAGIC, 132, unsigned int)
#define SDI_IOC_DQBUF_DEPRECATED2	_IOW(SDI_IOC_MAGIC, 132, unsigned int)
#define SDI_IOC_DQBUF		_IO(SDI_IOC_MAGIC, 132)

/* Transmitter event flag bit locations */
#define SDI_EVENT_TX_BUFFER_ORDER	0
#define SDI_EVENT_TX_BUFFER		(1 << SDI_EVENT_TX_BUFFER_ORDER)
#define SDI_EVENT_TX_FIFO_ORDER		1
#define SDI_EVENT_TX_FIFO		(1 << SDI_EVENT_TX_FIFO_ORDER)
#define SDI_EVENT_TX_DATA_ORDER		2
#define SDI_EVENT_TX_DATA		(1 << SDI_EVENT_TX_DATA_ORDER)

/* Receiver event flag bit locations */
#define SDI_EVENT_RX_BUFFER_ORDER	0
#define SDI_EVENT_RX_BUFFER		(1 << SDI_EVENT_RX_BUFFER_ORDER)
#define SDI_EVENT_RX_FIFO_ORDER		1
#define SDI_EVENT_RX_FIFO		(1 << SDI_EVENT_RX_FIFO_ORDER)
#define SDI_EVENT_RX_CARRIER_ORDER	2
#define SDI_EVENT_RX_CARRIER		(1 << SDI_EVENT_RX_CARRIER_ORDER)

/* Interface capabilities */
#define SDI_CAP_TX_RXCLKSRC	0x00000001

/* Transmitter clock source settings */
#define SDI_CTL_TX_CLKSRC_ONBOARD	0
#define SDI_CTL_TX_CLKSRC_EXT		1
#define SDI_CTL_TX_CLKSRC_RX		2

/* Mode settings */
#define SDI_CTL_MODE_8BIT	0
#define SDI_CTL_MODE_10BIT	1

#endif

