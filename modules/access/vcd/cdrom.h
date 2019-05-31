/****************************************************************************
 * cdrom.h: cdrom tools header
 *****************************************************************************
 * Copyright (C) 1998-2001 VLC authors and VideoLAN
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_CDROM_H
#define VLC_CDROM_H

enum {
    CDDA_TYPE = 0,
    VCD_TYPE  = 1,
};

/* size of a CD sector */
#define CD_RAW_SECTOR_SIZE  2352
#define CD_ROM_MODE1_DATA_SIZE 2048
#define CD_ROM_MODE2_DATA_SIZE 2336

#define CD_ROM_XA_MODE2_F1_DATA_SIZE 2048
#define CD_ROM_XA_MODE2_F2_DATA_SIZE 2324

#define CD_ROM_XA_FRAMES   75
#define CD_ROM_XA_INTERVAL ((60 + 90 + 2) * CD_ROM_XA_FRAMES)

/* Subcode control flag */
#define CD_ROM_DATA_FLAG    0x04

/* size of a CD sector */
#define CD_SECTOR_SIZE      CD_ROM_MODE1_DATA_SIZE

/* where the data start on a VCD sector */
#define VCD_DATA_START      24
/* size of the available data on a VCD sector */
#define VCD_DATA_SIZE       CD_ROM_XA_MODE2_F2_DATA_SIZE
/* size of a VCD sector, header and tail included */
#define VCD_SECTOR_SIZE     CD_RAW_SECTOR_SIZE
/* sector containing the entry points */
#define VCD_ENTRIES_SECTOR  151

/* where the data start on a CDDA sector */
#define CDDA_DATA_START     0
/* size of the available data on a CDDA sector */
#define CDDA_DATA_SIZE      CD_RAW_SECTOR_SIZE
/* size of a CDDA sector, header and tail included */
#define CDDA_SECTOR_SIZE    CD_RAW_SECTOR_SIZE

/*****************************************************************************
 * Misc. Macros
 *****************************************************************************/
static inline int MSF_TO_LBA(uint8_t min, uint8_t sec, uint8_t frame)
{
    return (int)(frame + 75 * (sec + 60 * min));
}
static inline int MSF_TO_LBA2(uint8_t min, uint8_t sec, uint8_t frame)
{
    return (int)(frame + 75 * (sec -2 + 60 * min));
}

/* Converts BCD to Binary data */
#define BCD_TO_BIN(i) \
    (uint8_t)((uint8_t)(0xf & (uint8_t)i)+((uint8_t)10*((uint8_t)i >> 4)))

typedef struct vcddev_s vcddev_t;
typedef struct
{
    int i_lba;
    int i_control;
} vcddev_sector_t;

typedef struct
{
    int i_tracks;
    vcddev_sector_t *p_sectors;
    int i_first_track;
    int i_last_track;
} vcddev_toc_t;

static inline vcddev_toc_t * vcddev_toc_New( void )
{
    return calloc(1, sizeof(vcddev_toc_t));
}

static inline void vcddev_toc_Reset( vcddev_toc_t *toc )
{
    free(toc->p_sectors);
    memset(toc, 0, sizeof(*toc));
}

static inline void vcddev_toc_Free( vcddev_toc_t *toc )
{
    free(toc->p_sectors);
    free(toc);
}

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
vcddev_toc_t * ioctl_GetTOC  ( vlc_object_t *, const vcddev_t *, bool );
int       ioctl_ReadSectors  ( vlc_object_t *, const vcddev_t *,
                               int, uint8_t *, int, int );

/* CDDA only
 * The track 0 is for album meta data */
int       ioctl_GetCdText( vlc_object_t *, const vcddev_t *,
                           vlc_meta_t ***ppp_tracks, int *pi_tracks );

#endif /* VLC_CDROM_H */
