/*****************************************************************************
 * ts_psi.c: Transport Stream input module for VLC.
 *****************************************************************************
 * Copyright (C) 2004-2016 VLC authors and VideoLAN
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#ifndef _DVBPSI_DVBPSI_H_
 # include <dvbpsi/dvbpsi.h>
#endif

#include <dvbpsi/descriptor.h>
#include <dvbpsi/pat.h>
#include <dvbpsi/pmt.h>
#include <dvbpsi/dr.h>

#include <vlc_demux.h>
#include <vlc_bits.h>

#include "ts_streams.h"
#include "ts_psi.h"
#include "ts_pid.h"
#include "ts_streams_private.h"
#include "ts.h"

#include "ts_strings.h"

#include "timestamps.h"

#include "../../codec/jpeg2000.h"
#include "../../codec/opus_header.h"
#include "../../packetizer/dts_header.h"

#include "sections.h"
#include "ts_sl.h"
#include "ts_scte.h"
#include "ts_psip.h"
#include "ts_si.h"
#include "ts_metadata.h"

#include "../access/dtv/en50221_capmt.h"

#include <assert.h>

static void PIDFillFormat( demux_t *, ts_stream_t *p_pes, int i_stream_type, ts_transport_type_t * );
static void PMTCallBack( void *data, dvbpsi_pmt_t *p_dvbpsipmt );
static ts_standards_e ProbePMTStandard( const dvbpsi_pmt_t *p_dvbpsipmt );

static int PATCheck( demux_t *p_demux, dvbpsi_pat_t *p_pat )
{
    /* Some Dreambox streams have all PMT set to same pid */
    int i_prev_pid = -1;
    for( dvbpsi_pat_program_t * p_program = p_pat->p_first_program;
         p_program != NULL;
         p_program = p_program->p_next )
    {
        if( p_program->i_pid == i_prev_pid )
        {
            msg_Warn( p_demux, "PAT check failed: duplicate program pid %d", i_prev_pid );
            return VLC_EGENERIC;
        }
        i_prev_pid = p_program->i_pid;
    }
    return VLC_SUCCESS;
}

static void PATCallBack( void *data, dvbpsi_pat_t *p_dvbpsipat )
{
    demux_t              *p_demux = data;
    demux_sys_t          *p_sys = p_demux->p_sys;
    dvbpsi_pat_program_t *p_program;
    ts_pid_t             *patpid = GetPID(p_sys, 0);
    ts_pat_t             *p_pat = GetPID(p_sys, 0)->u.p_pat;

    patpid->i_flags |= FLAG_SEEN;

    msg_Dbg( p_demux, "PATCallBack called" );

    if(unlikely( GetPID(p_sys, 0)->type != TYPE_PAT ))
    {
        msg_Warn( p_demux, "PATCallBack called on invalid pid" );
        return;
    }

    /* check versioning changes */
    if( !p_pat->b_generated )
    {
        /* override hotfixes */
        if( ( p_pat->i_version != -1 && p_dvbpsipat->i_version == p_pat->i_version ) ||
            ( p_pat->i_ts_id != -1 && p_dvbpsipat->i_ts_id != p_pat->i_ts_id ) )
        {
            dvbpsi_pat_delete( p_dvbpsipat );
            return;
        }
    }
    else msg_Warn( p_demux, "Replacing generated PAT with one received from stream" );

    /* check content */
    if( !p_dvbpsipat->b_current_next || p_sys->b_user_pmt ||
        PATCheck( p_demux, p_dvbpsipat ) )
    {
        dvbpsi_pat_delete( p_dvbpsipat );
        return;
    }

    msg_Dbg( p_demux, "new PAT ts_id=%d version=%d current_next=%d",
             p_dvbpsipat->i_ts_id, p_dvbpsipat->i_version, p_dvbpsipat->b_current_next );

    /* Save old programs array */
    DECL_ARRAY(ts_pid_t *) old_pmt_rm;
    old_pmt_rm.i_alloc = p_pat->programs.i_alloc;
    old_pmt_rm.i_size = p_pat->programs.i_size;
    old_pmt_rm.p_elems = p_pat->programs.p_elems;
    ARRAY_INIT(p_pat->programs);

    bool b_force_reselect = false;
    if( p_sys->programs.i_size && p_sys->seltype == PROGRAM_AUTO_DEFAULT )
    {
        /* If the program was set by default selection, we'll need to repick */
        b_force_reselect = true;
        for( p_program = p_dvbpsipat->p_first_program; p_program != NULL;
             p_program = p_program->p_next )
        {
            if( p_sys->programs.p_elems[0] == p_program->i_number )
            {
                b_force_reselect = false;
                break;
            }
        }
        if( b_force_reselect )
            ARRAY_RESET( p_sys->programs );
    }

    /* now create programs */
    for( p_program = p_dvbpsipat->p_first_program; p_program != NULL;
         p_program = p_program->p_next )
    {
        msg_Dbg( p_demux, "  * number=%d pid=%d", p_program->i_number,
                 p_program->i_pid );
        if( p_program->i_number == 0 )
            continue;

        ts_pid_t *pmtpid = GetPID(p_sys, p_program->i_pid);

        bool b_existing = (pmtpid->type == TYPE_PMT);
        /* create or temporary incref pid */
        if( !PIDSetup( p_demux, TYPE_PMT, pmtpid, patpid ) )
        {
            msg_Warn( p_demux, "  * number=%d pid=%d (ignored)", p_program->i_number,
                     p_program->i_pid );
            continue;
        }

        if( !b_existing || pmtpid->u.p_pmt->i_number != p_program->i_number )
        {
            if( b_existing && pmtpid->u.p_pmt->i_number != p_program->i_number )
                dvbpsi_pmt_detach(pmtpid->u.p_pmt->handle);

            if( !dvbpsi_pmt_attach( pmtpid->u.p_pmt->handle, p_program->i_number, PMTCallBack, p_demux ) )
                msg_Err( p_demux, "PATCallback failed attaching PMTCallback to program %d",
                         p_program->i_number );
        }

        pmtpid->u.p_pmt->i_number = p_program->i_number;

        ARRAY_APPEND( p_pat->programs, pmtpid );

        /* Now select PID at access level */
        if( p_sys->programs.i_size == 0 ||
            ProgramIsSelected( p_sys, p_program->i_number ) )
        {
            if( p_sys->programs.i_size == 0 )
            {
                msg_Dbg( p_demux, "temporary receiving program %d", p_program->i_number );
                p_sys->b_default_selection = true;
                ARRAY_APPEND( p_sys->programs, p_program->i_number );
            }

            SetPIDFilter( p_sys, pmtpid, true );

            if ( p_sys->es_creation == DELAY_ES )
                p_sys->es_creation = CREATE_ES;
        }
    }
    p_pat->i_version = p_dvbpsipat->i_version;
    p_pat->i_ts_id = p_dvbpsipat->i_ts_id;
    p_pat->b_generated = false;

    for(int i=0; i<old_pmt_rm.i_size; i++)
    {
        /* decref current or release now unreferenced */
        PIDRelease( p_demux, old_pmt_rm.p_elems[i] );
    }
    ARRAY_RESET(old_pmt_rm);

    if( b_force_reselect && p_sys->programs.i_size )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_GROUP, p_sys->programs.p_elems[0] );
    }

    dvbpsi_pat_delete( p_dvbpsipat );
}

#define PMT_DESC_PREFIX " * PMT descriptor: "
#define PMT_DESC_INDENT "                 : "
static void ParsePMTRegistrations( demux_t *p_demux, const dvbpsi_descriptor_t  *p_firstdr,
                                   ts_pmt_t *p_pmt, ts_pmt_registration_type_t *p_registration_type )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    ts_pmt_registration_type_t registration_type = *p_registration_type;
    int i_arib_score_flags = 0; /* Descriptors can be repeated */

    for( const dvbpsi_descriptor_t *p_dr = p_firstdr; p_dr != NULL; p_dr = p_dr->p_next )
    {
        /* general descriptors handling < 0x40 and scoring */
        if( p_dr->i_tag < 0x40 )
        {
            msg_Dbg( p_demux, PMT_DESC_PREFIX "%s (0x%x)",
                     ISO13818_1_Get_Descriptor_Description(p_dr->i_tag), p_dr->i_tag );
        }

        switch(p_dr->i_tag)
        {
            case 0x05: /* Registration Descriptor */
            {
                if( p_dr->i_length != 4 )
                {
                    msg_Warn( p_demux, PMT_DESC_INDENT "invalid registration descriptor" );
                    break;
                }

                static const struct
                {
                    const char rgs[4];
                    const ts_pmt_registration_type_t reg;
                } regs[] = {
                    { { 'H', 'D', 'M', 'V' }, TS_PMT_REGISTRATION_BLURAY },
                    { { 'H', 'D', 'P', 'R' }, TS_PMT_REGISTRATION_BLURAY },
                    { { 'G', 'A', '9', '4' }, TS_PMT_REGISTRATION_ATSC },
                };

                for( unsigned i=0; i<ARRAY_SIZE(regs); i++ )
                {
                    if( !memcmp( regs[i].rgs, p_dr->p_data, 4 ) )
                    {
                        registration_type = regs[i].reg;
                        msg_Dbg( p_demux, PMT_DESC_INDENT "%4.4s registration", p_dr->p_data );
                        break;
                    }
                }
            }
            break;

            case 0x09:
            {
                dvbpsi_ca_dr_t *p_cadr = dvbpsi_DecodeCADr( (dvbpsi_descriptor_t *) p_dr );
                msg_Dbg( p_demux, PMT_DESC_INDENT "CA System ID 0x%x", p_cadr->i_ca_system_id );
                i_arib_score_flags |= (p_cadr->i_ca_system_id == 0x05);
            }
            break;

            case 0x1d: /* We have found an IOD descriptor */
                p_pmt->iod = IODNew( VLC_OBJECT(p_demux), p_dr->i_length, p_dr->p_data );
                break;

            case 0xC1:
                i_arib_score_flags |= 1 << 2;
                break;

            case 0xF6:
                i_arib_score_flags |= 1 << 1;
                break;

            default:
                break;
        }
    }

    if ( p_sys->standard == TS_STANDARD_AUTO &&
         registration_type == TS_PMT_REGISTRATION_NONE &&
         i_arib_score_flags == 0x07 ) //0b111
    {
        registration_type = TS_PMT_REGISTRATION_ARIB;
    }

    *p_registration_type = registration_type;
}

