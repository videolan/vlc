/*****************************************************************************
 * dvd_ioctl.h: DVD ioctl replacement function
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd_ioctl.h,v 1.2 2001/02/20 23:30:15 sam Exp $
 *
 * Authors: David Giller <rafetmad@oxy.edu>
 *          Eberhard Moenkeberg <emoenke@gwdg.de>
 *          David van Leeuwen <david@tm.tno.nl>
 *          Erik Andersen <andersee@debian.org>
 *          Jens Axboe <axboe@suse.de>
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

int dvd_ioctl( int i_fd, unsigned long i_op, void *p_arg );

/*****************************************************************************
 * This is the Linux kernel's <linux/cdrom.h>, almost verbatim.
 *****************************************************************************/

/*
 * -- <linux/cdrom.h>
 * General header file for linux CD-ROM drivers 
 * Copyright (C) 1992         David Giller, rafetmad@oxy.edu
 *               1994, 1995   Eberhard Moenkeberg, emoenke@gwdg.de
 *               1996         David van Leeuwen, david@tm.tno.nl
 *               1997, 1998   Erik Andersen, andersee@debian.org
 *               1998-2000    Jens Axboe, axboe@suse.de
 */

#ifndef	_LINUX_CDROM_H
#define	_LINUX_CDROM_H

#ifdef SYS_BEOS
#   include <be/support/byteorder.h>
#else
#   include <asm/byteorder.h>
#endif

/*******************************************************
 * As of Linux 2.1.x, all Linux CD-ROM application programs will use this 
 * (and only this) include file.  It is my hope to provide Linux with
 * a uniform interface between software accessing CD-ROMs and the various 
 * device drivers that actually talk to the drives.  There may still be
 * 23 different kinds of strange CD-ROM drives, but at least there will 
 * now be one, and only one, Linux CD-ROM interface.
 *
 * Additionally, as of Linux 2.1.x, all Linux application programs 
 * should use the O_NONBLOCK option when opening a CD-ROM device 
 * for subsequent ioctl commands.  This allows for neat system errors 
 * like "No medium found" or "Wrong medium type" upon attempting to 
 * mount or play an empty slot, mount an audio disc, or play a data disc.
 * Generally, changing an application program to support O_NONBLOCK
 * is as easy as the following:
 *       -    drive = open("/dev/cdrom", O_RDONLY);
 *       +    drive = open("/dev/cdrom", O_RDONLY | O_NONBLOCK);
 * It is worth the small change.
 *
 *  Patches for many common CD programs (provided by David A. van Leeuwen)
 *  can be found at:  ftp://ftp.gwdg.de/pub/linux/cdrom/drivers/cm206/
 * 
 *******************************************************/

/* When a driver supports a certain function, but the cdrom drive we are 
 * using doesn't, we will return the error EDRIVE_CANT_DO_THIS.  We will 
 * borrow the "Operation not supported" error from the network folks to 
 * accomplish this.  Maybe someday we will get a more targeted error code, 
 * but this will do for now... */
#define EDRIVE_CANT_DO_THIS  EOPNOTSUPP

/*******************************************************
 * The CD-ROM IOCTL commands  -- these should be supported by 
 * all the various cdrom drivers.  For the CD-ROM ioctls, we 
 * will commandeer byte 0x53, or 'S'.
 *******************************************************/
#define CDROMPAUSE		0x5301 /* Pause Audio Operation */ 
#define CDROMRESUME		0x5302 /* Resume paused Audio Operation */
#define CDROMPLAYMSF		0x5303 /* Play Audio MSF (struct cdrom_msf) */
#define CDROMPLAYTRKIND		0x5304 /* Play Audio Track/index 
                                           (struct cdrom_ti) */
#define CDROMREADTOCHDR		0x5305 /* Read TOC header 
                                           (struct cdrom_tochdr) */
#define CDROMREADTOCENTRY	0x5306 /* Read TOC entry 
                                           (struct cdrom_tocentry) */
