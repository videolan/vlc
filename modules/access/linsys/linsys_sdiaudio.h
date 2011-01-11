/* sdiaudio.h
 *
 * Shared header file for the Linux user-space API for
 * Linear Systems Ltd. SMPTE 292M and SMPTE 259M-C Audio interface boards.
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

#ifndef _SDIAUDIO_H
#define _SDIAUDIO_H

/* Driver info */
#define SDIAUDIO_DRIVER_NAME "sdiaudio"

#define SDIAUDIO_MAJOR 0	/* Set to 0 for dynamic allocation.
			 * See /usr/src/linux/Documentation/devices.txt */

#define SDIAUDIO_TX_BUFFERS_MIN 2 /* This must be at least 2 */
/* The minimum transmit buffer size must be positive, divisible by 4,
 * and large enough that the buffers aren't transferred to the onboard FIFOs
 * too quickly for the machine to handle the interrupts.
 * This is especially a problem at startup, when the FIFOs are empty.
 * Relevant factors include onboard FIFO size, PCI bus throughput,
 * processor speed, and interrupt latency. */
#define SDIAUDIO_TX_BUFSIZE_MIN 1024
#define SDIAUDIO_RX_BUFFERS_MIN 2 /* This must be at least 2 */
#define SDIAUDIO_RX_BUFSIZE_MIN 8 /* This must be positive and divisible by 4 */

#define SDIAUDIO_TX_BUFFERS 30 /* This must be at least 2 */
#define SDIAUDIO_TX_BUFSIZE 6400 /* This must be positive and divisible by 4 */
#define SDIAUDIO_RX_BUFFERS 30 /* This must be at least 2 */
#define SDIAUDIO_RX_BUFSIZE 6400 /* This must be positive and divisible by 4 */

/* Ioctl () definitions */
#define SDIAUDIO_IOC_MAGIC '~' /* This ioctl magic number is currently free. See
			   * /usr/src/linux/Documentation/ioctl-number.txt */

#define SDIAUDIO_IOC_TXGETCAP			_IOR(SDIAUDIO_IOC_MAGIC, 1, unsigned int)
#define SDIAUDIO_IOC_TXGETEVENTS		_IOR(SDIAUDIO_IOC_MAGIC, 2, unsigned int)
#define SDIAUDIO_IOC_TXGETBUFLEVEL		_IOR(SDIAUDIO_IOC_MAGIC, 3, unsigned int)
#define SDIAUDIO_IOC_TXGETTXD			_IOR(SDIAUDIO_IOC_MAGIC, 4, int)

#define SDIAUDIO_IOC_RXGETCAP			_IOR(SDIAUDIO_IOC_MAGIC, 65, unsigned int)
#define SDIAUDIO_IOC_RXGETEVENTS		_IOR(SDIAUDIO_IOC_MAGIC, 66, unsigned int)
#define SDIAUDIO_IOC_RXGETBUFLEVEL		_IOR(SDIAUDIO_IOC_MAGIC, 67, unsigned int)
#define SDIAUDIO_IOC_RXGETCARRIER		_IOR(SDIAUDIO_IOC_MAGIC, 68, int)
#define SDIAUDIO_IOC_RXGETSTATUS		_IOR(SDIAUDIO_IOC_MAGIC, 69, int)
#define SDIAUDIO_IOC_RXGETAUDIOGR0ERROR		_IOR(SDIAUDIO_IOC_MAGIC, 70, unsigned int)
#define SDIAUDIO_IOC_RXGETAUDIOGR0DELAYA	_IOR(SDIAUDIO_IOC_MAGIC, 71, unsigned int)
#define SDIAUDIO_IOC_RXGETAUDIOGR0DELAYB	_IOR(SDIAUDIO_IOC_MAGIC, 72, unsigned int)
#define SDIAUDIO_IOC_RXGETNONAUDIO		_IOR(SDIAUDIO_IOC_MAGIC, 73, unsigned int)
#define SDIAUDIO_IOC_RXGETAUDSTAT		_IOR(SDIAUDIO_IOC_MAGIC, 74, unsigned int)
#define SDIAUDIO_IOC_RXGETAUDRATE		_IOR(SDIAUDIO_IOC_MAGIC, 75, unsigned int)

#define SDIAUDIO_IOC_GETID			_IOR(SDIAUDIO_IOC_MAGIC, 129, unsigned int)
#define SDIAUDIO_IOC_GETVERSION			_IOR(SDIAUDIO_IOC_MAGIC, 130, unsigned int)
/* Provide compatibility with applications compiled for older API */
#define SDIAUDIO_IOC_QBUF_DEPRECATED		_IOW(SDIAUDIO_IOC_MAGIC, 131, unsigned int)
#define SDIAUDIO_IOC_QBUF			_IO(SDIAUDIO_IOC_MAGIC, 131)
/* Provide compatibility with applications compiled for older API */
#define SDIAUDIO_IOC_DQBUF_DEPRECATED		_IOW(SDIAUDIO_IOC_MAGIC, 132, unsigned int)
#define SDIAUDIO_IOC_DQBUF			_IO(SDIAUDIO_IOC_MAGIC, 132)