static void ParsePMTPrivateRegistrations( demux_t *p_demux, const dvbpsi_descriptor_t *p_firstdr,
                                          ts_pmt_t *p_pmt, ts_standards_e i_standard )
{
    VLC_UNUSED(p_pmt);
     /* Now process private descriptors >= 0x40 */
    for( const dvbpsi_descriptor_t *p_dr = p_firstdr; p_dr != NULL; p_dr = p_dr->p_next )
    {
        if( p_dr->i_tag < 0x40 )
            continue;

        switch( i_standard )
        {
            case TS_STANDARD_ARIB:
            {
                const char *psz_desc = ARIB_B10_Get_PMT_Descriptor_Description( p_dr->i_tag );
                if( psz_desc )
                    msg_Dbg( p_demux, PMT_DESC_PREFIX "%s (0x%x)", psz_desc, p_dr->i_tag );
                else
                    msg_Dbg( p_demux, PMT_DESC_PREFIX "Unknown Private (0x%x)", p_dr->i_tag );
            }
            break;

            case TS_STANDARD_DVB:
            case TS_STANDARD_AUTO:
            {
                if( p_dr->i_tag == 0x88 )
                {
                    /* EACEM Simulcast HD Logical channels ordering */
                    /* TODO: apply visibility flags */
                    msg_Dbg( p_demux, PMT_DESC_PREFIX "EACEM Simulcast HD" );
                    break;
                }
            }
            /* fallthrough */
            default:
                msg_Dbg( p_demux, PMT_DESC_PREFIX "Unknown Private (0x%x)", p_dr->i_tag );
                break;
        }
    }
}

/*****************************************************************************
 * PMT callback and helpers
 *****************************************************************************/
static dvbpsi_descriptor_t *PMTEsFindDescriptor( const dvbpsi_pmt_es_t *p_es,
                                                 int i_tag )
{
    dvbpsi_descriptor_t *p_dr = p_es->p_first_descriptor;;
    while( p_dr && ( p_dr->i_tag != i_tag ) )
        p_dr = p_dr->p_next;
    return p_dr;
}
static bool PMTEsHasRegistration( demux_t *p_demux,
                                  const dvbpsi_pmt_es_t *p_es,
                                  const char *psz_tag )
{
    dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_es, 0x05 );
    if( !p_dr )
        return false;

    if( p_dr->i_length < 4 )
    {
        msg_Warn( p_demux, "invalid Registration Descriptor" );
        return false;
    }

    assert( strlen(psz_tag) == 4 );
    return !memcmp( p_dr->p_data, psz_tag, 4 );
}

static bool PMTEsHasComponentTagBetween( const dvbpsi_pmt_es_t *p_es,
                                         uint8_t i_low, uint8_t i_high )
{
    dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_es, 0x52 );
    if( !p_dr )
        return false;
    dvbpsi_stream_identifier_dr_t *p_si = dvbpsi_DecodeStreamIdentifierDr( p_dr );
    if( !p_si )
        return false;

    return p_si->i_component_tag >= i_low && p_si->i_component_tag <= i_high;
}

static ts_standards_e ProbePMTStandard( const dvbpsi_pmt_t *p_dvbpsipmt )
{
    dvbpsi_pmt_es_t *p_dvbpsies;
    for( p_dvbpsies = p_dvbpsipmt->p_first_es; p_dvbpsies; p_dvbpsies = p_dvbpsies->p_next )
    {
        if( p_dvbpsies->i_type == 0x06 )
        {
            /* Probe for ARIB subtitles */
            dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0xFD );
            if( p_dr && p_dr->i_length >= 2 )
            {
                const uint16_t i_data_component_id = GetWBE(p_dr->p_data);
                if( ( i_data_component_id == 0x0008 &&
                      PMTEsHasComponentTagBetween( p_dvbpsies, 0x30, 0x37 ) ) ||
                    ( i_data_component_id == 0x0012 &&
                      PMTEsHasComponentTagBetween( p_dvbpsies, 0x87, 0x88 ) ) )
                    return TS_STANDARD_ARIB;
            }
        }
    }
    return TS_STANDARD_AUTO;
}

static void SetupAudioExtendedDescriptors( demux_t *p_demux, ts_es_t *p_es,
                                           const dvbpsi_pmt_es_t *p_dvbpsies )
{
    if( p_demux->p_sys->standard == TS_STANDARD_AUTO ||
        p_demux->p_sys->standard == TS_STANDARD_DVB )
    {
        const dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x7F );
        if( p_dr && p_dr->i_length > 1 && p_dr->p_data[0] == 0x06 /* Tag extension */ )
        {
            static const char *editorial_classification_coding[] = {
                N_("Main audio"),
                N_("Audio description for the visually impaired"),
                N_("Clean audio for the hearing impaired"),
                N_("Spoken subtitles for the visually impaired"),
            };

            uint8_t i_audio_type = (p_dr->p_data[1] & 0x7F) >> 2;

            if( i_audio_type < ARRAY_SIZE(editorial_classification_coding) )
            {
                free( p_es->fmt.psz_description );
                p_es->fmt.psz_description = strdup(editorial_classification_coding[i_audio_type]);
            }

            if( i_audio_type == 0x00 /* Main Audio */ )
                p_es->fmt.i_priority = ES_PRIORITY_SELECTABLE_MIN + 1;

            if( (p_dr->p_data[1] & 0x80) == 0x00 ) /* Split mixed */
                p_es->fmt.i_priority = ES_PRIORITY_NOT_DEFAULTABLE;

            if( (p_dr->p_data[1] & 0x01) && p_dr->i_length >= 5 )
            {
                free( p_es->fmt.psz_language );
                p_es->fmt.psz_language = malloc( 4 );
                if( p_es->fmt.psz_language )
                {
                    memcpy( p_es->fmt.psz_language, &p_dr->p_data[2], 3 );
                    p_es->fmt.psz_language[3] = 0;
                    msg_Dbg( p_demux, "      found language: %s", p_es->fmt.psz_language );
                }
            }
        }
    }
}

static void SetupISO14496Descriptors( demux_t *p_demux, ts_stream_t *p_pes,
                                      const ts_pmt_t *p_pmt, const dvbpsi_pmt_es_t *p_dvbpsies )
{
    const dvbpsi_descriptor_t *p_dr = p_dvbpsies->p_first_descriptor;
    ts_es_t *p_es = p_pes->p_es;

    while( p_dr )
    {
        uint8_t i_length = p_dr->i_length;

        switch( p_dr->i_tag )
        {
            case 0x1f: /* FMC Descriptor */
                while( i_length >= 2 /* see below */ && !p_es->i_sl_es_id )
                {
                    p_es->i_sl_es_id = ( p_dr->p_data[0] << 8 ) | p_dr->p_data[1];
                    /* FIXME: map all ids and flexmux channels */
                    /* Handle broken streams with 2 byte 0x1F descriptors
                     * see samples/A-codecs/AAC/freetv_aac_latm.txt */
                    if( i_length == 2 )
                        break;
                    i_length -= 3;
                    msg_Dbg( p_demux, "     - found FMC_descriptor mapping es_id=%"PRIu16, p_es->i_sl_es_id );
                }
                break;
            case 0x1e: /* SL Descriptor */
                if( i_length == 2 )
                {
                    p_es->i_sl_es_id = ( p_dr->p_data[0] << 8 ) | p_dr->p_data[1];
                    msg_Dbg( p_demux, "     - found SL_descriptor mapping es_id=%"PRIu16, p_es->i_sl_es_id );

                    if( p_dvbpsies->i_type == 0x12 ) /* SL AU pes stream */
                    {
                        if( !p_pes->p_proc )
                            p_pes->p_proc = SL_stream_processor_New( p_pes );
                    }
                    else if( p_dvbpsies->i_type == 0x13 ) /* IOD / SL sections */
                    {
                        ts_sections_processor_Add( p_demux,
                                                   &p_pes->p_sections_proc, 0x05, 0x00,
                                                   SLPackets_Section_Handler, p_pes );
                    }
                    p_pes->b_always_receive = true;
                }
                break;
            default:
                break;
        }

        p_dr = p_dr->p_next;
    }

    if( p_es->i_sl_es_id )
    {
        const es_mpeg4_descriptor_t *p_mpeg4desc = GetMPEG4DescByEsId( p_pmt, p_es->i_sl_es_id );
        if( p_mpeg4desc && p_mpeg4desc->b_ok )
        {
            if( !SetupISO14496LogicalStream( p_demux, &p_mpeg4desc->dec_descr, &p_es->fmt ) )
                msg_Dbg( p_demux, "     - IOD not yet available for es_id=%"PRIu16, p_es->i_sl_es_id );
        }
    }
    else
    {
        switch( p_dvbpsies->i_type )
        {
        /* non fatal, set by packetizer */
        case 0x0f: /* ADTS */
        case 0x11: /* LOAS */
            msg_Dbg( p_demux, "     - SL/FMC descriptor not found/matched" );
            break;
        default:
            msg_Err( p_demux, "      - SL/FMC descriptor not found/matched" );
            break;
        }
    }
}

static void SetupMetadataDescriptors( demux_t *p_demux, ts_stream_t *p_stream, const dvbpsi_pmt_es_t *p_dvbpsies )
{
    ts_es_t *p_es = p_stream->p_es;
    const dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x26 );
    if( p_dr && p_dr->i_length >= 13 )
    {
        /* app format 0xFFFF
         * metadata_application_format_identifier ID3\x20
         * i_metadata_format 0xFF
         * metadata_format_identifier ID3\x20 */
        if( !memcmp( p_dr->p_data, "\xFF\xFFID3 \xFFID3 ", 11 ) &&
            (p_dr->p_data[12] & 0xF0) == 0x00 )
        {
            p_es->metadata.i_format = VLC_FOURCC('I', 'D', '3', ' ');
            p_es->metadata.i_service_id = p_dr->p_data[11];
            msg_Dbg( p_demux, "     - found Metadata_descriptor type ID3 with service_id=0x%"PRIx8,
                     p_dr->p_data[11] );
            if( !p_stream->p_proc )
                p_stream->p_proc = Metadata_stream_processor_New( p_stream, p_demux->out );
        }
    }
}

static void SetupAVCDescriptors( demux_t *p_demux, ts_es_t *p_es, const dvbpsi_pmt_es_t *p_dvbpsies )
{
    const dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x28 );
    if( p_dr && p_dr->i_length >= 4 )
    {
        p_es->fmt.i_profile = p_dr->p_data[0];
        p_es->fmt.i_level = p_dr->p_data[2];
        msg_Dbg( p_demux, "     - found AVC_video_descriptor profile=0x%"PRIx8" level=0x%"PRIx8,
                 p_es->fmt.i_profile, p_es->fmt.i_level );
    }
}