#define CDROMSTOP		0x5307 /* Stop the cdrom drive */
#define CDROMSTART		0x5308 /* Start the cdrom drive */
#define CDROMEJECT		0x5309 /* Ejects the cdrom media */
#define CDROMVOLCTRL		0x530a /* Control output volume 
                                           (struct cdrom_volctrl) */
#define CDROMSUBCHNL		0x530b /* Read subchannel data 
                                           (struct cdrom_subchnl) */
#define CDROMREADMODE2		0x530c /* Read CDROM mode 2 data (2336 Bytes) 
                                           (struct cdrom_read) */
#define CDROMREADMODE1		0x530d /* Read CDROM mode 1 data (2048 Bytes)
                                           (struct cdrom_read) */
#define CDROMREADAUDIO		0x530e /* (struct cdrom_read_audio) */
#define CDROMEJECT_SW		0x530f /* enable(1)/disable(0) auto-ejecting */
#define CDROMMULTISESSION	0x5310 /* Obtain the start-of-last-session 
                                           address of multi session disks 
                                           (struct cdrom_multisession) */
#define CDROM_GET_MCN		0x5311 /* Obtain the "Universal Product Code" 
                                           if available (struct cdrom_mcn) */
#define CDROM_GET_UPC		CDROM_GET_MCN  /* This one is depricated, 
                                          but here anyway for compatability */
#define CDROMRESET		0x5312 /* hard-reset the drive */
#define CDROMVOLREAD		0x5313 /* Get the drive's volume setting 
                                          (struct cdrom_volctrl) */
#define CDROMREADRAW		0x5314	/* read data in raw mode (2352 Bytes)
                                           (struct cdrom_read) */
/* 
 * These ioctls are used only used in aztcd.c and optcd.c
 */
#define CDROMREADCOOKED		0x5315	/* read data in cooked mode */
#define CDROMSEEK		0x5316  /* seek msf address */
  
/*
 * This ioctl is only used by the scsi-cd driver.  
   It is for playing audio in logical block addressing mode.
 */
#define CDROMPLAYBLK		0x5317	/* (struct cdrom_blk) */

/* 
 * These ioctls are only used in optcd.c
 */
#define CDROMREADALL		0x5318	/* read all 2646 bytes */

/* 
 * These ioctls are (now) only in ide-cd.c for controlling 
 * drive spindown time.  They should be implemented in the
 * Uniform driver, via generic packet commands, GPCMD_MODE_SELECT_10,
 * GPCMD_MODE_SENSE_10 and the GPMODE_POWER_PAGE...
 *  -Erik
 */
#define CDROMGETSPINDOWN        0x531d
#define CDROMSETSPINDOWN        0x531e

/* 
 * These ioctls are implemented through the uniform CD-ROM driver
 * They _will_ be adopted by all CD-ROM drivers, when all the CD-ROM
 * drivers are eventually ported to the uniform CD-ROM driver interface.
 */
#define CDROMCLOSETRAY		0x5319	/* pendant of CDROMEJECT */
#define CDROM_SET_OPTIONS	0x5320  /* Set behavior options */
#define CDROM_CLEAR_OPTIONS	0x5321  /* Clear behavior options */
#define CDROM_SELECT_SPEED	0x5322  /* Set the CD-ROM speed */
#define CDROM_SELECT_DISC	0x5323  /* Select disc (for juke-boxes) */
#define CDROM_MEDIA_CHANGED	0x5325  /* Check is media changed  */
#define CDROM_DRIVE_STATUS	0x5326  /* Get tray position, etc. */
#define CDROM_DISC_STATUS	0x5327  /* Get disc type, etc. */
#define CDROM_CHANGER_NSLOTS    0x5328  /* Get number of slots */
#define CDROM_LOCKDOOR		0x5329  /* lock or unlock door */
#define CDROM_DEBUG		0x5330	/* Turn debug messages on/off */
#define CDROM_GET_CAPABILITY	0x5331	/* get capabilities */

/* This ioctl is only used by sbpcd at the moment */
#define CDROMAUDIOBUFSIZ        0x5382	/* set the audio buffer size */

