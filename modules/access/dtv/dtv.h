/**
 * @file dtv.h
 * @brief Digital TV module common header
 */
/*****************************************************************************
 * Copyright © 2011 Rémi Denis-Courmont
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

#ifndef VLC_DTV_H
# define VLC_DTV_H 1
# ifdef __cplusplus
extern "C" {
# endif

typedef enum {
    DTV_DELIVERY_NONE   = 0x00000000,
    DTV_DELIVERY_ATSC   = 0x00000001,
    DTV_DELIVERY_CQAM   = 0x00000002,

    DTV_DELIVERY_DVB_C  = 0x00000010,
    DTV_DELIVERY_DVB_C2 = 0x00000020,
    DTV_DELIVERY_DVB_S  = 0x00000040,
    DTV_DELIVERY_DVB_S2 = 0x00000080,
    DTV_DELIVERY_DVB_T  = 0x00000100,
    DTV_DELIVERY_DVB_T2 = 0x00000200,

    DTV_DELIVERY_ISDB_C = 0x00001000,
    DTV_DELIVERY_ISDB_S = 0x00002000,
    DTV_DELIVERY_ISDB_T = 0x00004000,
} dtv_delivery_t;

#define DTV_DELGROUP_G2         ( DTV_DELIVERY_DVB_C2 | DTV_DELIVERY_DVB_T2 | \
                                  DTV_DELIVERY_DVB_S2 )

#define DTV_DELGROUP_SAT        ( DTV_DELIVERY_DVB_S | DTV_DELIVERY_DVB_S2 | \
                                  DTV_DELIVERY_ISDB_S )

#define DTV_DELGROUP_CABLE      ( DTV_DELIVERY_DVB_C | DTV_DELIVERY_DVB_C2 | \
                                  DTV_DELIVERY_CQAM  | DTV_DELIVERY_ISDB_C )

#define DTV_DELGROUP_TERRES     ( DTV_DELIVERY_DVB_T | DTV_DELIVERY_DVB_T2 | \
                                  DTV_DELIVERY_ATSC  | DTV_DELIVERY_ISDB_T )

typedef struct dvb_device dvb_device_t;

typedef int (* tuner_setup_t) (vlc_object_t *, dvb_device_t *, uint64_t freq);
tuner_setup_t dtv_get_delivery_tuner_setup( dtv_delivery_t );

dvb_device_t *dvb_open (vlc_object_t *obj);
void dvb_close (dvb_device_t *);
ssize_t dvb_read (dvb_device_t *, void *, size_t, int);

int dvb_add_pid (dvb_device_t *, uint16_t);
void dvb_remove_pid (dvb_device_t *, uint16_t);
bool dvb_get_pid_state (const dvb_device_t *, uint16_t);

unsigned dvb_enum_systems (dvb_device_t *);
float dvb_get_signal_strength (dvb_device_t *);
float dvb_get_snr (dvb_device_t *);

typedef struct en50221_capmt_info_s en50221_capmt_info_t;
bool dvb_set_ca_pmt (dvb_device_t *, en50221_capmt_info_t *);

int dvb_set_inversion (dvb_device_t *, int);
int dvb_tune (dvb_device_t *);

typedef struct
{
    struct
    {
        unsigned min;
        unsigned max;
        unsigned step;
    } frequency;
    struct
    {
        unsigned min;
        unsigned max;
        unsigned step;
    } symbolrate;
    bool b_can_cam_auto;
} dvb_device_caps_t;

int dvb_fill_device_caps( dvb_device_t *, dvb_device_caps_t * );

#define VLC_FEC(a,b)   (((a) << 16u) | (b))
#define VLC_FEC_AUTO   0xFFFFFFFF
#define VLC_GUARD(a,b) (((a) << 16u) | (b))
#define VLC_GUARD_AUTO 0xFFFFFFFF

/* DVB-C */
int dvb_set_dvbc (dvb_device_t *, uint32_t freq, const char *mod,
                  uint32_t srate, uint32_t fec);

/* DVB-S */
int dvb_set_dvbs (dvb_device_t *, uint64_t freq, uint32_t srate, uint32_t fec);
int dvb_set_dvbs2 (dvb_device_t *, uint64_t freq, const char *mod,
                   uint32_t srate, uint32_t fec, int pilot, int rolloff,
                   uint8_t sid);
int dvb_set_sec (dvb_device_t *, uint64_t freq, char pol,
                 uint32_t lowf, uint32_t highf, uint32_t switchf);

/* DVB-T */
int dvb_set_dvbt (dvb_device_t *, uint32_t freq, const char *mod,
                  uint32_t fec_hp, uint32_t fec_lp, uint32_t bandwidth,
                  int transmission, uint32_t guard, int hierarchy);
int dvb_set_dvbt2 (dvb_device_t *, uint32_t freq, const char *mod,
                   uint32_t fec, uint32_t bandwidth,
                   int transmission, uint32_t guard, uint8_t plp);

/* ATSC */
int dvb_set_atsc (dvb_device_t *, uint32_t freq, const char *mod);
int dvb_set_cqam (dvb_device_t *, uint32_t freq, const char *mod);

/* ISDB-C */
int dvb_set_isdbc (dvb_device_t *, uint32_t freq, const char *mod,
                   uint32_t srate, uint32_t fec);

/* ISDB-S */
/* TODO: modulation? */
int dvb_set_isdbs (dvb_device_t *, uint64_t freq, uint16_t ts_id);

/* ISDB-T */
typedef struct isdbt_layer
{
    const char *modulation;
    uint32_t code_rate;
    uint8_t segment_count;
    uint8_t time_interleaving;
} isdbt_layer_t;

int dvb_set_isdbt (dvb_device_t *, uint32_t freq, uint32_t bandwidth,
                   int transmission, uint32_t guard, const isdbt_layer_t[3]);

typedef struct isdbt_sound
{
    uint8_t subchannel_id;
    uint8_t segment_index;
    uint8_t segment_count;
} isdbt_sound_t;

# ifdef __cplusplus
}
# endif
#endif
