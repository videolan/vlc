/*****************************************************************************
 * scan.h : functions to ease DVB scanning
 *****************************************************************************
 * Copyright (C) 2008,2010 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
 *          David Kaplan <david@2of1.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA    02111, USA.
 *****************************************************************************/

typedef enum
{
    SCAN_NONE = 0,
    SCAN_DVB_T,
    SCAN_DVB_S,
    SCAN_DVB_C,
} scan_type_t;

typedef enum
{
    SCAN_DELIVERY_UNKNOWN = 0,
    SCAN_DELIVERY_DVB_T,
    SCAN_DELIVERY_DVB_T2,
    SCAN_DELIVERY_DVB_S,
    SCAN_DELIVERY_DVB_S2,
    SCAN_DELIVERY_DVB_C,
    SCAN_DELIVERY_ISDB_T,
} scan_delivery_t;

typedef enum
{
    SCAN_MODULATION_AUTO       = 0x00,
    SCAN_MODULATION_QAM_16     = 0x01,
    SCAN_MODULATION_QAM_32     = 0x02,
    SCAN_MODULATION_QAM_64     = 0x03,
    SCAN_MODULATION_QAM_128    = 0x04,
    SCAN_MODULATION_QAM_256    = 0x05,
    SCAN_MODULATION_QAM_4NR,
    SCAN_MODULATION_QAM_AUTO,
    SCAN_MODULATION_PSK_8,
    SCAN_MODULATION_QPSK,
    SCAN_MODULATION_DQPSK,
    SCAN_MODULATION_APSK_16,
    SCAN_MODULATION_APSK_32,
    SCAN_MODULATION_VSB_8,
    SCAN_MODULATION_VSB_16,
} scan_modulation_t;

#define make_tuple(a,b) ((a << 16)|b)
typedef enum
{
    SCAN_CODERATE_AUTO = -1,
    SCAN_CODERATE_NONE = 0,
    SCAN_CODERATE_1_2  = make_tuple(1,2),
    SCAN_CODERATE_2_3  = make_tuple(2,3),
    SCAN_CODERATE_3_4  = make_tuple(3,4),
    SCAN_CODERATE_3_5  = make_tuple(3,5),
    SCAN_CODERATE_4_5  = make_tuple(4,5),
    SCAN_CODERATE_5_6  = make_tuple(5,6),
    SCAN_CODERATE_7_8  = make_tuple(7,8),
    SCAN_CODERATE_8_9  = make_tuple(8,9),
    SCAN_CODERATE_9_10 = make_tuple(9,10),
} scan_coderate_t;

typedef enum
{
    SCAN_POLARIZATION_NONE       = 0,
    SCAN_POLARIZATION_HORIZONTAL = 'H',
    SCAN_POLARIZATION_CIRC_LEFT  = 'L',
    SCAN_POLARIZATION_CIRC_RIGHT = 'R',
    SCAN_POLARIZATION_VERTICAL   = 'V',
} scan_polarization_t;

typedef enum
{
    SCAN_GUARD_INTERVAL_AUTO   = 0,
    SCAN_GUARD_INTERVAL_1_4    = make_tuple(1,4),
    SCAN_GUARD_INTERVAL_1_8    = make_tuple(1,8),
    SCAN_GUARD_INTERVAL_1_16   = make_tuple(1,16),
    SCAN_GUARD_INTERVAL_1_32   = make_tuple(1,32),
    SCAN_GUARD_INTERVAL_1_128  = make_tuple(1,128),
    SCAN_GUARD_INTERVAL_19_128 = make_tuple(19,128),
    SCAN_GUARD_INTERVAL_19_256 = make_tuple(19,256),
} scan_guard_t;

typedef struct
{
    unsigned i_frequency;
    union
    {
        unsigned i_bandwidth;
        unsigned i_symbolrate;
    };

    scan_modulation_t modulation;
    scan_coderate_t coderate_lp;
    scan_coderate_t coderate_hp;
    scan_coderate_t inner_fec;
    scan_polarization_t polarization;

    scan_type_t type;
    scan_delivery_t delivery;

} scan_tuner_config_t;

typedef struct scan_parameter_t
{
    scan_type_t type;
    bool b_exhaustive;
    bool b_use_nit;
    bool b_free_only;

    bool b_modulation_set;
    unsigned i_symbolrate;

    struct
    {
        unsigned i_min;
        unsigned i_max;
        unsigned i_step;
    } frequency;

    struct
    {
        unsigned i_min;
        unsigned i_max;
    } bandwidth;/* Bandwidth should be 6, 7 or 8 */

    char *psz_scanlist_file;
    enum
    {
        FORMAT_DVBv3,
        FORMAT_DVBv5,
    } scanlist_format;

} scan_parameter_t;

#define SCAN_READ_BUFFER_COUNT 20

typedef struct scan_t scan_t;
typedef int (*scan_frontend_tune_cb)( scan_t *, void *, const scan_tuner_config_t * );
typedef int (*scan_frontend_stats_cb)( scan_t *, void *, int * );
typedef int (*scan_demux_filter_cb)( scan_t *, void *, uint16_t, bool );
typedef int (*scan_demux_read_cb)( scan_t *, void *, unsigned, size_t, uint8_t *, size_t * );

typedef struct scan_service_t scan_service_t;
typedef const void * (*scan_service_notify_cb)( scan_t *, void *, const scan_service_t *, const void *, bool );
void scan_set_NotifyCB( scan_t *, scan_service_notify_cb );

const char * scan_service_GetName( const scan_service_t *s );
const char * scan_service_GetProvider( const scan_service_t *s );
char * scan_service_GetUri( const scan_service_t *s );
uint16_t scan_service_GetProgram( const scan_service_t *s );
const char * scan_service_GetNetworkName( const scan_service_t *s );

void scan_parameter_Init( scan_parameter_t * );
void scan_parameter_Clean( scan_parameter_t * );

scan_t *scan_New( vlc_object_t *p_obj, const scan_parameter_t *p_parameter,
                  scan_frontend_tune_cb,
                  scan_frontend_stats_cb,
                  scan_demux_filter_cb,
                  scan_demux_read_cb,
                  void * );
void scan_Destroy( scan_t *p_scan );

int scan_Run( scan_t *p_scan );

block_t *scan_GetM3U( scan_t *p_scan );
bool scan_IsCancelled( scan_t *p_scan );

const char *scan_value_modulation(scan_modulation_t);
const char *scan_value_coderate(scan_coderate_t);