/* DVD-ROM Specific ioctls */
#define DVD_READ_STRUCT		0x5390  /* Read structure */
#define DVD_WRITE_STRUCT	0x5391  /* Write structure */
#define DVD_AUTH		0x5392  /* Authentication */

#define CDROM_SEND_PACKET	0x5393	/* send a packet to the drive */
#define CDROM_NEXT_WRITABLE	0x5394	/* get next writable block */
#define CDROM_LAST_WRITTEN	0x5395	/* get last block written on disc */

/*******************************************************
 * CDROM IOCTL structures
 *******************************************************/

/* Address in MSF format */
struct cdrom_msf0		
{
	unsigned char	minute;
	unsigned char	second;
	unsigned char	frame;
};

/* Address in either MSF or logical format */
union cdrom_addr		
{
	struct cdrom_msf0	msf;
	int			lba;
};

/* This struct is used by the CDROMPLAYMSF ioctl */ 
struct cdrom_msf 
{
	unsigned char	cdmsf_min0;	/* start minute */
	unsigned char	cdmsf_sec0;	/* start second */
	unsigned char	cdmsf_frame0;	/* start frame */
	unsigned char	cdmsf_min1;	/* end minute */
	unsigned char	cdmsf_sec1;	/* end second */
	unsigned char	cdmsf_frame1;	/* end frame */
};

/* This struct is used by the CDROMPLAYTRKIND ioctl */
struct cdrom_ti 
{
	unsigned char	cdti_trk0;	/* start track */
	unsigned char	cdti_ind0;	/* start index */
	unsigned char	cdti_trk1;	/* end track */
	unsigned char	cdti_ind1;	/* end index */
};

/* This struct is used by the CDROMREADTOCHDR ioctl */
struct cdrom_tochdr 	
{
	unsigned char	cdth_trk0;	/* start track */
	unsigned char	cdth_trk1;	/* end track */
};

/* This struct is used by the CDROMVOLCTRL and CDROMVOLREAD ioctls */
struct cdrom_volctrl
{
	unsigned char	channel0;
	unsigned char	channel1;
	unsigned char	channel2;
	unsigned char	channel3;
};

/* This struct is used by the CDROMSUBCHNL ioctl */
struct cdrom_subchnl 
{
	unsigned char	cdsc_format;
	unsigned char	cdsc_audiostatus;
	unsigned char	cdsc_adr:	4;
	unsigned char	cdsc_ctrl:	4;
	unsigned char	cdsc_trk;
	unsigned char	cdsc_ind;
	union cdrom_addr cdsc_absaddr;
	union cdrom_addr cdsc_reladdr;
};


/* This struct is used by the CDROMREADTOCENTRY ioctl */
struct cdrom_tocentry 
{
	unsigned char	cdte_track;
	unsigned char	cdte_adr	:4;
	unsigned char	cdte_ctrl	:4;
	unsigned char	cdte_format;
	union cdrom_addr cdte_addr;
	unsigned char	cdte_datamode;
};

/* This struct is used by the CDROMREADMODE1, and CDROMREADMODE2 ioctls */
struct cdrom_read      
{
	int	cdread_lba;
	char 	*cdread_bufaddr;
	int	cdread_buflen;
};

/* This struct is used by the CDROMREADAUDIO ioctl */
struct cdrom_read_audio
{
	union cdrom_addr addr; /* frame address */
	unsigned char addr_format;    /* CDROM_LBA or CDROM_MSF */
	int nframes;           /* number of 2352-byte-frames to read at once */
	unsigned char *buf;           /* frame buffer (size: nframes*2352 bytes) */
};

/* This struct is used with the CDROMMULTISESSION ioctl */
struct cdrom_multisession
{
	union cdrom_addr addr; /* frame address: start-of-last-session 
	                           (not the new "frame 16"!).  Only valid
	                           if the "xa_flag" is true. */
	unsigned char xa_flag;        /* 1: "is XA disk" */
	unsigned char addr_format;    /* CDROM_LBA or CDROM_MSF */
};