static void SetupJ2KDescriptors( demux_t *p_demux, ts_es_t *p_es, const dvbpsi_pmt_es_t *p_dvbpsies )
{
    const dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x32 );
    if( p_dr && p_dr->i_length >= 24 )
    {
        es_format_Change( &p_es->fmt, VIDEO_ES, VLC_CODEC_JPEG2000 );
        p_es->fmt.i_profile = p_dr->p_data[0];
        p_es->fmt.i_level = p_dr->p_data[1];
        p_es->fmt.video.i_width = GetDWBE(&p_dr->p_data[2]);
        p_es->fmt.video.i_height = GetDWBE(&p_dr->p_data[6]);
        p_es->fmt.video.i_frame_rate_base = GetWBE(&p_dr->p_data[18]);
        p_es->fmt.video.i_frame_rate = GetWBE(&p_dr->p_data[20]);
        j2k_fill_color_profile( p_dr->p_data[21],
                               &p_es->fmt.video.primaries,
                               &p_es->fmt.video.transfer,
                               &p_es->fmt.video.space );
        p_es->b_interlaced = p_dr->p_data[23] & 0x40;
        if( p_dr->i_length > 24 )
        {
            p_es->fmt.p_extra = malloc(p_dr->i_length - 24);
            if( p_es->fmt.p_extra )
                p_es->fmt.i_extra = p_dr->i_length - 24;
        }
        msg_Dbg( p_demux, "     - found J2K_video_descriptor profile=0x%"PRIx8" level=0x%"PRIx8,
                 p_es->fmt.i_profile, p_es->fmt.i_level );
    }
}

typedef struct
{
    int  i_type;
    int  i_magazine;
    int  i_page;
    char p_iso639[3];
} ts_teletext_page_t;

static const char *const ppsz_teletext_type[] = {
 "",
 N_("Teletext"),
 N_("Teletext subtitles"),
 N_("Teletext: additional information"),
 N_("Teletext: program schedule"),
 N_("Teletext subtitles: hearing impaired")
};

static void PMTSetupEsTeletext( demux_t *p_demux, ts_stream_t *p_pes,
                                const dvbpsi_pmt_es_t *p_dvbpsies )
{
    es_format_t *p_fmt = &p_pes->p_es->fmt;

    ts_teletext_page_t p_page[2 * 64 + 20];
    unsigned i_page = 0;
    dvbpsi_descriptor_t *p_dr;

    /* Gather pages information */
    for( unsigned i_tag_idx = 0; i_tag_idx < 2; i_tag_idx++ )
    {
        p_dr = PMTEsFindDescriptor( p_dvbpsies, i_tag_idx == 0 ? 0x46 : 0x56 );
        if( !p_dr )
            continue;

        dvbpsi_teletext_dr_t *p_sub = dvbpsi_DecodeTeletextDr( p_dr );
        if( !p_sub )
            continue;

        for( int i = 0; i < p_sub->i_pages_number; i++ )
        {
            const dvbpsi_teletextpage_t *p_src = &p_sub->p_pages[i];

            if( p_src->i_teletext_type >= 0x06 )
                continue;

            assert( i_page < sizeof(p_page)/sizeof(*p_page) );

            ts_teletext_page_t *p_dst = &p_page[i_page++];

            p_dst->i_type = p_src->i_teletext_type;
            p_dst->i_magazine = p_src->i_teletext_magazine_number
                ? p_src->i_teletext_magazine_number : 8;
            p_dst->i_page = p_src->i_teletext_page_number;
            memcpy( p_dst->p_iso639, p_src->i_iso6392_language_code, 3 );
        }
    }

    p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x59 );
    if( p_dr )
    {
        dvbpsi_subtitling_dr_t *p_sub = dvbpsi_DecodeSubtitlingDr( p_dr );
        for( int i = 0; p_sub && i < p_sub->i_subtitles_number; i++ )
        {
            dvbpsi_subtitle_t *p_src = &p_sub->p_subtitle[i];

            if( p_src->i_subtitling_type < 0x01 || p_src->i_subtitling_type > 0x03 )
                continue;

            assert( i_page < sizeof(p_page)/sizeof(*p_page) );

            ts_teletext_page_t *p_dst = &p_page[i_page++];

            switch( p_src->i_subtitling_type )
            {
            case 0x01:
                p_dst->i_type = 0x02;
                break;
            default:
                p_dst->i_type = 0x03;
                break;
            }
            /* FIXME check if it is the right split */
            p_dst->i_magazine = (p_src->i_composition_page_id >> 8)
                ? (p_src->i_composition_page_id >> 8) : 8;
            p_dst->i_page = p_src->i_composition_page_id & 0xff;
            memcpy( p_dst->p_iso639, p_src->i_iso6392_language_code, 3 );
        }
    }

    /* */
    es_format_Change(p_fmt, SPU_ES, VLC_CODEC_TELETEXT );

    if( !p_demux->p_sys->b_split_es || i_page <= 0 )
    {
        p_fmt->subs.teletext.i_magazine = -1;
        p_fmt->subs.teletext.i_page = 0;
        p_fmt->psz_description = strdup( vlc_gettext(ppsz_teletext_type[1]) );

        p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x46 );
        if( !p_dr )
            p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x56 );

        if( !p_demux->p_sys->b_split_es && p_dr && p_dr->i_length > 0 )
        {
            /* Descriptor pass-through */
            p_fmt->p_extra = malloc( p_dr->i_length );
            if( p_fmt->p_extra )
            {
                p_fmt->i_extra = p_dr->i_length;
                memcpy( p_fmt->p_extra, p_dr->p_data, p_dr->i_length );
            }
        }
    }
    else
    {
        for( unsigned i = 0; i < i_page; i++ )
        {
            ts_es_t *p_page_es;

            /* */
            if( i == 0 )
            {
                p_page_es = p_pes->p_es;
            }
            else
            {
                p_page_es = ts_es_New( p_pes->p_es->p_program );
                if( !p_page_es )
                    break;

                es_format_Copy( &p_page_es->fmt, p_fmt );
                free( p_page_es->fmt.psz_language );
                free( p_page_es->fmt.psz_description );
                p_page_es->fmt.psz_language = NULL;
                p_page_es->fmt.psz_description = NULL;
                ts_stream_Add_es( p_pes, p_page_es, true );
            }

            /* */
            const ts_teletext_page_t *p = &p_page[i];
            p_page_es->fmt.i_priority = (p->i_type == 0x02 || p->i_type == 0x05) ?
                      ES_PRIORITY_SELECTABLE_MIN : ES_PRIORITY_NOT_DEFAULTABLE;
            p_page_es->fmt.psz_language = strndup( p->p_iso639, 3 );
            p_page_es->fmt.psz_description = strdup(vlc_gettext(ppsz_teletext_type[p->i_type]));
            p_page_es->fmt.subs.teletext.i_magazine = p->i_magazine;
            p_page_es->fmt.subs.teletext.i_page = p->i_page;

            msg_Dbg( p_demux,
                         "    * ttxt type=%s lan=%s page=%d%02x",
                         p_page_es->fmt.psz_description,
                         p_page_es->fmt.psz_language,
                         p->i_magazine, p->i_page );
        }
    }
}
static void PMTSetupEsDvbSubtitle( demux_t *p_demux, ts_stream_t *p_pes,
                                   const dvbpsi_pmt_es_t *p_dvbpsies )
{
    es_format_t *p_fmt = &p_pes->p_es->fmt;

    es_format_Change( p_fmt, SPU_ES, VLC_CODEC_DVBS );

    dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x59 );
    int i_page = 0;
    dvbpsi_subtitling_dr_t *p_sub = dvbpsi_DecodeSubtitlingDr( p_dr );
    for( int i = 0; p_sub && i < p_sub->i_subtitles_number; i++ )
    {
        const int i_type = p_sub->p_subtitle[i].i_subtitling_type;
        if( ( i_type >= 0x10 && i_type <= 0x14 ) ||
            ( i_type >= 0x20 && i_type <= 0x24 ) )
            i_page++;
    }

    if( !p_demux->p_sys->b_split_es  || i_page <= 0 )
    {
        p_fmt->subs.dvb.i_id = -1;
        p_fmt->psz_description = strdup( _("DVB subtitles") );

        if( !p_demux->p_sys->b_split_es && p_dr && p_dr->i_length > 0 )
        {
            /* Descriptor pass-through */
            p_fmt->p_extra = malloc( p_dr->i_length );
            if( p_fmt->p_extra )
            {
                p_fmt->i_extra = p_dr->i_length;
                memcpy( p_fmt->p_extra, p_dr->p_data, p_dr->i_length );
            }
        }
    }
    else
    {
        for( int i = 0; i < p_sub->i_subtitles_number; i++ )
        {
            ts_es_t *p_subs_es;

            /* */
            if( i == 0 )
            {
                p_subs_es = p_pes->p_es;
            }
            else
            {
                p_subs_es = ts_es_New( p_pes->p_es->p_program );
                if( !p_subs_es )
                    break;

                es_format_Copy( &p_subs_es->fmt, p_fmt );
                free( p_subs_es->fmt.psz_language );
                free( p_subs_es->fmt.psz_description );
                p_subs_es->fmt.psz_language = NULL;
                p_subs_es->fmt.psz_description = NULL;

                ts_stream_Add_es( p_pes, p_subs_es, true );
            }

            /* */
            const dvbpsi_subtitle_t *p = &p_sub->p_subtitle[i];
            p_subs_es->fmt.psz_language = strndup( (char *)p->i_iso6392_language_code, 3 );
            switch( p->i_subtitling_type )
            {
            case 0x10: /* unspec. */
            case 0x11: /* 4:3 */
            case 0x12: /* 16:9 */
            case 0x13: /* 2.21:1 */
            case 0x14: /* HD monitor */
                p_subs_es->fmt.psz_description = strdup( _("DVB subtitles") );
                break;
            case 0x20: /* Hearing impaired unspec. */
            case 0x21: /* h.i. 4:3 */
            case 0x22: /* h.i. 16:9 */
            case 0x23: /* h.i. 2.21:1 */
            case 0x24: /* h.i. HD monitor */
                p_subs_es->fmt.psz_description = strdup( _("DVB subtitles: hearing impaired") );
                break;
            default:
                break;
            }

            /* Hack, FIXME */
            p_subs_es->fmt.subs.dvb.i_id = ( p->i_composition_page_id <<  0 ) |
                                      ( p->i_ancillary_page_id   << 16 );
        }
    }
}

static int vlc_ceil_log2( const unsigned int val )
{
    int n = 31 - clz(val);
    if ((1U << n) != val)
        n++;

    return n;
}

