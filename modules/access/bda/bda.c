/*****************************************************************************
 * bda.c : BDA access module for vlc
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 *
 * Author: Ken Self <kens@campoz.fslife.co.uk>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "bda.h"
#include <vlc_plugin.h>

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/
static int  Open( vlc_object_t *p_this );
static int  ParsePath( access_t *p_access, const char* psz_module,
    const int i_param_count, const char** psz_param, const int* i_type );
static void Close( vlc_object_t *p_this );
static block_t *Block( access_t * );
static int Control( access_t *, int, va_list );


#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for DVB streams. This " \
    "value should be set in milliseconds." )

#define ADAPTER_TEXT N_("Adapter card to tune")
#define ADAPTER_LONGTEXT N_("Adapter cards have a device file in directory " \
    "named /dev/dvb/adapter[n] with n>=0.")

#define DEVICE_TEXT N_("Device number to use on adapter")
#define DEVICE_LONGTEXT ""

#define FREQ_TEXT N_("Transponder/multiplex frequency")
#if defined(WIN32) || defined(WINCE)
#    define FREQ_LONGTEXT N_("In kHz for DVB-S or Hz for DVB-C/T")
#else
#    define FREQ_LONGTEXT N_("In kHz for DVB-C/S/T")
#endif

#define INVERSION_TEXT N_("Inversion mode")
#define INVERSION_LONGTEXT N_("Inversion mode [0=off, 1=on, 2=auto]")
static const int i_inversion_list[] = { -1, 0, 1, 2 };
static const char *const ppsz_inversion_text[] = { N_("Undefined"), N_("Off"),
    N_("On"), N_("Auto") };

#define PROBE_TEXT N_("Probe DVB card for capabilities")
#define PROBE_LONGTEXT N_("Some DVB cards do not like to be probed for their " \
    "capabilities, you can disable this feature if you experience some " \
    "trouble.")

#define BUDGET_TEXT N_("Budget mode")
#define BUDGET_LONGTEXT N_("This allows you to stream an entire transponder " \
    "with a \"budget\" card.")

/* Satellite */
#if defined(WIN32) || defined(WINCE)
#    define NETID_TEXT N_("Network Identifier")
#    define NETID_LONGTEXT ""
#else
#    define SATNO_TEXT N_("Satellite number in the Diseqc system")
#    define SATNO_LONGTEXT N_("[0=no diseqc, 1-4=satellite number].")
#endif

#define VOLTAGE_TEXT N_("LNB voltage")
#define VOLTAGE_LONGTEXT N_("In Volts [0, 13=vertical, 18=horizontal].")

#define HIGH_VOLTAGE_TEXT N_("High LNB voltage")
#define HIGH_VOLTAGE_LONGTEXT N_("Enable high voltage if your cables are " \
    "particularly long. This is not supported by all frontends.")

#define TONE_TEXT N_("22 kHz tone")
#define TONE_LONGTEXT N_("[0=off, 1=on, -1=auto].")

#define FEC_TEXT N_("Transponder FEC")
#define FEC_LONGTEXT N_("FEC=Forward Error Correction mode [9=auto].")

#define SRATE_TEXT N_("Transponder symbol rate in kHz")
#define SRATE_LONGTEXT ""

#define LNB_LOF1_TEXT N_("Antenna lnb_lof1 (kHz)")
#define LNB_LOF1_LONGTEXT N_("Low Band Local Osc Freq in kHz usually 9.75GHz")

#define LNB_LOF2_TEXT N_("Antenna lnb_lof2 (kHz)")
#define LNB_LOF2_LONGTEXT N_("High Band Local Osc Freq in kHz usually 10.6GHz")

#define LNB_SLOF_TEXT N_("Antenna lnb_slof (kHz)")
#define LNB_SLOF_LONGTEXT N_( \
    "Low Noise Block switch freq in kHz usually 11.7GHz")