/* This struct is used with the CDROM_GET_MCN ioctl.  
 * Very few audio discs actually have Universal Product Code information, 
 * which should just be the Medium Catalog Number on the box.  Also note 
 * that the way the codeis written on CD is _not_ uniform across all discs!
 */  
struct cdrom_mcn 
{
  unsigned char medium_catalog_number[14]; /* 13 ASCII digits, null-terminated */
};

/* This is used by the CDROMPLAYBLK ioctl */
struct cdrom_blk 
{
	unsigned from;
	unsigned short len;
};

#define CDROM_PACKET_SIZE	12

#define CGC_DATA_UNKNOWN	0
#define CGC_DATA_WRITE		1
#define CGC_DATA_READ		2
#define CGC_DATA_NONE		3

/* for CDROM_PACKET_COMMAND ioctl */
struct cdrom_generic_command
{
	unsigned char 		cmd[CDROM_PACKET_SIZE];
	unsigned char 		*buffer;
	unsigned int 		buflen;
	int			stat;
	struct request_sense	*sense;
	unsigned char		data_direction;
	int			quiet;
	int			timeout;
	void			*reserved[1];
};


/*
 * A CD-ROM physical sector size is 2048, 2052, 2056, 2324, 2332, 2336, 
 * 2340, or 2352 bytes long.  

*         Sector types of the standard CD-ROM data formats:
 *
 * format   sector type               user data size (bytes)
 * -----------------------------------------------------------------------------
 *   1     (Red Book)    CD-DA          2352    (CD_FRAMESIZE_RAW)
 *   2     (Yellow Book) Mode1 Form1    2048    (CD_FRAMESIZE)
 *   3     (Yellow Book) Mode1 Form2    2336    (CD_FRAMESIZE_RAW0)
 *   4     (Green Book)  Mode2 Form1    2048    (CD_FRAMESIZE)
 *   5     (Green Book)  Mode2 Form2    2328    (2324+4 spare bytes)
 *
 *
 *       The layout of the standard CD-ROM data formats:
 * -----------------------------------------------------------------------------
 * - audio (red):                  | audio_sample_bytes |
 *                                 |        2352        |
 *
 * - data (yellow, mode1):         | sync - head - data - EDC - zero - ECC |
 *                                 |  12  -   4  - 2048 -  4  -   8  - 276 |
 *
 * - data (yellow, mode2):         | sync - head - data |
 *                                 |  12  -   4  - 2336 |
 *
 * - XA data (green, mode2 form1): | sync - head - sub - data - EDC - ECC |
 *                                 |  12  -   4  -  8  - 2048 -  4  - 276 |
 *
 * - XA data (green, mode2 form2): | sync - head - sub - data - Spare |
 *                                 |  12  -   4  -  8  - 2324 -  4    |
 *
 */

/* Some generally useful CD-ROM information -- mostly based on the above */
#define CD_MINS              74 /* max. minutes per CD, not really a limit */
#define CD_SECS              60 /* seconds per minute */
#define CD_FRAMES            75 /* frames per second */
#define CD_SYNC_SIZE         12 /* 12 sync bytes per raw data frame */
#define CD_MSF_OFFSET       150 /* MSF numbering offset of first frame */
#define CD_CHUNK_SIZE        24 /* lowest-level "data bytes piece" */
#define CD_NUM_OF_CHUNKS     98 /* chunks per frame */
#define CD_FRAMESIZE_SUB     96 /* subchannel data "frame" size */
#define CD_HEAD_SIZE          4 /* header (address) bytes per raw data frame */
#define CD_SUBHEAD_SIZE       8 /* subheader bytes per raw XA data frame */
#define CD_EDC_SIZE           4 /* bytes EDC per most raw data frame types */
#define CD_ZERO_SIZE          8 /* bytes zero per yellow book mode 1 frame */
#define CD_ECC_SIZE         276 /* bytes ECC per most raw data frame types */
#define CD_FRAMESIZE       2048 /* bytes per frame, "cooked" mode */
#define CD_FRAMESIZE_RAW   2352 /* bytes per frame, "raw" mode */
#define CD_FRAMESIZE_RAWER 2646 /* The maximum possible returned bytes */ 
/* most drives don't deliver everything: */
#define CD_FRAMESIZE_RAW1 (CD_FRAMESIZE_RAW-CD_SYNC_SIZE) /*2340*/
#define CD_FRAMESIZE_RAW0 (CD_FRAMESIZE_RAW-CD_SYNC_SIZE-CD_HEAD_SIZE) /*2336*/