static void OpusSetup(demux_t *demux, uint8_t *p, size_t len, es_format_t *p_fmt)
{
    OpusHeader h;

    /* default mapping */
    static const unsigned char map[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    memcpy(h.stream_map, map, sizeof(map));

    int csc, mapping;
    int channels = 0;
    int stream_count = 0;
    int ccc = p[1]; // channel_config_code
    if (ccc <= 8) {
        channels = ccc;
        if (channels)
            mapping = channels > 2;
        else {
            mapping = 255;
            channels = 2; // dual mono
        }
        static const uint8_t p_csc[8] = { 0, 1, 1, 2, 2, 2, 3, 3 };
        csc = p_csc[channels - 1];
        stream_count = channels - csc;

        static const uint8_t maps[6][7] = {
            { 2,1 },
            { 1,2,3 },
            { 4,1,2,3 },
            { 4,1,2,3,5 },
            { 4,1,2,3,5,6 },
            { 6,1,2,3,4,5,7 },
        };
        if (channels > 2)
            memcpy(&h.stream_map[1], maps[channels-3], channels - 1);
    } else if (ccc == 0x81) {
        if (len < 4)
            goto explicit_config_too_short;

        channels = p[2];
        mapping = p[3];
        csc = 0;
        if (mapping) {
            bs_t s;
            bs_init(&s, &p[4], len - 4);
            stream_count = 1;
            if (channels) {
                int bits = vlc_ceil_log2(channels);
                if (s.i_left < bits)
                    goto explicit_config_too_short;
                stream_count = bs_read(&s, bits) + 1;
                bits = vlc_ceil_log2(stream_count + 1);
                if (s.i_left < bits)
                    goto explicit_config_too_short;
                csc = bs_read(&s, bits);
            }
            int channel_bits = vlc_ceil_log2(stream_count + csc + 1);
            if (s.i_left < channels * channel_bits)
                goto explicit_config_too_short;

            unsigned char silence = (1U << (stream_count + csc + 1)) - 1;
            for (int i = 0; i < channels; i++) {
                unsigned char m = bs_read(&s, channel_bits);
                if (m == silence)
                    m = 0xff;
                h.stream_map[i] = m;
            }
        }
    } else if (ccc >= 0x80 && ccc <= 0x88) {
        channels = ccc - 0x80;
        if (channels)
            mapping = 1;
        else {
            mapping = 255;
            channels = 2; // dual mono
        }
        csc = 0;
        stream_count = channels;
    } else {
        msg_Err(demux, "Opus channel configuration 0x%.2x is reserved", ccc);
    }

    if (!channels) {
        msg_Err(demux, "Opus channel configuration 0x%.2x not supported yet", p[1]);
        return;
    }

    opus_prepare_header(channels, 0, &h);
    h.preskip = 0;
    h.input_sample_rate = 48000;
    h.nb_coupled = csc;
    h.nb_streams = channels - csc;
    h.channel_mapping = mapping;

    if (h.channels) {
        uint8_t *p_extra = NULL;
        int i_extra = 0;
        opus_write_header(&p_extra, &i_extra, &h, NULL /* FIXME */);
        if (p_extra) {
            es_format_Change(p_fmt, AUDIO_ES, VLC_CODEC_OPUS);
            p_fmt->p_extra = p_extra;
            p_fmt->i_extra = i_extra;
            p_fmt->audio.i_channels = h.channels;
            p_fmt->audio.i_rate = 48000;
        }
    }

    return;

explicit_config_too_short:
    msg_Err(demux, "Opus descriptor too short");
}

static void PMTSetupEs0x02( ts_es_t *p_es,
                            const dvbpsi_pmt_es_t *p_dvbpsies )
{
    dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x02 );
    if( p_dr )
    {
        /* sample: wcbs.ts */
        const dvbpsi_vstream_dr_t *p_vdr = dvbpsi_DecodeVStreamDr( p_dr );
        if( p_vdr )
        {
            if( p_vdr->i_frame_rate_code > 1 && p_vdr->i_frame_rate_code < 9 &&
                !p_vdr->b_multiple_frame_rate )
            {
                static const int code_to_frame_rate[8][2] =
                {
                    { 24000, 1001 }, { 24, 1 }, { 25, 1 },       { 30000, 1001 },
                    { 30, 1 },       { 50, 1 }, { 60000, 1001 }, { 60, 1 },
                };
                p_es->fmt.video.i_frame_rate = code_to_frame_rate[p_vdr->i_frame_rate_code - 1][0];
                p_es->fmt.video.i_frame_rate_base = code_to_frame_rate[p_vdr->i_frame_rate_code - 1][1];
            }
            if( !p_vdr->b_mpeg2 && p_es->fmt.i_codec == VLC_CODEC_MPGV )
                p_es->fmt.i_original_fourcc = VLC_CODEC_MP1V;
        }
    }

    /* MPEG2_stereoscopic_video_format_descriptor */
    p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x34 );
    if( p_dr && p_dr->i_length > 0 && (p_dr->p_data[0] & 0x80) )
    {
        video_multiview_mode_t mode;
        switch( p_dr->p_data[0] & 0x7F )
        {
            case 0x03:
                mode = MULTIVIEW_STEREO_SBS; break;
            case 0x04:
                mode = MULTIVIEW_STEREO_TB; break;
            case 0x08:
            default:
                mode = MULTIVIEW_2D; break;
        }
        p_es->fmt.video.multiview_mode = mode;
    }
}

static void PMTSetupEs0x05PrivateData( demux_t *p_demux, ts_es_t *p_es,
                                       const dvbpsi_pmt_es_t *p_dvbpsies )
{
    VLC_UNUSED(p_es);
    if( p_demux->p_sys->standard == TS_STANDARD_DVB ||
        p_demux->p_sys->standard == TS_STANDARD_AUTO )
    {
        dvbpsi_descriptor_t *p_ait_dr = PMTEsFindDescriptor( p_dvbpsies, 0x6F );
        if( p_ait_dr )
        {
            uint8_t *p_data = p_ait_dr->p_data;
            for( uint8_t i_data = p_ait_dr->i_length; i_data >= 3; i_data -= 3, p_data += 3 )
            {
                uint16_t i_app_type = ((p_data[0] & 0x7F) << 8) | p_data[1];
                msg_Dbg( p_demux, "      - Application type 0x%"PRIx16" version %"PRIu8,
                         i_app_type, p_data[2] & 0x1F);
            }
        }
    }
}

static void PMTSetupEs0x06( demux_t *p_demux, ts_stream_t *p_pes,
                            const dvbpsi_pmt_es_t *p_dvbpsies )
{
    es_format_t *p_fmt = &p_pes->p_es->fmt;
    dvbpsi_descriptor_t *p_subs_dr = PMTEsFindDescriptor( p_dvbpsies, 0x59 );
    dvbpsi_descriptor_t *desc;
    if( PMTEsHasRegistration( p_demux, p_dvbpsies, "EAC3" ) ||
        PMTEsFindDescriptor( p_dvbpsies, 0x7a ) )
    {
        /* DVB with stream_type 0x06 (ETS EN 300 468) */
        es_format_Change( p_fmt, AUDIO_ES, VLC_CODEC_EAC3 );
    }
    else if( PMTEsHasRegistration( p_demux, p_dvbpsies, "AC-3" ) ||
             PMTEsFindDescriptor( p_dvbpsies, 0x6a ) ||
             PMTEsFindDescriptor( p_dvbpsies, 0x81 ) ) /* AC-3 channel (also in EAC3) */
    {
        es_format_Change( p_fmt, AUDIO_ES, VLC_CODEC_A52 );
    }
    else if( (desc = PMTEsFindDescriptor( p_dvbpsies, 0x7f ) ) &&
             desc->i_length >= 2 && desc->p_data[0] == 0x80 &&
              PMTEsHasRegistration(p_demux, p_dvbpsies, "Opus"))
    {
        OpusSetup(p_demux, desc->p_data, desc->i_length, p_fmt);
    }
    else if( PMTEsHasRegistration( p_demux, p_dvbpsies, "DTS1" ) || /* 512 Bpf */
             PMTEsHasRegistration( p_demux, p_dvbpsies, "DTS2" ) || /* 1024 Bpf */
             PMTEsHasRegistration( p_demux, p_dvbpsies, "DTS3" ) || /* 2048 Bpf */
             PMTEsFindDescriptor( p_dvbpsies, 0x73 ) )
    {
        /*registration descriptor(ETSI TS 101 154 Annex F)*/
        es_format_Change( p_fmt, AUDIO_ES, VLC_CODEC_DTS );
    }
    else if( PMTEsHasRegistration( p_demux, p_dvbpsies, "BSSD" ) && !p_subs_dr )
    {
        /* BSSD is AES3 DATA, but could also be subtitles
         * we need to check for secondary descriptor then s*/
        es_format_Change( p_fmt, AUDIO_ES, VLC_CODEC_302M );
        p_fmt->b_packetized = true;
    }
    else if( PMTEsHasRegistration( p_demux, p_dvbpsies, "HEVC" ) )
    {
        es_format_Change( p_fmt, VIDEO_ES, VLC_CODEC_HEVC );
    }
    else if ( p_demux->p_sys->standard == TS_STANDARD_ARIB )
    {
        /* Lookup our data component descriptor first ARIB STD B10 6.4 */
        dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0xFD );
        /* and check that it maps to something ARIB STD B14 Table 5.1/5.2 */
        if ( p_dr && p_dr->i_length >= 2 )
        {
            /* See STD-B10 Annex J, table J-1 mappings */
            const uint16_t i_data_component_id = GetWBE(p_dr->p_data);
            if( i_data_component_id == 0x0008 &&
                PMTEsHasComponentTagBetween( p_dvbpsies, 0x30, 0x37 ) )
            {
                es_format_Change( p_fmt, SPU_ES, VLC_CODEC_ARIB_A );
                p_fmt->psz_language = strndup ( "jpn", 3 );
                p_fmt->psz_description = strdup( _("ARIB subtitles") );
            }
            else if( i_data_component_id == 0x0012 &&
                     PMTEsHasComponentTagBetween( p_dvbpsies, 0x87, 0x88 ) )
            {
                es_format_Change( p_fmt, SPU_ES, VLC_CODEC_ARIB_C );
                p_fmt->psz_language = strndup ( "jpn", 3 );
                p_fmt->psz_description = strdup( _("ARIB subtitles") );
            }
        }
    }
    else
    {
        /* Subtitle/Teletext/VBI fallbacks */
        dvbpsi_subtitling_dr_t *p_sub;
        if( p_subs_dr && ( p_sub = dvbpsi_DecodeSubtitlingDr( p_subs_dr ) ) )
        {
            for( int i = 0; i < p_sub->i_subtitles_number; i++ )
            {
                if( p_fmt->i_cat != UNKNOWN_ES )
                    break;

                switch( p_sub->p_subtitle[i].i_subtitling_type )
                {
                case 0x01: /* EBU Teletext subtitles */
                case 0x02: /* Associated EBU Teletext */
                case 0x03: /* VBI data */
                    PMTSetupEsTeletext( p_demux, p_pes, p_dvbpsies );
                    break;
                case 0x10: /* DVB Subtitle (normal) with no monitor AR critical */
                case 0x11: /*                 ...   on 4:3 AR monitor */
                case 0x12: /*                 ...   on 16:9 AR monitor */
                case 0x13: /*                 ...   on 2.21:1 AR monitor */
                case 0x14: /*                 ...   for display on a high definition monitor */
                case 0x20: /* DVB Subtitle (impaired) with no monitor AR critical */
                case 0x21: /*                 ...   on 4:3 AR monitor */
                case 0x22: /*                 ...   on 16:9 AR monitor */
                case 0x23: /*                 ...   on 2.21:1 AR monitor */
                case 0x24: /*                 ...   for display on a high definition monitor */
                    PMTSetupEsDvbSubtitle( p_demux, p_pes, p_dvbpsies );
                    break;
                default:
                    msg_Err( p_demux, "Unrecognized DVB subtitle type (0x%x)",
                             p_sub->p_subtitle[i].i_subtitling_type );
                    break;
                }
            }
        }

        if( p_fmt->i_cat == UNKNOWN_ES &&
            ( PMTEsFindDescriptor( p_dvbpsies, 0x45 ) ||  /* VBI Data descriptor */
              PMTEsFindDescriptor( p_dvbpsies, 0x46 ) ||  /* VBI Teletext descriptor */
              PMTEsFindDescriptor( p_dvbpsies, 0x56 ) ) ) /* EBU Teletext descriptor */
        {
            /* Teletext/VBI */
            PMTSetupEsTeletext( p_demux, p_pes, p_dvbpsies );
        }
    }

    /* FIXME is it useful ? */
    if( PMTEsFindDescriptor( p_dvbpsies, 0x52 ) )
    {
        dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x52 );
        dvbpsi_stream_identifier_dr_t *p_si = dvbpsi_DecodeStreamIdentifierDr( p_dr );

        msg_Dbg( p_demux, "    * Stream Component Identifier: %d", p_si->i_component_tag );
    }
}