/* Cable */
#define MODULATION_TEXT N_("Modulation type")
#define MODULATION_LONGTEXT N_("QAM constellation points " \
    "[16, 32, 64, 128, 256]")
static const int i_qam_list[] = { -1, 16, 32, 64, 128, 256 };
static const char *const ppsz_qam_text[] = {
    N_("Undefined"), N_("16"), N_("32"), N_("64"), N_("128"), N_("256") };

/* Terrestrial */
#define CODE_RATE_HP_TEXT N_("Terrestrial high priority stream code rate (FEC)")
#define CODE_RATE_HP_LONGTEXT N_("High Priority FEC Rate " \
    "[Undefined,1/2,2/3,3/4,5/6,7/8]")
static const int i_hp_fec_list[] = { -1, 1, 2, 3, 4, 5 };
static const char *const ppsz_hp_fec_text[] = {
    N_("Undefined"), N_("1/2"), N_("2/3"), N_("3/4"), N_("5/6"), N_("7/8") };

#define CODE_RATE_LP_TEXT N_("Terrestrial low priority stream code rate (FEC)")
#define CODE_RATE_LP_LONGTEXT N_("Low Priority FEC Rate " \
    "[Undefined,1/2,2/3,3/4,5/6,7/8]")
static const int i_lp_fec_list[] = { -1, 1, 2, 3, 4, 5 };
static const char *const ppsz_lp_fec_text[] = {
    N_("Undefined"), N_("1/2"), N_("2/3"), N_("3/4"), N_("5/6"), N_("7/8") };

#define BANDWIDTH_TEXT N_("Terrestrial bandwidth")
#define BANDWIDTH_LONGTEXT N_("Terrestrial bandwidth [0=auto,6,7,8 in MHz]")
static const int i_band_list[] = { -1, 6, 7, 8 };
static const char *const ppsz_band_text[] = {
    N_("Undefined"), N_("6 MHz"), N_("7 MHz"), N_("8 MHz") };

#define GUARD_TEXT N_("Terrestrial guard interval")
#define GUARD_LONGTEXT N_("Guard interval [Undefined,1/4,1/8,1/16,1/32]")
static const int i_guard_list[] = { -1, 4, 8, 16, 32 };
static const char *const ppsz_guard_text[] = {
    N_("Undefined"), N_("1/4"), N_("1/8"), N_("1/16"), N_("1/32") };

#define TRANSMISSION_TEXT N_("Terrestrial transmission mode")
#define TRANSMISSION_LONGTEXT N_("Transmission mode [Undefined,2k,8k]")
static const int i_transmission_list[] = { -1, 2, 8 };
static const char *const ppsz_transmission_text[] = {
    N_("Undefined"), N_("2k"), N_("8k") };

#define HIERARCHY_TEXT N_("Terrestrial hierarchy mode")
#define HIERARCHY_LONGTEXT N_("Hierarchy alpha value [Undefined,1,2,4]")
static const int i_hierarchy_list[] = { -1, 1, 2, 4 };
static const char *const ppsz_hierarchy_text[] = {
    N_("Undefined"), N_("1"), N_("2"), N_("4") };

/* BDA module additional DVB-S Parameters */
#define AZIMUTH_TEXT N_("Satellite Azimuth")
#define AZIMUTH_LONGTEXT N_("Satellite Azimuth in tenths of degree")
#define ELEVATION_TEXT N_("Satellite Elevation")
#define ELEVATION_LONGTEXT N_("Satellite Elevation in tenths of degree")
#define LONGITUDE_TEXT N_("Satellite Longitude")
#define LONGITUDE_LONGTEXT N_( \
    "Satellite Longitude in 10ths of degree, -ve=West")
#define POLARISATION_TEXT N_("Satellite Polarisation")
#define POLARISATION_LONGTEXT N_("Satellite Polarisation [H/V/L/R]")
static const char *const ppsz_polar_list[] = { "H", "V", "L", "R" };
static const char *const ppsz_polar_text[] = {
    N_("Horizontal"), N_("Vertical"),
    N_("Circular Left"), N_("Circular Right") };