#define CD_XA_HEAD        (CD_HEAD_SIZE+CD_SUBHEAD_SIZE) /* "before data" part of raw XA frame */
#define CD_XA_TAIL        (CD_EDC_SIZE+CD_ECC_SIZE) /* "after data" part of raw XA frame */
#define CD_XA_SYNC_HEAD   (CD_SYNC_SIZE+CD_XA_HEAD) /* sync bytes + header of XA frame */

/* CD-ROM address types (cdrom_tocentry.cdte_format) */
#define	CDROM_LBA 0x01 /* "logical block": first frame is #0 */
#define	CDROM_MSF 0x02 /* "minute-second-frame": binary, not bcd here! */

/* bit to tell whether track is data or audio (cdrom_tocentry.cdte_ctrl) */
#define	CDROM_DATA_TRACK	0x04

/* The leadout track is always 0xAA, regardless of # of tracks on disc */
#define	CDROM_LEADOUT		0xAA

/* audio states (from SCSI-2, but seen with other drives, too) */
#define	CDROM_AUDIO_INVALID	0x00	/* audio status not supported */
#define	CDROM_AUDIO_PLAY	0x11	/* audio play operation in progress */
#define	CDROM_AUDIO_PAUSED	0x12	/* audio play operation paused */
#define	CDROM_AUDIO_COMPLETED	0x13	/* audio play successfully completed */
#define	CDROM_AUDIO_ERROR	0x14	/* audio play stopped due to error */
#define	CDROM_AUDIO_NO_STATUS	0x15	/* no current audio status to return */

/* capability flags used with the uniform CD-ROM driver */ 
#define CDC_CLOSE_TRAY		0x1     /* caddy systems _can't_ close */
#define CDC_OPEN_TRAY		0x2     /* but _can_ eject.  */
#define CDC_LOCK		0x4     /* disable manual eject */
#define CDC_SELECT_SPEED 	0x8     /* programmable speed */
#define CDC_SELECT_DISC		0x10    /* select disc from juke-box */
#define CDC_MULTI_SESSION 	0x20    /* read sessions>1 */
#define CDC_MCN			0x40    /* Medium Catalog Number */
#define CDC_MEDIA_CHANGED 	0x80    /* media changed */
#define CDC_PLAY_AUDIO		0x100   /* audio functions */
#define CDC_RESET               0x200   /* hard reset device */
#define CDC_IOCTLS              0x400   /* driver has non-standard ioctls */
#define CDC_DRIVE_STATUS        0x800   /* driver implements drive status */
#define CDC_GENERIC_PACKET	0x1000	/* driver implements generic packets */
#define CDC_CD_R		0x2000	/* drive is a CD-R */
#define CDC_CD_RW		0x4000	/* drive is a CD-RW */
#define CDC_DVD			0x8000	/* drive is a DVD */
#define CDC_DVD_R		0x10000	/* drive can write DVD-R */
#define CDC_DVD_RAM		0x20000	/* drive can write DVD-RAM */

/* drive status possibilities returned by CDROM_DRIVE_STATUS ioctl */
#define CDS_NO_INFO		0	/* if not implemented */
#define CDS_NO_DISC		1
#define CDS_TRAY_OPEN		2
#define CDS_DRIVE_NOT_READY	3
#define CDS_DISC_OK		4