static void PMTSetupEs0xEA( demux_t *p_demux, ts_es_t *p_es,
                           const dvbpsi_pmt_es_t *p_dvbpsies )
{
    /* Registration Descriptor */
    if( !PMTEsHasRegistration( p_demux, p_dvbpsies, "VC-1" ) )
    {
        msg_Err( p_demux, "Registration descriptor not found or invalid" );
        return;
    }

    /* registration descriptor for VC-1 (SMPTE rp227) */
    es_format_Change( &p_es->fmt, VIDEO_ES, VLC_CODEC_VC1 );

    /* XXX With Simple and Main profile the SEQUENCE
     * header is modified: video width and height are
     * inserted just after the start code as 2 int16_t
     * The packetizer will take care of that. */
}

static void PMTSetupEs0xD1( demux_t *p_demux, ts_es_t *p_es,
                           const dvbpsi_pmt_es_t *p_dvbpsies )
{
    /* Registration Descriptor */
    if( !PMTEsHasRegistration( p_demux, p_dvbpsies, "drac" ) )
    {
        msg_Err( p_demux, "Registration descriptor not found or invalid" );
        return;
    }

    /* registration descriptor for Dirac
     * (backwards compatable with VC-2 (SMPTE Sxxxx:2008)) */
    es_format_Change( &p_es->fmt, VIDEO_ES, VLC_CODEC_DIRAC );
}

static void PMTSetupEs0xA0( demux_t *p_demux, ts_es_t *p_es,
                           const dvbpsi_pmt_es_t *p_dvbpsies )
{
    /* MSCODEC sent by vlc */
    dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0xa0 );
    if( !p_dr || p_dr->i_length < 10 )
    {
        msg_Warn( p_demux,
                  "private MSCODEC (vlc) without bih private descriptor" );
        return;
    }

    es_format_t *p_fmt = &p_es->fmt;
    es_format_Change( &p_es->fmt, VIDEO_ES,
                      VLC_FOURCC( p_dr->p_data[0], p_dr->p_data[1],
                                 p_dr->p_data[2], p_dr->p_data[3] ) );
    p_fmt->video.i_width = GetWBE( &p_dr->p_data[4] );
    p_fmt->video.i_height = GetWBE( &p_dr->p_data[6] );
    p_fmt->video.i_visible_width = p_fmt->video.i_width;
    p_fmt->video.i_visible_height = p_fmt->video.i_height;
    p_fmt->i_extra = GetWBE( &p_dr->p_data[8] );

    if( p_fmt->i_extra > 0 )
    {
        p_fmt->p_extra = malloc( p_fmt->i_extra );
        if( p_fmt->p_extra )
            memcpy( p_fmt->p_extra, &p_dr->p_data[10],
                    __MIN( p_fmt->i_extra, p_dr->i_length - 10 ) );
        else
            p_fmt->i_extra = 0;
    }
    /* For such stream we will gather them ourself and don't launch a
     * packetizer.
     * Yes it's ugly but it's the only way to have DIV3 working */
    p_fmt->b_packetized = true;
}

static void PMTSetupEs0x83( const dvbpsi_pmt_t *p_pmt, ts_es_t *p_es, int i_pid )
{
    /* WiDi broadcasts without registration on PMT 0x1, PCR 0x1000 and
     * with audio track pid being 0x1100..0x11FF */
    if ( p_pmt->i_program_number == 0x1 &&
         p_pmt->i_pcr_pid == 0x1000 &&
        ( i_pid >> 8 ) == 0x11 )
    {
        /* Not enough ? might contain 0x83 private descriptor, 2 bytes 0x473F */
        es_format_Change( &p_es->fmt, AUDIO_ES, VLC_CODEC_WIDI_LPCM );
    }
    else
        es_format_Change( &p_es->fmt, AUDIO_ES, VLC_CODEC_DVD_LPCM );
}

static bool PMTSetupEsHDMV( demux_t *p_demux, ts_es_t *p_es,
                            const dvbpsi_pmt_es_t *p_dvbpsies )
{
    es_format_t *p_fmt = &p_es->fmt;

    /* Blu-Ray mapping */
    switch( p_dvbpsies->i_type )
    {
    case 0x80:
        es_format_Change( p_fmt, AUDIO_ES, VLC_CODEC_BD_LPCM );
        break;
    case 0x81:
        es_format_Change( p_fmt, AUDIO_ES, VLC_CODEC_A52 );
        break;
    case 0x85: /* DTS-HD High resolution audio */
    case 0x86: /* DTS-HD Master audio */
        es_format_Change( p_fmt, AUDIO_ES, VLC_CODEC_DTS );
        p_fmt->i_profile = PROFILE_DTS_HD;
        break;
    case 0x82:
    case 0xA2: /* Secondary DTS audio */
        es_format_Change( p_fmt, AUDIO_ES, VLC_CODEC_DTS );
        break;

    case 0x83: /* TrueHD AC3 */
        es_format_Change( p_fmt, AUDIO_ES, VLC_CODEC_TRUEHD );
        break;

    case 0x84: /* E-AC3 */
    case 0xA1: /* Secondary E-AC3 */
        es_format_Change( p_fmt, AUDIO_ES, VLC_CODEC_EAC3 );
        break;
    case 0x90: /* Presentation graphics */
        es_format_Change( p_fmt, SPU_ES, VLC_CODEC_BD_PG );
        break;
    case 0x91: /* Interactive graphics */
        return false;
    case 0x92: /* Subtitle */
        es_format_Change( p_fmt, SPU_ES, VLC_CODEC_BD_TEXT );
        break;
    case 0xEA:
        es_format_Change( p_fmt, VIDEO_ES, VLC_CODEC_VC1 );
        break;
    default:
        msg_Info( p_demux, "HDMV registration not implemented for pid 0x%x type 0x%x",
                  p_dvbpsies->i_pid, p_dvbpsies->i_type );
        return false;
    }
    return true;
}

static bool PMTSetupEsRegistration( demux_t *p_demux, ts_es_t *p_es,
                                    const dvbpsi_pmt_es_t *p_dvbpsies )
{
    static const struct
    {
        char                      psz_tag[5];
        enum es_format_category_e i_cat;
        vlc_fourcc_t              i_codec;
    } p_regs[] = {
        { "AC-3", AUDIO_ES, VLC_CODEC_A52   },
        { "EAC3", AUDIO_ES, VLC_CODEC_EAC3  },
        { "DTS1", AUDIO_ES, VLC_CODEC_DTS   },
        { "DTS2", AUDIO_ES, VLC_CODEC_DTS   },
        { "DTS3", AUDIO_ES, VLC_CODEC_DTS   },
        { "BSSD", AUDIO_ES, VLC_CODEC_302M  },
        { "VC-1", VIDEO_ES, VLC_CODEC_VC1   },
        { "drac", VIDEO_ES, VLC_CODEC_DIRAC },
        { "", UNKNOWN_ES, 0 }
    };
    es_format_t *p_fmt = &p_es->fmt;

    for( int i = 0; p_regs[i].i_cat != UNKNOWN_ES; i++ )
    {
        if( PMTEsHasRegistration( p_demux, p_dvbpsies, p_regs[i].psz_tag ) )
        {
            es_format_Change( p_fmt, p_regs[i].i_cat, p_regs[i].i_codec );

            /* System A AC3 extension, see ATSC A/52 Annex G.2 */
            if ( p_regs[i].i_codec == VLC_CODEC_A52 && p_dvbpsies->i_type == 0x87 )
                p_fmt->i_codec = VLC_CODEC_EAC3;

            return true;
        }
    }
    return false;
}

static char *GetIso639AudioTypeDesc( uint8_t type )
{
    static const char *audio_type[] = {
        /* "Main audio", */
        N_("clean effects"),
        N_("hearing impaired"),
        N_("visual impaired commentary"),
    };

    if ( type == 0 || type >= ARRAY_SIZE(audio_type) )
        return NULL;

    return strdup( audio_type[ --type ] );
}

static void PMTParseEsIso639( demux_t *p_demux, ts_es_t *p_es,
                              const dvbpsi_pmt_es_t *p_dvbpsies )
{
    /* get language descriptor */
    dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x0a );

    if( !p_dr )
        return;

    dvbpsi_iso639_dr_t *p_decoded = dvbpsi_DecodeISO639Dr( p_dr );
    if( !p_decoded )
    {
        msg_Err( p_demux, "      Failed to decode a ISO 639 descriptor" );
        return;
    }

    p_es->fmt.psz_language = malloc( 4 );
    if( p_es->fmt.psz_language )
    {
        memcpy( p_es->fmt.psz_language, p_decoded->code[0].iso_639_code, 3 );
        p_es->fmt.psz_language[3] = 0;
        msg_Dbg( p_demux, "      found language: %s", p_es->fmt.psz_language);
    }

    uint8_t type = p_decoded->code[0].i_audio_type;
    p_es->fmt.psz_description = GetIso639AudioTypeDesc( type );
    if (type == 0x00) /* Undefined */
        p_es->fmt.i_priority = ES_PRIORITY_SELECTABLE_MIN + 1; // prioritize normal audio tracks

    p_es->fmt.i_extra_languages = p_decoded->i_code_count-1;
    if( p_es->fmt.i_extra_languages > 0 )
        p_es->fmt.p_extra_languages =
            malloc( sizeof(*p_es->fmt.p_extra_languages) *
                    p_es->fmt.i_extra_languages );
    if( p_es->fmt.p_extra_languages )
    {
        for( unsigned i = 0; i < p_es->fmt.i_extra_languages; i++ )
        {
            extra_languages_t *p_lang = &p_es->fmt.p_extra_languages[i];
            if( (p_lang->psz_language = malloc(4)) )
            {
                memcpy( p_lang->psz_language, p_decoded->code[i+1].iso_639_code, 3 );
                p_lang->psz_language[3] = '\0';
            }
            p_lang->psz_description = GetIso639AudioTypeDesc( p_decoded->code[i].i_audio_type );
        }
    }
}