vlc_module_begin();
    set_shortname( N_("DVB") );
    set_description( N_("DirectShow DVB input") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    add_integer( "dvb-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT,
                 CACHING_LONGTEXT, true );
    add_integer( "dvb-frequency", 11954000, NULL, FREQ_TEXT, FREQ_LONGTEXT,
                 false );
#   if defined(WIN32) || defined(WINCE)
#   else
        add_integer( "dvb-adapter", 0, NULL, ADAPTER_TEXT, ADAPTER_LONGTEXT,
                     false );
        add_integer( "dvb-device", 0, NULL, DEVICE_TEXT, DEVICE_LONGTEXT,
                     true );
        add_bool( "dvb-probe", 1, NULL, PROBE_TEXT, PROBE_LONGTEXT, true );
        add_bool( "dvb-budget-mode", 0, NULL, BUDGET_TEXT, BUDGET_LONGTEXT,
                  true );
#   endif

    /* DVB-S (satellite) */
    add_integer( "dvb-inversion", 2, NULL, INVERSION_TEXT,
        INVERSION_LONGTEXT, true );
        change_integer_list( i_inversion_list, ppsz_inversion_text, NULL );
#   if defined(WIN32) || defined(WINCE)
        add_string( "dvb-polarisation", NULL, NULL, POLARISATION_TEXT,
            POLARISATION_LONGTEXT, true );
            change_string_list( ppsz_polar_list, ppsz_polar_text, 0 );
        add_integer( "dvb-network-id", 0, NULL, NETID_TEXT, NETID_LONGTEXT,
            true );
        add_integer( "dvb-azimuth", 0, NULL, AZIMUTH_TEXT, AZIMUTH_LONGTEXT,
            true );
        add_integer( "dvb-elevation", 0, NULL, ELEVATION_TEXT,
            ELEVATION_LONGTEXT, true );
        add_integer( "dvb-longitude", 0, NULL, LONGITUDE_TEXT,
            LONGITUDE_LONGTEXT, true );
            /* Note: Polaristion H = voltage 18; V = voltage 13; */
#   else
        add_integer( "dvb-satno", 0, NULL, SATNO_TEXT, SATNO_LONGTEXT,
            true );
        add_integer( "dvb-voltage", 13, NULL, VOLTAGE_TEXT, VOLTAGE_LONGTEXT,
            true );
        add_bool( "dvb-high-voltage", 0, NULL, HIGH_VOLTAGE_TEXT,
            HIGH_VOLTAGE_LONGTEXT, true );
        add_integer( "dvb-tone", -1, NULL, TONE_TEXT, TONE_LONGTEXT,
            true );
#   endif
    add_integer( "dvb-lnb-lof1", 0, NULL, LNB_LOF1_TEXT,
        LNB_LOF1_LONGTEXT, true );
    add_integer( "dvb-lnb-lof2", 0, NULL, LNB_LOF2_TEXT,
        LNB_LOF2_LONGTEXT, true );
    add_integer( "dvb-lnb-slof", 0, NULL, LNB_SLOF_TEXT,
        LNB_SLOF_LONGTEXT, true );

    add_integer( "dvb-fec", 9, NULL, FEC_TEXT, FEC_LONGTEXT, true );
    add_integer( "dvb-srate", 27500000, NULL, SRATE_TEXT, SRATE_LONGTEXT,
        false );

    /* DVB-C (cable) */
    add_integer( "dvb-modulation", -1, NULL, MODULATION_TEXT,
        MODULATION_LONGTEXT, true );
        change_integer_list( i_qam_list, ppsz_qam_text, NULL );

    /* DVB-T (terrestrial) */
    add_integer( "dvb-code-rate-hp", -1, NULL, CODE_RATE_HP_TEXT,
        CODE_RATE_HP_LONGTEXT, true );
        change_integer_list( i_hp_fec_list, ppsz_hp_fec_text, NULL );
    add_integer( "dvb-code-rate-lp", -1, NULL, CODE_RATE_LP_TEXT,
        CODE_RATE_LP_LONGTEXT, true );
        change_integer_list( i_lp_fec_list, ppsz_lp_fec_text, NULL );
    add_integer( "dvb-bandwidth", 0, NULL, BANDWIDTH_TEXT, BANDWIDTH_LONGTEXT,
        true );
        change_integer_list( i_band_list, ppsz_band_text, NULL );
    add_integer( "dvb-guard", -1, NULL, GUARD_TEXT, GUARD_LONGTEXT, true );
        change_integer_list( i_guard_list, ppsz_guard_text, NULL );
    add_integer( "dvb-transmission", -1, NULL, TRANSMISSION_TEXT,
        TRANSMISSION_LONGTEXT, true );
        change_integer_list( i_transmission_list, ppsz_transmission_text, NULL );
    add_integer( "dvb-hierarchy", -1, NULL, HIERARCHY_TEXT, HIERARCHY_LONGTEXT,
        true );
        change_integer_list( i_hierarchy_list, ppsz_hierarchy_text, NULL );

    set_capability( "access", 0 );
    add_shortcut( "dvb" );      /* Generic name */

    add_shortcut( "dvb-s" );    /* Satellite */
    add_shortcut( "dvbs" );
    add_shortcut( "qpsk" );
    add_shortcut( "satellite" );

    add_shortcut( "dvb-c" );    /* Cable */
    add_shortcut( "dvbc" );
    add_shortcut( "qam" );
    add_shortcut( "cable" );

    add_shortcut( "dvbt" );    /* Terrestrial */
    add_shortcut( "dvb-t" );
    add_shortcut( "ofdm" );
    add_shortcut( "terrestrial" );

    add_shortcut( "atsc" );     /* Atsc */
    add_shortcut( "usdigital" );

    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Open: open direct show device as an access module
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    const char* psz_module  = "dvb";
    const int   i_param_count = 19;
    const char* psz_param[] = { "frequency", "bandwidth",
        "srate", "azimuth", "elevation", "longitude", "polarisation",
        "modulation", "caching", "lnb-lof1", "lnb-lof2", "lnb-slof",
        "inversion", "network-id", "code-rate-hp", "code-rate-lp",
        "guard", "transmission", "hierarchy" };

    const int   i_type[] = { VLC_VAR_INTEGER, VLC_VAR_INTEGER,
        VLC_VAR_INTEGER, VLC_VAR_INTEGER, VLC_VAR_INTEGER, VLC_VAR_INTEGER,
        VLC_VAR_STRING, VLC_VAR_INTEGER, VLC_VAR_INTEGER, VLC_VAR_INTEGER,
        VLC_VAR_INTEGER, VLC_VAR_INTEGER, VLC_VAR_INTEGER, VLC_VAR_INTEGER,
        VLC_VAR_INTEGER, VLC_VAR_INTEGER, VLC_VAR_INTEGER, VLC_VAR_INTEGER,
        VLC_VAR_INTEGER };

    char  psz_full_name[128];
    int i_ret;

   /* Only if selected */
    if( *p_access->psz_access == '\0' )
        return VLC_EGENERIC;

    /* Setup Access */
    p_access->pf_read = NULL;
    p_access->pf_block = Block;     /* Function to read compressed data */
    p_access->pf_control = Control; /* Function to control the module */
    p_access->pf_seek = NULL;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = false;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->p_sys = p_sys = (access_sys_t *)malloc( sizeof( access_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    memset( p_sys, 0, sizeof( access_sys_t ) );

    for( int i = 0; i < i_param_count; i++ )
    {
        snprintf( psz_full_name, 128, "%s-%s\0", psz_module,
                  psz_param[i] );
        var_Create( p_access, psz_full_name, i_type[i] | VLC_VAR_DOINHERIT );
    }

    /* Parse the command line */
    if( ParsePath( p_access, psz_module, i_param_count, psz_param, i_type ) )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Build directshow graph */
    dvb_newBDAGraph( p_access );

    i_ret = VLC_EGENERIC;

    if( strncmp( p_access->psz_access, "qpsk", 4 ) == 0 ||
        strncmp( p_access->psz_access, "dvb-s", 5 ) == 0 ||
        strncmp( p_access->psz_access, "dvbs", 4 ) == 0 ||
        strncmp( p_access->psz_access, "satellite", 9 ) == 0 )
    {
        i_ret = dvb_SubmitDVBSTuneRequest( p_access );
    }
    if( strncmp( p_access->psz_access, "cable", 5 ) == 0 ||
        strncmp( p_access->psz_access, "dvb-c", 5 ) == 0  ||
        strncmp( p_access->psz_access, "dvbc", 4 ) == 0  ||
        strncmp( p_access->psz_access, "qam", 3 ) == 0 )
    {
        i_ret = dvb_SubmitDVBCTuneRequest( p_access );
    }
    if( strncmp( p_access->psz_access, "terrestrial", 11 ) == 0 ||
        strncmp( p_access->psz_access, "dvb-t", 5 ) == 0 ||
        strncmp( p_access->psz_access, "ofdm", 4 ) == 0 ||
        strncmp( p_access->psz_access, "dvbt", 4 ) == 0 )
    {
        i_ret = dvb_SubmitDVBTTuneRequest( p_access );
    }
    if( strncmp( p_access->psz_access, "usdigital", 9 ) == 0 ||
        strncmp( p_access->psz_access, "atsc", 4 ) == 0 )
    {
        i_ret = dvb_SubmitATSCTuneRequest( p_access );
    }

    if( i_ret != VLC_SUCCESS )
        msg_Warn( p_access, "DVB_Open: Unsupported Network %s",
            p_access->psz_access);
    return i_ret;
}

/*****************************************************************************
 * ParsePath:
 * Parses the path passed to VLC treating it as a MRL which
 * is organized as a sequence of <key>=<value> pairs separated by a colon
 * e.g. :key1=value1:key2=value2:key3=value3.
 * Each <key> is matched to one of the parameters passed in psz_param using
 * whatever characters are provided. e.g. fr = fre = frequency
 *****************************************************************************/
static int ParsePath( access_t *p_access, const char* psz_module,
    const int i_param_count, const char** psz_param, const int* i_type )
{
    const int   MAXPARAM = 20;
    BOOL        b_used[MAXPARAM];
    char*       psz_parser;
    char*       psz_token;
    char*       psz_value;
    vlc_value_t v_value;
    size_t      i_token_len, i_param_len;
    int         i_this_param;
    char        psz_full_name[128];

    if( i_param_count > MAXPARAM )
    {
        msg_Warn( p_access, "ParsePath: Too many parameters: %d > %d",
            i_param_count, MAXPARAM );
            return VLC_EGENERIC;
    }
    for( int i = 0; i < i_param_count; i++ )
        b_used[i] = FALSE;
    psz_parser = p_access->psz_path;
    if( strlen( psz_parser ) <= 0 )
        return VLC_SUCCESS;

    i_token_len = strcspn( psz_parser, ":" );
    if( i_token_len <= 0 )
        i_token_len  = strcspn( ++psz_parser, ":" );
 
    do
    {
        psz_token = strndup( psz_parser, i_token_len );
        i_param_len  = strcspn( psz_token, "=" );
        if( i_param_len <= 0 )
        {
            msg_Warn( p_access, "ParsePath: Unspecified parameter %s",
                psz_token );
            if( psz_token )
                free( psz_token );
            return VLC_EGENERIC;
        }
        i_this_param = -1;
        for( int i = 0; i < i_param_count; i++ )
        {
            if( strncmp( psz_token, psz_param[i], i_param_len ) == 0 )
            {
                i_this_param = i;
                break;
            }
        }
        if( i_this_param < 0 )
        {
            msg_Warn( p_access, "ParsePath: Unknown parameter %s", psz_token );
            if( psz_token )
                free( psz_token );
            return VLC_EGENERIC;
        }
        if( b_used[i_this_param] )
        {
            msg_Warn( p_access, "ParsePath: Duplicate parameter %s",
                psz_token );
            if( psz_token )
                free( psz_token );
            return VLC_EGENERIC;
        }
        b_used[i_this_param] = TRUE;

        /* if "=" was found in token then value starts at
         * psz_token + i_paramlen + 1
         * else there is no value specified so we use an empty string */
        psz_value = psz_token + i_param_len + 1;
        if( i_param_len >= i_token_len )
            psz_value--;
        if( i_type[i_this_param] == VLC_VAR_STRING )
             v_value.psz_string = strdup( psz_value );
        if( i_type[i_this_param] == VLC_VAR_INTEGER )
             v_value.i_int = atol( psz_value );
        snprintf( psz_full_name, 128, "%s-%s\0", psz_module,
            psz_param[i_this_param] );
        var_Set( p_access, psz_full_name, v_value );

        if( psz_token )
            free( psz_token );
        if( i_token_len >= strlen( psz_parser ) )
            break;
        psz_parser += i_token_len + 1;
        i_token_len = strcspn( psz_parser, ":" );
    }
    while( TRUE );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * AccessClose: close device
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t *)p_this;
    access_sys_t *p_sys    = p_access->p_sys;

    dvb_deleteBDAGraph( p_access );

    vlc_mutex_destroy( &p_sys->lock );
    vlc_cond_destroy( &p_sys->wait );

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    bool   *pb_bool, b_bool;
    int          *pi_int, i_int;
    int64_t      *pi_64;

    switch( i_query )
    {
    case ACCESS_CAN_SEEK:           /* 0 */
    case ACCESS_CAN_FASTSEEK:       /* 1 */
    case ACCESS_CAN_PAUSE:          /* 2 */
    case ACCESS_CAN_CONTROL_PACE:   /* 3 */
        pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = false;
        break;
    case ACCESS_GET_MTU:            /* 4 */
        pi_int = (int*)va_arg( args, int * );
        *pi_int = 0;
        break;
    case ACCESS_GET_PTS_DELAY:      /* 5 */
        pi_64 = (int64_t*)va_arg( args, int64_t * );
        *pi_64 = var_GetInteger( p_access, "dvb-caching" ) * 1000;
        break;
        /* */
    case ACCESS_GET_TITLE_INFO:     /* 6 */
    case ACCESS_GET_META:           /* 7 */
    case ACCESS_SET_PAUSE_STATE:    /* 8 */
    case ACCESS_SET_TITLE:          /* 9 */
    case ACCESS_SET_SEEKPOINT:      /* 10 */
    case ACCESS_GET_CONTENT_TYPE:
        return VLC_EGENERIC;

    case ACCESS_SET_PRIVATE_ID_STATE: /* 11 */
        i_int  = (int)va_arg( args, int );
        b_bool = (bool)va_arg( args, int );
        break;
    case ACCESS_SET_PRIVATE_ID_CA:  /* 12 -From Demux */
        break;
    default:
        msg_Warn( p_access,
                  "DVB_Control: Unimplemented query in control %d", i_query );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Block:
 *****************************************************************************/
static block_t *Block( access_t *p_access )
{
    block_t *p_block;
    long l_buffer_len;

    if( !vlc_object_alive (p_access) )
        return NULL;

    l_buffer_len = dvb_GetBufferSize( p_access );
    if( l_buffer_len < 0 )
    {
        p_access->info.b_eof = true;
        return NULL;
    }

    p_block = block_New( p_access, l_buffer_len );
    if( dvb_ReadBuffer( p_access, &l_buffer_len, p_block->p_buffer ) < 0 )
    {
        p_access->info.b_eof = true;
        return NULL;
    }

    return p_block;
}