/* return values for the CDROM_DISC_STATUS ioctl */
/* can also return CDS_NO_[INFO|DISC], from above */
#define CDS_AUDIO		100
#define CDS_DATA_1		101
#define CDS_DATA_2		102
#define CDS_XA_2_1		103
#define CDS_XA_2_2		104
#define CDS_MIXED		105

/* User-configurable behavior options for the uniform CD-ROM driver */
#define CDO_AUTO_CLOSE		0x1     /* close tray on first open() */
#define CDO_AUTO_EJECT		0x2     /* open tray on last release() */
#define CDO_USE_FFLAGS		0x4     /* use O_NONBLOCK information on open */
#define CDO_LOCK		0x8     /* lock tray on open files */
#define CDO_CHECK_TYPE		0x10    /* check type on open for data */

/* Special codes used when specifying changer slots. */
#define CDSL_NONE       	((int) (~0U>>1)-1)
#define CDSL_CURRENT    	((int) (~0U>>1))

/* For partition based multisession access. IDE can handle 64 partitions
 * per drive - SCSI CD-ROM's use minors to differentiate between the
 * various drives, so we can't do multisessions the same way there.
 * Use the -o session=x option to mount on them.
 */
#define CD_PART_MAX		64
#define CD_PART_MASK		(CD_PART_MAX - 1)

/*********************************************************************
 * Generic Packet commands, MMC commands, and such
 *********************************************************************/

 /* The generic packet command opcodes for CD/DVD Logical Units,
 * From Table 57 of the SFF8090 Ver. 3 (Mt. Fuji) draft standard. */
#define GPCMD_BLANK			    0xa1
#define GPCMD_CLOSE_TRACK		    0x5b
#define GPCMD_FLUSH_CACHE		    0x35
#define GPCMD_FORMAT_UNIT		    0x04
#define GPCMD_GET_CONFIGURATION		    0x46
#define GPCMD_GET_EVENT_STATUS_NOTIFICATION 0x4a
#define GPCMD_GET_PERFORMANCE		    0xac
#define GPCMD_INQUIRY			    0x12
#define GPCMD_LOAD_UNLOAD		    0xa6
#define GPCMD_MECHANISM_STATUS		    0xbd
#define GPCMD_MODE_SELECT_10		    0x55
#define GPCMD_MODE_SENSE_10		    0x5a
#define GPCMD_PAUSE_RESUME		    0x4b
#define GPCMD_PLAY_AUDIO_10		    0x45
#define GPCMD_PLAY_AUDIO_MSF		    0x47
#define GPCMD_PLAY_AUDIO_TI		    0x48
#define GPCMD_PLAY_CD			    0xbc
#define GPCMD_PREVENT_ALLOW_MEDIUM_REMOVAL  0x1e
#define GPCMD_READ_10			    0x28
#define GPCMD_READ_12			    0xa8
#define GPCMD_READ_CDVD_CAPACITY	    0x25
#define GPCMD_READ_CD			    0xbe
#define GPCMD_READ_CD_MSF		    0xb9
#define GPCMD_READ_DISC_INFO		    0x51
#define GPCMD_READ_DVD_STRUCTURE	    0xad
#define GPCMD_READ_FORMAT_CAPACITIES	    0x23
#define GPCMD_READ_HEADER		    0x44
#define GPCMD_READ_TRACK_RZONE_INFO	    0x52
#define GPCMD_READ_SUBCHANNEL		    0x42
#define GPCMD_READ_TOC_PMA_ATIP		    0x43
#define GPCMD_REPAIR_RZONE_TRACK	    0x58
#define GPCMD_REPORT_KEY		    0xa4
#define GPCMD_REQUEST_SENSE		    0x03
#define GPCMD_RESERVE_RZONE_TRACK	    0x53
#define GPCMD_SCAN			    0xba
#define GPCMD_SEEK			    0x2b
#define GPCMD_SEND_DVD_STRUCTURE	    0xad
#define GPCMD_SEND_EVENT		    0xa2
#define GPCMD_SEND_KEY			    0xa3
#define GPCMD_SEND_OPC			    0x54
#define GPCMD_SET_READ_AHEAD		    0xa7
#define GPCMD_SET_STREAMING		    0xb6
#define GPCMD_START_STOP_UNIT		    0x1b
#define GPCMD_STOP_PLAY_SCAN		    0x4e
#define GPCMD_TEST_UNIT_READY		    0x00
#define GPCMD_VERIFY_10			    0x2f
#define GPCMD_WRITE_10			    0x2a
#define GPCMD_WRITE_AND_VERIFY_10	    0x2e
/* This is listed as optional in ATAPI 2.6, but is (curiously) 
 * missing from Mt. Fuji, Table 57.  It _is_ mentioned in Mt. Fuji
 * Table 377 as an MMC command for SCSi devices though...  Most ATAPI
 * drives support it. */