static void PIDFillFormat( demux_t *p_demux, ts_stream_t *p_pes,
                           int i_stream_type, ts_transport_type_t *p_datatype )
{
    es_format_t *fmt = &p_pes->p_es->fmt;
    switch( i_stream_type )
    {
    case 0x01:  /* MPEG-1 video */
        es_format_Change( fmt, VIDEO_ES, VLC_CODEC_MPGV );
        fmt->i_original_fourcc = VLC_CODEC_MP1V;
        break;
    case 0x02:  /* MPEG-2 video */
    case 0x80:  /* MPEG-2 MOTO video */
        es_format_Change( fmt, VIDEO_ES, VLC_CODEC_MPGV );
        break;
    case 0x03:  /* MPEG-1 audio */
    case 0x04:  /* MPEG-2 audio */
        es_format_Change( fmt, AUDIO_ES, VLC_CODEC_MPGA );
        break;
    case 0x0f:  /* ISO/IEC 13818-7 Audio with ADTS transport syntax */
        es_format_Change( fmt, AUDIO_ES, VLC_CODEC_MP4A );
        fmt->i_original_fourcc = VLC_FOURCC('A','D','T','S');
        break;
    case 0x10:  /* MPEG4 (video) */
        es_format_Change( fmt, VIDEO_ES, VLC_CODEC_MP4V );
        break;
    case 0x11:  /* MPEG4 (audio) LATM */
        es_format_Change( fmt, AUDIO_ES, VLC_CODEC_MP4A );
        fmt->i_original_fourcc = VLC_FOURCC('L','A','T','M');
        break;
    case 0x1B:  /* H264 <- check transport syntax/needed descriptor */
        es_format_Change( fmt, VIDEO_ES, VLC_CODEC_H264 );
        break;
    case 0x1C:  /* ISO/IEC 14496-3 Audio, without using any additional
                   transport syntax, such as DST, ALS and SLS */
        es_format_Change( fmt, AUDIO_ES, VLC_CODEC_MP4A );
        break;
    case 0x24:  /* HEVC */
        es_format_Change( fmt, VIDEO_ES, VLC_CODEC_HEVC );
        break;
    case 0x42:  /* CAVS (Chinese AVS) */
        es_format_Change( fmt, VIDEO_ES, VLC_CODEC_CAVS );
        break;

    case 0x81:  /* A52 (audio) */
        es_format_Change( fmt, AUDIO_ES, VLC_CODEC_A52 );
        break;
    case 0x82:  /* SCTE-27 (sub) */
        es_format_Change( fmt, SPU_ES, VLC_CODEC_SCTE_27 );
        *p_datatype = TS_TRANSPORT_SECTIONS;
        ts_sections_processor_Add( p_demux, &p_pes->p_sections_proc, 0xC6, 0x00,
                                   SCTE27_Section_Callback, p_pes );
        break;
    case 0x84:  /* SDDS (audio) */
        es_format_Change( fmt, AUDIO_ES, VLC_CODEC_SDDS );
        break;
    case 0x85:  /* DTS (audio) FIXME: HDMV Only ? */
        es_format_Change( fmt, AUDIO_ES, VLC_CODEC_DTS );
        break;
    case 0x87: /* E-AC3, ATSC */
        es_format_Change( fmt, AUDIO_ES, VLC_CODEC_EAC3 );
        break;
    case 0x8a: /* DTS (audio) */
        es_format_Change( fmt, AUDIO_ES, VLC_CODEC_DTS );
        break;
    case 0x91:  /* A52 vls (audio) */
        es_format_Change( fmt, AUDIO_ES, VLC_FOURCC( 'a', '5', '2', 'b' ) );
        break;
    case 0x92:  /* DVD_SPU vls (sub) */
        es_format_Change( fmt, SPU_ES, VLC_FOURCC( 's', 'p', 'u', 'b' ) );
        break;

    case 0x94:  /* SDDS (audio) */
        es_format_Change( fmt, AUDIO_ES, VLC_FOURCC( 's', 'd', 'd', 'b' ) );
        break;

    case 0xa0:  /* MSCODEC vlc (video) (fixed later) */
        es_format_Change( fmt, UNKNOWN_ES, 0 );
        break;

    case 0x06:  /* PES_PRIVATE  (fixed later) */
    case 0x12:  /* MPEG-4 generic (sub/scene/...) (fixed later) */
    case 0xEA:  /* Privately managed ES (VC-1) (fixed later */
    default:
        es_format_Change( fmt, UNKNOWN_ES, 0 );
        break;
    }
}

static void FillPESFromDvbpsiES( demux_t *p_demux,
                                 const dvbpsi_pmt_t *p_dvbpsipmt,
                                 const dvbpsi_pmt_es_t *p_dvbpsies,
                                 ts_pmt_registration_type_t registration_type,
                                 const ts_pmt_t *p_pmt,
                                 ts_stream_t *p_pes )
{
    ts_transport_type_t type_change = TS_TRANSPORT_PES;
    PIDFillFormat( p_demux, p_pes, p_dvbpsies->i_type, &type_change );

    p_pes->i_stream_type = p_dvbpsies->i_type;

    bool b_registration_applied = false;
    if ( p_dvbpsies->i_type >= 0x80 ) /* non standard, extensions */
    {
        if ( registration_type == TS_PMT_REGISTRATION_BLURAY )
        {
            if (( b_registration_applied = PMTSetupEsHDMV( p_demux, p_pes->p_es, p_dvbpsies ) ))
                msg_Dbg( p_demux, "    + HDMV registration applied to pid %d type 0x%x",
                         p_dvbpsies->i_pid, p_dvbpsies->i_type );
        }
        else
        {
            if (( b_registration_applied = PMTSetupEsRegistration( p_demux, p_pes->p_es, p_dvbpsies ) ))
                msg_Dbg( p_demux, "    + registration applied to pid %d type 0x%x",
                    p_dvbpsies->i_pid, p_dvbpsies->i_type );
        }
    }

    if ( !b_registration_applied )
    {
        p_pes->transport = type_change; /* Only change type if registration has not changed meaning */

        switch( p_dvbpsies->i_type )
        {
        case 0x02:
            PMTSetupEs0x02( p_pes->p_es, p_dvbpsies );
            break;
        case 0x05: /* Private data in sections */
            p_pes->transport = TS_TRANSPORT_SECTIONS;
            PMTSetupEs0x05PrivateData( p_demux, p_pes->p_es, p_dvbpsies );
            break;
        case 0x06:
            /* Handle PES private data */
            PMTSetupEs0x06( p_demux, p_pes, p_dvbpsies );
            break;
        case 0x0a: /* DSM-CC */
        case 0x0b:
        case 0x0c:
        case 0x0d:
            p_pes->transport = TS_TRANSPORT_IGNORE;
            break;
        /* All other private or reserved types */
        case 0x13: /* SL in sections */
            p_pes->transport = TS_TRANSPORT_SECTIONS;
            /* fallthrough */
        case 0x0f:
        case 0x10:
        case 0x11:
        case 0x12:
            SetupISO14496Descriptors( p_demux, p_pes, p_pmt, p_dvbpsies );
            break;
        case 0x15:
            SetupMetadataDescriptors( p_demux, p_pes, p_dvbpsies );
            break;
        case 0x1b:
            SetupAVCDescriptors( p_demux, p_pes->p_es, p_dvbpsies );
            break;
        case 0x21:
            SetupJ2KDescriptors( p_demux, p_pes->p_es, p_dvbpsies );
            break;
        case 0x83:
            /* LPCM (audio) */
            PMTSetupEs0x83( p_dvbpsipmt, p_pes->p_es, p_dvbpsies->i_pid );
            break;
        case 0xa0:
            PMTSetupEs0xA0( p_demux, p_pes->p_es, p_dvbpsies );
            break;
        case 0xd1:
            PMTSetupEs0xD1( p_demux, p_pes->p_es, p_dvbpsies );
            break;
        case 0xEA:
            PMTSetupEs0xEA( p_demux, p_pes->p_es, p_dvbpsies );
        default:
            break;
        }
    }

    if( p_pes->p_es->fmt.i_cat == AUDIO_ES ||
        ( p_pes->p_es->fmt.i_cat == SPU_ES &&
          p_pes->p_es->fmt.i_codec != VLC_CODEC_DVBS &&
          p_pes->p_es->fmt.i_codec != VLC_CODEC_TELETEXT ) )
    {
        PMTParseEsIso639( p_demux, p_pes->p_es, p_dvbpsies );
    }

    if( p_pes->p_es->fmt.i_cat == AUDIO_ES )
    {
        SetupAudioExtendedDescriptors( p_demux, p_pes->p_es, p_dvbpsies );
    }

    /* Disable dolbyvision */
    if ( registration_type == TS_PMT_REGISTRATION_BLURAY &&
         p_dvbpsies->i_pid == 0x1015 &&
         PMTEsHasRegistration( p_demux, p_dvbpsies, "HDMV" ) )
        p_pes->p_es->fmt.i_priority = ES_PRIORITY_NOT_DEFAULTABLE;

    /* PES packets usually contain truncated frames */
    p_pes->p_es->fmt.b_packetized = false;

    /* Set Groups / ID */
    p_pes->p_es->fmt.i_group = p_dvbpsipmt->i_program_number;
    if( p_demux->p_sys->b_es_id_pid )
        p_pes->p_es->fmt.i_id = p_dvbpsies->i_pid;
}

static en50221_capmt_info_t * CreateCAPMTInfo( const dvbpsi_pmt_t *p_pmt )
{
    en50221_capmt_info_t *p_en = en50221_capmt_New( p_pmt->i_version,
                                                    p_pmt->i_program_number );
    if( unlikely(p_en == NULL) )
        return p_en;

    for( const dvbpsi_descriptor_t *p_dr = p_pmt->p_first_descriptor;
                                           p_dr; p_dr = p_dr->p_next )
    {
        if( p_dr->i_tag == 0x09 )
            en50221_capmt_AddCADescriptor( p_en, p_dr->p_data, p_dr->i_length );
    }

    for( const dvbpsi_pmt_es_t *p_es = p_pmt->p_first_es;
                                       p_es; p_es = p_es->p_next )
    {
        en50221_capmt_es_info_t *p_enes = en50221_capmt_EsAdd( p_en,
                                                               p_es->i_type,
                                                               p_es->i_pid );
        if( likely(p_enes) )
        {
            for( const dvbpsi_descriptor_t *p_dr = p_es->p_first_descriptor;
                                                   p_dr; p_dr = p_dr->p_next )
            {
                if( p_dr->i_tag == 0x09 )
                    en50221_capmt_AddESCADescriptor( p_enes, p_dr->p_data, p_dr->i_length );
            }
        }
    }

    return p_en;
}