/* Transmitter event flag bit locations */
#define SDIAUDIO_EVENT_TX_BUFFER_ORDER	0
#define SDIAUDIO_EVENT_TX_BUFFER	(1 << SDIAUDIO_EVENT_TX_BUFFER_ORDER)
#define SDIAUDIO_EVENT_TX_FIFO_ORDER	1
#define SDIAUDIO_EVENT_TX_FIFO		(1 << SDIAUDIO_EVENT_TX_FIFO_ORDER)
#define SDIAUDIO_EVENT_TX_DATA_ORDER	2
#define SDIAUDIO_EVENT_TX_DATA		(1 << SDIAUDIO_EVENT_TX_DATA_ORDER)

/* Receiver event flag bit locations */
#define SDIAUDIO_EVENT_RX_BUFFER_ORDER	0
#define SDIAUDIO_EVENT_RX_BUFFER	(1 << SDIAUDIO_EVENT_RX_BUFFER_ORDER)
#define SDIAUDIO_EVENT_RX_FIFO_ORDER	1
#define SDIAUDIO_EVENT_RX_FIFO		(1 << SDIAUDIO_EVENT_RX_FIFO_ORDER)
#define SDIAUDIO_EVENT_RX_CARRIER_ORDER	2
#define SDIAUDIO_EVENT_RX_CARRIER	(1 << SDIAUDIO_EVENT_RX_CARRIER_ORDER)
#define SDIAUDIO_EVENT_RX_DATA_ORDER	3
#define SDIAUDIO_EVENT_RX_DATA		(1 << SDIAUDIO_EVENT_RX_DATA_ORDER)

/* Interface capabilities */
#define SDIAUDIO_CAP_RX_CD		0x00000001
#define SDIAUDIO_CAP_RX_DATA		0x00000002
#define SDIAUDIO_CAP_RX_STATS		0x00000004
#define SDIAUDIO_CAP_RX_NONAUDIO	0x00000008
#define SDIAUDIO_CAP_RX_24BIT		0x00000010

/* Audio sample size */
#define SDIAUDIO_CTL_AUDSAMP_SZ_16	16 /* 16 bit */
#define SDIAUDIO_CTL_AUDSAMP_SZ_24	24 /* 24 bit */
#define SDIAUDIO_CTL_AUDSAMP_SZ_32	32 /* 32 bit */

/* Audio channel enable */
#define SDIAUDIO_CTL_AUDCH_EN_0		0 /* 0 channel/disable audio */
#define SDIAUDIO_CTL_AUDCH_EN_2		2 /* 2 channel */
#define SDIAUDIO_CTL_AUDCH_EN_4		4 /* 4 channel */
#define SDIAUDIO_CTL_AUDCH_EN_6		6 /* 6 channel */
#define SDIAUDIO_CTL_AUDCH_EN_8		8 /* 8 channel */

#define SDIAUDIO_CTL_PCM_ALLCHANNEL		0x00000000 /* PCM for channel 1 - 8 */
#define SDIAUDIO_CTL_NONAUDIO_ALLCHANNEL	0x000000ff /* No audio for channel 1 - 8 */

/* Active audio channels status */
#define SDIAUDIO_CTL_ACT_CHAN_0		0x00 /* no audio control packets */
#define SDIAUDIO_CTL_ACT_CHAN_2		0x03 /* 2 channels */
#define SDIAUDIO_CTL_ACT_CHAN_4		0x0f /* 4 channels */
#define SDIAUDIO_CTL_ACT_CHAN_6		0x3f /* 6 channels */
#define SDIAUDIO_CTL_ACT_CHAN_8		0xff /* 8 channels */

/* Audio rate */
#define SDIAUDIO_CTL_SYNC_48_KHZ	0 /* Synchronous, 48 kHz */
#define SDIAUDIO_CTL_SYNC_44_1_KHZ	2 /* Synchronous, 44.1 kHz */
#define SDIAUDIO_CTL_SYNC_32_KHZ	4 /* Synchronous, 32 kHz */
#define SDIAUDIO_CTL_SYNC_96_KHZ	8 /* Synchronous, 96 kHz */
#define SDIAUDIO_CTL_SYNC_FREE_RUNNING	14 /* Synchronous, free running */
#define SDIAUDIO_CTL_ASYNC_48_KHZ	1 /* Asynchronous, 48 kHz */
#define SDIAUDIO_CTL_ASYNC_44_1_KHZ	3 /* Asynchronous, 44.1 kHz */
#define SDIAUDIO_CTL_ASYNC_32_KHZ	5 /* Asynchronous, 32 kHz */
#define SDIAUDIO_CTL_ASYNC_96_KHZ	9 /* Asynchronous, 96 kHz */
#define SDIAUDIO_CTL_ASYNC_FREE_RUNNING	15 /* Asynchronous, free running */

#endif