#define GPCMD_SET_SPEED			    0xbb
/* This seems to be a SCSI specific CD-ROM opcode 
 * to play data at track/index */
#define GPCMD_PLAYAUDIO_TI		    0x48
/*
 * From MS Media Status Notification Support Specification. For
 * older drives only.
 */
#define GPCMD_GET_MEDIA_STATUS		    0xda

/* Mode page codes for mode sense/set */
#define GPMODE_R_W_ERROR_PAGE		0x01
#define GPMODE_WRITE_PARMS_PAGE		0x05
#define GPMODE_AUDIO_CTL_PAGE		0x0e
#define GPMODE_POWER_PAGE		0x1a
#define GPMODE_FAULT_FAIL_PAGE		0x1c
#define GPMODE_TO_PROTECT_PAGE		0x1d
#define GPMODE_CAPABILITIES_PAGE	0x2a
#define GPMODE_ALL_PAGES		0x3f
/* Not in Mt. Fuji, but in ATAPI 2.6 -- depricated now in favor
 * of MODE_SENSE_POWER_PAGE */
#define GPMODE_CDROM_PAGE		0x0d



/* DVD struct types */
#define DVD_STRUCT_PHYSICAL	0x00
#define DVD_STRUCT_COPYRIGHT	0x01
#define DVD_STRUCT_DISCKEY	0x02
#define DVD_STRUCT_BCA		0x03
#define DVD_STRUCT_MANUFACT	0x04

struct dvd_layer {
	unsigned char book_version	: 4;
	unsigned char book_type		: 4;
	unsigned char min_rate		: 4;
	unsigned char disc_size		: 4;
	unsigned char layer_type		: 4;
	unsigned char track_path		: 1;
	unsigned char nlayers		: 2;
	unsigned char track_density	: 4;
	unsigned char linear_density	: 4;
	unsigned char bca		: 1;
	u32 start_sector;
	u32 end_sector;
	u32 end_sector_l0;
};

struct dvd_physical {
	unsigned char type;
	unsigned char layer_num;
	struct dvd_layer layer[4];
};

struct dvd_copyright {
	unsigned char type;

	unsigned char layer_num;
	unsigned char cpst;
	unsigned char rmi;
};

struct dvd_disckey {
	unsigned char type;

	unsigned agid		: 2;
	unsigned char value[2048];
};

struct dvd_bca {
	unsigned char type;

	int len;
	unsigned char value[188];
};

struct dvd_manufact {
	unsigned char type;

	unsigned char layer_num;
	int len;
	unsigned char value[2048];
};

typedef union {
	unsigned char type;

	struct dvd_physical	physical;
	struct dvd_copyright	copyright;
	struct dvd_disckey	disckey;
	struct dvd_bca		bca;
	struct dvd_manufact	manufact;
} dvd_struct;

/*
 * DVD authentication ioctl
 */

/* Authentication states */
#define DVD_LU_SEND_AGID	0
#define DVD_HOST_SEND_CHALLENGE	1
#define DVD_LU_SEND_KEY1	2
#define DVD_LU_SEND_CHALLENGE	3
#define DVD_HOST_SEND_KEY2	4

/* Termination states */
#define DVD_AUTH_ESTABLISHED	5
#define DVD_AUTH_FAILURE	6