static void PMTCallBack( void *data, dvbpsi_pmt_t *p_dvbpsipmt )
{
    demux_t      *p_demux = data;
    demux_sys_t  *p_sys = p_demux->p_sys;

    ts_pid_t     *pmtpid = NULL;
    ts_pmt_t     *p_pmt = NULL;

    msg_Dbg( p_demux, "PMTCallBack called for program %d", p_dvbpsipmt->i_program_number );

    if (unlikely(GetPID(p_sys, 0)->type != TYPE_PAT))
    {
        assert(GetPID(p_sys, 0)->type == TYPE_PAT);
        dvbpsi_pmt_delete(p_dvbpsipmt);
    }

    const ts_pat_t *p_pat = GetPID(p_sys, 0)->u.p_pat;

    /* First find this PMT declared in PAT */
    for( int i = 0; !pmtpid && i < p_pat->programs.i_size; i++ )
    {
        const int i_pmt_prgnumber = p_pat->programs.p_elems[i]->u.p_pmt->i_number;
        if( i_pmt_prgnumber != TS_USER_PMT_NUMBER &&
            i_pmt_prgnumber == p_dvbpsipmt->i_program_number )
        {
            pmtpid = p_pat->programs.p_elems[i];
            assert(pmtpid->type == TYPE_PMT);
            p_pmt = pmtpid->u.p_pmt;
        }
    }

    if( pmtpid == NULL )
    {
        msg_Warn( p_demux, "unreferenced program (broken stream)" );
        dvbpsi_pmt_delete(p_dvbpsipmt);
        return;
    }

    pmtpid->i_flags |= FLAG_SEEN;

    if( p_pmt->i_version != -1 &&
        ( !p_dvbpsipmt->b_current_next || p_pmt->i_version == p_dvbpsipmt->i_version ) )
    {
        dvbpsi_pmt_delete( p_dvbpsipmt );
        return;
    }

    /* Save old es array */
    DECL_ARRAY(ts_pid_t *) pid_to_decref;
    pid_to_decref.i_alloc = p_pmt->e_streams.i_alloc;
    pid_to_decref.i_size = p_pmt->e_streams.i_size;
    pid_to_decref.p_elems = p_pmt->e_streams.p_elems;
    if( p_pmt->p_atsc_si_basepid )
        ARRAY_APPEND( pid_to_decref, p_pmt->p_atsc_si_basepid );
    if( p_pmt->p_si_sdt_pid )
        ARRAY_APPEND( pid_to_decref, p_pmt->p_si_sdt_pid );
    ARRAY_INIT(p_pmt->e_streams);

    if( p_pmt->iod )
    {
        ODFree( p_pmt->iod );
        p_pmt->iod = NULL;
    }

    msg_Dbg( p_demux, "new PMT program number=%d version=%d pid_pcr=%d",
             p_dvbpsipmt->i_program_number, p_dvbpsipmt->i_version, p_dvbpsipmt->i_pcr_pid );
    p_pmt->i_pid_pcr = p_dvbpsipmt->i_pcr_pid;
    p_pmt->i_version = p_dvbpsipmt->i_version;

    if( ProgramIsSelected( p_sys, p_pmt->i_number ) )
        SetPIDFilter( p_sys, GetPID(p_sys, p_pmt->i_pid_pcr), true ); /* Set demux filter */

    /* Parse PMT descriptors */
    ts_pmt_registration_type_t registration_type = TS_PMT_REGISTRATION_NONE;
    ParsePMTRegistrations( p_demux, p_dvbpsipmt->p_first_descriptor, p_pmt, &registration_type );

    if( p_sys->standard == TS_STANDARD_AUTO )
    {
        switch( registration_type )
        {
            case TS_PMT_REGISTRATION_BLURAY:
                TsChangeStandard( p_sys, TS_STANDARD_MPEG );
                break;
            case TS_PMT_REGISTRATION_ARIB:
                TsChangeStandard( p_sys, TS_STANDARD_ARIB );
                break;
            case TS_PMT_REGISTRATION_ATSC:
                TsChangeStandard( p_sys, TS_STANDARD_ATSC );
                break;
            default:
                if(SEEN(GetPID(p_sys, ATSC_BASE_PID)))
                {
                    TsChangeStandard( p_sys, TS_STANDARD_ATSC );
                }
                else
                {
                    /* Probe using ES */
                    p_sys->standard = ProbePMTStandard( p_dvbpsipmt );
                }
                break;
        }
    }

    /* Private descriptors depends on standard */
    ParsePMTPrivateRegistrations( p_demux, p_dvbpsipmt->p_first_descriptor, p_pmt, p_sys->standard );

    dvbpsi_pmt_es_t *p_dvbpsies;
    for( p_dvbpsies = p_dvbpsipmt->p_first_es; p_dvbpsies != NULL; p_dvbpsies = p_dvbpsies->p_next )
    {
        ts_pid_t *pespid = GetPID(p_sys, p_dvbpsies->i_pid);
        if ( pespid->type != TYPE_STREAM && pespid->type != TYPE_FREE )
        {
            msg_Warn( p_demux, " * PMT wants to create PES on pid %d used by non PES", pespid->i_pid );
            continue;
        }

        char const * psz_typedesc = ISO13818_1_Get_StreamType_Description( p_dvbpsies->i_type );

        msg_Dbg( p_demux, "  * pid=%d type=0x%x %s",
                 p_dvbpsies->i_pid, p_dvbpsies->i_type, psz_typedesc );

        /* PMT element/component descriptors */
        for( dvbpsi_descriptor_t *p_dr = p_dvbpsies->p_first_descriptor;
             p_dr != NULL; p_dr = p_dr->p_next )
        {
            const char *psz_desc = NULL;
            if( registration_type == TS_PMT_REGISTRATION_ARIB )
                psz_desc = ARIB_B10_Get_PMT_Descriptor_Description( p_dr->i_tag );

            if( psz_desc )
                msg_Dbg( p_demux, "    - ES descriptor %s 0x%x", psz_desc, p_dr->i_tag );
            else
                msg_Dbg( p_demux, "    - ES descriptor tag 0x%x", p_dr->i_tag );
        }

        const bool b_pid_inuse = ( pespid->type == TYPE_STREAM );
        ts_stream_t *p_pes;

        if ( !PIDSetup( p_demux, TYPE_STREAM, pespid, pmtpid ) )
        {
            msg_Warn( p_demux, "  * pid=%d type=0x%x %s (skipped)",
                      p_dvbpsies->i_pid, p_dvbpsies->i_type, psz_typedesc );
            continue;
        }
        else
        {
            if( b_pid_inuse ) /* pes will point to a temp */
            {
                p_pes = ts_stream_New( p_demux, p_pmt );
                if( !p_pes )
                {
                    PIDRelease( p_demux, pespid );
                    continue;
                }
            }
            else  /* pes will point to the new one allocated from PIDSetup */
            {
                p_pes = pespid->u.p_stream;
            }
        }

        /* Add pid to the list of used ones in pmt */
        ARRAY_APPEND( p_pmt->e_streams, pespid );
        pespid->i_flags |= SEEN(GetPID(p_sys, p_dvbpsies->i_pid));

        /* Fill p_pes es and add extra es if any */
        FillPESFromDvbpsiES( p_demux, p_dvbpsipmt, p_dvbpsies,
                             registration_type, p_pmt, p_pes );

        /* Set description and debug */
        if( p_pes->p_es->fmt.i_cat == UNKNOWN_ES )
        {
            msg_Dbg( p_demux, "   => pid %d content is *unknown*",
                     p_dvbpsies->i_pid );
            p_pes->p_es->fmt.psz_description = strdup( psz_typedesc );
        }
        else
        {
            msg_Dbg( p_demux, "   => pid %d has now es fcc=%4.4s",
                     p_dvbpsies->i_pid, (char*)&p_pes->p_es->fmt.i_codec );
        }

        dvbpsi_descriptor_t *p_dr = PMTEsFindDescriptor( p_dvbpsies, 0x09 );
        if( p_dr && p_dr->i_length >= 2 )
        {
            msg_Dbg( p_demux, "    - ES descriptor : CA (0x9) SysID 0x%x",
                     (p_dr->p_data[0] << 8) | p_dr->p_data[1] );
        }

        const bool b_create_es = (p_pes->p_es->fmt.i_cat != UNKNOWN_ES);

        /* Now check and merge */
        if( b_pid_inuse ) /* We need to compare to the existing pes/es */
        {
            ts_es_t *p_existing_es = ts_stream_Find_es( pespid->u.p_stream, p_pmt );
            if( p_existing_es )
            {
                const es_format_t *ofmt = &p_existing_es->fmt;
                const es_format_t *nfmt = &p_pes->p_es->fmt;

                /* Check if we can avoid restarting that ES */
                bool b_canreuse = es_format_IsSimilar( ofmt, nfmt );

                /* Check codecs extra */
                b_canreuse = b_canreuse &&
                (
                    ofmt->i_extra == nfmt->i_extra &&
                    ( ofmt->i_extra == 0 ||
                      memcmp( ofmt->p_extra, nfmt->p_extra, nfmt->i_extra ) == 0 )
                );

                /* Tracks must have same language */
                b_canreuse = b_canreuse &&
                (
                    ( !!ofmt->psz_language == !!nfmt->psz_language ) &&
                    ( ofmt->psz_language == NULL ||
                      !strcmp( ofmt->psz_language, nfmt->psz_language ) )
                );

                /* Check is we have any subtitles */
                b_canreuse = b_canreuse &&
                ( ts_Count_es( p_pes->p_es->p_extraes, false, NULL ) ==
                  ts_Count_es( p_existing_es->p_extraes, false, NULL )
                );

                if( b_canreuse )
                {
                    /* Just keep using previous es */
                    ts_stream_Del( p_demux, p_pes );
                }
                else
                {
                    ts_es_t *p_new = ts_stream_Extract_es( p_pes, p_pmt );
                    ts_es_t *p_old = ts_stream_Extract_es( pespid->u.p_stream, p_pmt );
                    ts_stream_Add_es( pespid->u.p_stream, p_new, false );
                    assert(p_old == p_existing_es);
                    assert(ts_Count_es(p_pes->p_es, false, NULL) == 0);
                    ts_stream_Add_es( p_pes, p_old, false );
                    ts_stream_Del( p_demux, p_pes );
                }
            }
            else /* There was no es for that program on that pid, merge in */
            {
                assert(ts_Count_es(pespid->u.p_stream->p_es, false, NULL)); /* Used by another program */
                ts_es_t *p_new = ts_stream_Extract_es( p_pes, p_pmt );
                assert( p_new );
                ts_stream_Add_es( pespid->u.p_stream, p_new, false );
                ts_stream_Del( p_demux, p_pes );
            }
        }
        /* Nothing to do, pes is now just set */
        if( b_create_es )
            AddAndCreateES( p_demux, pespid, false );
    }

    /* Set CAM descrambling */
    if( ProgramIsSelected( p_sys, p_pmt->i_number ) )
    {
        en50221_capmt_info_t *p_en = CreateCAPMTInfo( p_dvbpsipmt );
        if( p_en )
        {
            /* DTV/CAM takes ownership of en50221_capmt_info_t on success */
            if( vlc_stream_Control( p_sys->stream, STREAM_SET_PRIVATE_ID_CA, p_en ) != VLC_SUCCESS )
            {
                en50221_capmt_Delete( p_en );
                if ( p_sys->standard == TS_STANDARD_ARIB && !p_sys->arib.b25stream )
                {
                    p_sys->arib.b25stream = vlc_stream_FilterNew( p_demux->s, "aribcam" );
                    p_sys->stream = ( p_sys->arib.b25stream ) ? p_sys->arib.b25stream : p_demux->s;
                }
            }
        }
    }

     /* Add arbitrary PID from here */
    if ( p_sys->standard == TS_STANDARD_ATSC )
    {
        ts_pid_t *atsc_base_pid = GetPID(p_sys, ATSC_BASE_PID);
        if ( PIDSetup( p_demux, TYPE_PSIP, atsc_base_pid, pmtpid ) )
        {
            ts_psip_t *p_psip = atsc_base_pid->u.p_psip;
            if( !ATSC_Attach_Dvbpsi_Base_Decoders( p_psip->handle, atsc_base_pid ) )
            {
                msg_Err( p_demux, "dvbpsi_atsc_AttachMGT/STT failed for program %d",
                         p_pmt->i_number );
                PIDRelease( p_demux, atsc_base_pid );
            }
            else
            {
                p_pmt->p_atsc_si_basepid = atsc_base_pid;
                SetPIDFilter( p_demux->p_sys, atsc_base_pid, true );
                msg_Dbg( p_demux, "  * pid=%d listening for MGT/STT", atsc_base_pid->i_pid );

                /* Set up EAS spu es */
                if( p_pmt->e_streams.i_size )
                {
                    ts_es_t *p_eas_es = ts_es_New( p_pmt );
                    if( likely(p_eas_es) )
                    {
                        es_format_Change( &p_eas_es->fmt, SPU_ES, VLC_CODEC_SCTE_18 );
                        p_eas_es->fmt.i_id = ATSC_BASE_PID;
                        p_eas_es->fmt.i_group = p_pmt->i_number;
                        p_eas_es->fmt.psz_description = strdup(SCTE18_DESCRIPTION);
                        if( p_psip->p_eas_es )
                        {
                            ts_es_t *p_next = p_psip->p_eas_es->p_next;
                            p_psip->p_eas_es->p_next = p_eas_es;
                            p_eas_es->p_next = p_next;
                        }
                        else
                        {
                            p_psip->p_eas_es = p_eas_es;
                        }
                        msg_Dbg( p_demux, "  * pid=%d listening for EAS events", ATSC_BASE_PID );
                    }
                }
            }
        }
        else if( atsc_base_pid->type != TYPE_FREE )
        {
            msg_Err( p_demux, "can't attach PSIP table handlers"
                              "on already in use ATSC base pid %d", ATSC_BASE_PID );
        }
    }
    else if( p_sys->standard != TS_STANDARD_MPEG && p_sys->standard != TS_STANDARD_TDMB )
    {
        ts_pid_t *p_sdt_pid = ts_pid_Get( &p_sys->pids, TS_SI_SDT_PID );
        if ( PIDSetup( p_demux, TYPE_SI, p_sdt_pid, pmtpid ) ) /* Create or incref SDT */
        {
            if( !ts_attach_SI_Tables_Decoders( p_sdt_pid ) )
            {
                msg_Err( p_demux, "Can't attach SI table decoders from program %d",
                         p_pmt->i_number );
                PIDRelease( p_demux, p_sdt_pid );
            }
            else
            {
                p_pmt->p_si_sdt_pid = p_sdt_pid;
                SetPIDFilter( p_demux->p_sys, p_sdt_pid, true );
                msg_Dbg( p_demux, "  * pid=%d listening for SDT", p_sdt_pid->i_pid );
            }
        }
        else if( p_sdt_pid->type != TYPE_FREE )
        {
            msg_Err( p_demux, "can't attach SI SDT table handler"
                              "on already in used pid %d (Not DVB ?)", p_sdt_pid->i_pid );
        }
    }

    /* Decref or clean now unused es */
    for( int i = 0; i < pid_to_decref.i_size; i++ )
        PIDRelease( p_demux, pid_to_decref.p_elems[i] );
    ARRAY_RESET( pid_to_decref );

    if( !p_sys->b_trust_pcr )
    {
        int i_cand = FindPCRCandidate( p_pmt );
        p_pmt->i_pid_pcr = i_cand;
        p_pmt->pcr.b_disable = true;
        msg_Warn( p_demux, "PCR not trusted for program %d, set up workaround using pid %d",
                  p_pmt->i_number, i_cand );
    }

    UpdatePESFilters( p_demux, p_demux->p_sys->seltype == PROGRAM_ALL );

    /* Probe Boundaries */
    if( p_sys->b_canfastseek && p_pmt->i_last_dts == -1 )
    {
        p_pmt->i_last_dts = 0;
        ProbeStart( p_demux, p_pmt->i_number );
        ProbeEnd( p_demux, p_pmt->i_number );
    }

    dvbpsi_pmt_delete( p_dvbpsipmt );
}

