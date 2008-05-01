/****************************************************************************
 * cdrom.h: cdrom tools header
 *****************************************************************************
 * Copyright (C) 1998-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#define CDDA_TYPE 0
#define VCD_TYPE 1

/* where the data start on a VCD sector */
#define VCD_DATA_START 24
/* size of the availablr data on a VCD sector */
#define VCD_DATA_SIZE 2324
/* size of a VCD sector, header and tail included */
#define VCD_SECTOR_SIZE 2352
/* size of a CD sector */
#define CD_SECTOR_SIZE 2048
/* sector containing the entry points */
#define VCD_ENTRIES_SECTOR 151

/* where the data start on a CDDA sector */
#define CDDA_DATA_START 0
/* size of the availablr data on a CDDA sector */
#define CDDA_DATA_SIZE 2352
/* size of a CDDA sector, header and tail included */
#define CDDA_SECTOR_SIZE 2352

/*****************************************************************************
 * Misc. Macros
 *****************************************************************************/
/* LBA = msf.frame + 75 * ( msf.second + 60 * msf.minute ) */
#define MSF_TO_LBA(min, sec, frame) ((int)frame + 75 * (sec + 60 * min))
/* LBA = msf.frame + 75 * ( msf.second - 2 + 60 * msf.minute ) */
#define MSF_TO_LBA2(min, sec, frame) ((int)frame + 75 * (sec -2 + 60 * min))
/* Converts BCD to Binary data */
#define BCD_TO_BIN(i) \
    (uint8_t)((uint8_t)(0xf & (uint8_t)i)+((uint8_t)10*((uint8_t)i >> 4)))

typedef struct vcddev_s vcddev_t;

/*****************************************************************************
 * structure to store minute/second/frame locations
 *****************************************************************************/
typedef struct msf_s
{
    uint8_t minute;
    uint8_t second;
    uint8_t frame;
} msf_t;

/*****************************************************************************
 * entries_sect structure: the sector containing entry points
 *****************************************************************************/
typedef struct entries_sect_s
{
    char psz_id[8];                                 /* "ENTRYVCD" */
    uint8_t i_version;                              /* 0x02 VCD2.0
                                                       0x01 SVCD  */
    uint8_t i_sys_prof_tag;                         /* 0x01 if VCD1.1
                                                       0x00 else */
    uint16_t i_entries_nb;                          /* entries number <= 500 */

    struct
    {
        uint8_t i_track;                            /* track number */
        msf_t   msf;                                /* msf location
                                                       (in BCD format) */
    } entry[500];
    uint8_t zeros[36];                              /* should be 0x00 */
} entries_sect_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
vcddev_t *ioctl_Open         ( vlc_object_t *, const char * );
void      ioctl_Close        ( vlc_object_t *, vcddev_t * );
int       ioctl_GetTracksMap ( vlc_object_t *, const vcddev_t *, int ** );
int       ioctl_ReadSectors  ( vlc_object_t *, const vcddev_t *,
                               int, uint8_t *, int, int );