/* Other functions */
#define DVD_LU_SEND_TITLE_KEY	7
#define DVD_LU_SEND_ASF		8
#define DVD_INVALIDATE_AGID	9
#define DVD_LU_SEND_RPC_STATE	10
#define DVD_HOST_SEND_RPC_STATE	11

/* State data */
typedef unsigned char dvd_key[5];		/* 40-bit value, MSB is first elem. */
typedef unsigned char dvd_challenge[10];	/* 80-bit value, MSB is first elem. */

struct dvd_lu_send_agid {
	unsigned char type;
	unsigned agid		: 2;
};

struct dvd_host_send_challenge {
	unsigned char type;
	unsigned agid		: 2;

	dvd_challenge chal;
};

struct dvd_send_key {
	unsigned char type;
	unsigned agid		: 2;

	dvd_key key;
};

struct dvd_lu_send_challenge {
	unsigned char type;
	unsigned agid		: 2;

	dvd_challenge chal;
};

#define DVD_CPM_NO_COPYRIGHT	0
#define DVD_CPM_COPYRIGHTED	1

#define DVD_CP_SEC_NONE		0
#define DVD_CP_SEC_EXIST	1

#define DVD_CGMS_UNRESTRICTED	0
#define DVD_CGMS_SINGLE		2
#define DVD_CGMS_RESTRICTED	3

struct dvd_lu_send_title_key {
	unsigned char type;
	unsigned agid		: 2;

	dvd_key title_key;
	int lba;
	unsigned cpm		: 1;
	unsigned cp_sec		: 1;
	unsigned cgms		: 2;
};

struct dvd_lu_send_asf {
	unsigned char type;
	unsigned agid		: 2;

	unsigned asf		: 1;
};

struct dvd_host_send_rpcstate {
	unsigned char type;
	unsigned char pdrc;
};

struct dvd_lu_send_rpcstate {
	unsigned char type		: 2;
	unsigned char vra		: 3;
	unsigned char ucca		: 3;
	unsigned char region_mask;
	unsigned char rpc_scheme;
};

typedef union {
	unsigned char type;

	struct dvd_lu_send_agid		lsa;
	struct dvd_host_send_challenge	hsc;
	struct dvd_send_key		lsk;
	struct dvd_lu_send_challenge	lsc;
	struct dvd_send_key		hsk;
	struct dvd_lu_send_title_key	lstk;
	struct dvd_lu_send_asf		lsasf;
	struct dvd_host_send_rpcstate	hrpcs;
	struct dvd_lu_send_rpcstate	lrpcs;
} dvd_authinfo;

struct request_sense {
#if defined(__BIG_ENDIAN_BITFIELD)
	unsigned char valid		: 1;
	unsigned char error_code		: 7;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned char error_code		: 7;
	unsigned char valid		: 1;
#endif
	unsigned char segment_number;
#if defined(__BIG_ENDIAN_BITFIELD)
	unsigned char reserved1		: 2;
	unsigned char ili		: 1;
	unsigned char reserved2		: 1;
	unsigned char sense_key		: 4;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned char sense_key		: 4;
	unsigned char reserved2		: 1;
	unsigned char ili		: 1;
	unsigned char reserved1		: 2;
#endif
	unsigned char information[4];
	unsigned char add_sense_len;
	unsigned char command_info[4];
	unsigned char asc;
	unsigned char ascq;
	unsigned char fruc;
	unsigned char sks[3];
	unsigned char asb[46];
};

typedef struct {
	u16 report_key_length;
	unsigned char reserved1;
	unsigned char reserved2;
#if defined(__BIG_ENDIAN_BITFIELD)
	unsigned char type_code			: 2;
	unsigned char vra			: 3;
	unsigned char ucca			: 3;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned char ucca			: 3;
	unsigned char vra			: 3;
	unsigned char type_code			: 2;
#endif
	unsigned char region_mask;
	unsigned char rpc_scheme;
	unsigned char reserved3;
} rpc_state_t;

#endif  /* _LINUX_CDROM_H */