int UserPmt( demux_t *p_demux, const char *psz_fmt )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    char *psz_dup = strdup( psz_fmt );
    char *psz = psz_dup;
    int  i_number;

    if( !psz_dup )
        return VLC_ENOMEM;

    /* Parse PID */
    unsigned long i_pid = strtoul( psz, &psz, 0 );
    if( i_pid < 2 || i_pid >= 8192 )
        goto error;

    /* Parse optional program number */
    i_number = 0;
    if( *psz == ':' )
        i_number = strtol( &psz[1], &psz, 0 );

    /* */
    ts_pid_t *pmtpid = GetPID(p_sys, i_pid);

    msg_Dbg( p_demux, "user pmt specified (pid=%lu,number=%d)", i_pid, i_number );
    if ( !PIDSetup( p_demux, TYPE_PMT, pmtpid, GetPID(p_sys, 0) ) )
        goto error;

    /* Dummy PMT */
    ts_pmt_t *p_pmt = pmtpid->u.p_pmt;
    p_pmt->i_number   = i_number != 0 ? i_number : TS_USER_PMT_NUMBER;
    if( !dvbpsi_pmt_attach( p_pmt->handle,
                            ((i_number != TS_USER_PMT_NUMBER ? i_number : 1)),
                            PMTCallBack, p_demux ) )
    {
        PIDRelease( p_demux, pmtpid );
        goto error;
    }

    ARRAY_APPEND( GetPID(p_sys, 0)->u.p_pat->programs, pmtpid );

    psz = strchr( psz, '=' );
    if( psz )
        psz++;
    while( psz && *psz )
    {
        char *psz_next = strchr( psz, ',' );

        if( psz_next )
            *psz_next++ = '\0';

        i_pid = strtoul( psz, &psz, 0 );
        if( *psz != ':' || i_pid < 2 || i_pid >= 8192 )
            goto next;

        char *psz_opt = &psz[1];
        if( !strcmp( psz_opt, "pcr" ) )
        {
            p_pmt->i_pid_pcr = i_pid;
        }
        else if( GetPID(p_sys, i_pid)->type == TYPE_FREE )
        {
            ts_pid_t *pid = GetPID(p_sys, i_pid);

            char *psz_arg = strchr( psz_opt, '=' );
            if( psz_arg )
                *psz_arg++ = '\0';

            if ( !PIDSetup( p_demux, TYPE_STREAM, pid, pmtpid ) )
                continue;

            ARRAY_APPEND( p_pmt->e_streams, pid );

            if( p_pmt->i_pid_pcr <= 0 )
                p_pmt->i_pid_pcr = i_pid;

            es_format_t *fmt = &pid->u.p_stream->p_es->fmt;

            if( psz_arg && strlen( psz_arg ) == 4 )
            {
                const vlc_fourcc_t i_codec = VLC_FOURCC( psz_arg[0], psz_arg[1],
                                                         psz_arg[2], psz_arg[3] );
                int i_cat = UNKNOWN_ES;

                if( !strcmp( psz_opt, "video" ) )
                    i_cat = VIDEO_ES;
                else if( !strcmp( psz_opt, "audio" ) )
                    i_cat = AUDIO_ES;
                else if( !strcmp( psz_opt, "spu" ) )
                    i_cat = SPU_ES;

                es_format_Change( fmt, i_cat, i_codec );
                fmt->b_packetized = false;
            }
            else
            {
                const int i_stream_type = strtol( psz_opt, NULL, 0 );
                PIDFillFormat( p_demux, pid->u.p_stream, i_stream_type, &pid->u.p_stream->transport );
            }

            fmt->i_group = i_number;
            if( p_sys->b_es_id_pid )
                fmt->i_id = i_pid;

            if( fmt->i_cat != UNKNOWN_ES )
            {
                msg_Dbg( p_demux, "  * es pid=%lu fcc=%4.4s", i_pid,
                         (char*)&fmt->i_codec );
                pid->u.p_stream->p_es->id = es_out_Add( p_demux->out, fmt );
                p_sys->i_pmt_es++;
            }
        }

    next:
        psz = psz_next;
    }

    p_sys->b_user_pmt = true;
    free( psz_dup );
    return VLC_SUCCESS;

error:
    free( psz_dup );
    return VLC_EGENERIC;
}

bool ts_psi_PAT_Attach( ts_pid_t *patpid, void *cbdata )
{
    if( unlikely(patpid->type != TYPE_PAT || patpid->i_pid != TS_PSI_PAT_PID) )
        return false;
    return dvbpsi_pat_attach( patpid->u.p_pat->handle, PATCallBack, cbdata );
}

void ts_psi_Packet_Push( ts_pid_t *p_pid, const uint8_t *p_pktbuffer )
{
    if( p_pid->type == TYPE_PAT )
        dvbpsi_packet_push( p_pid->u.p_pat->handle, (uint8_t *) p_pktbuffer );
    else if( p_pid->type == TYPE_PMT )
        dvbpsi_packet_push( p_pid->u.p_pmt->handle, (uint8_t *) p_pktbuffer );
}
