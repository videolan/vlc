/*****************************************************************************
 * chapter_command.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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

#include "chapter_command.hpp"
#include "demux.hpp"
#include <algorithm>

namespace mkv {

void chapter_codec_cmds_c::AddCommand( const KaxChapterProcessCommand & command )
{
    uint32 codec_time = uint32(-1);
    for( size_t i = 0; i < command.ListSize(); i++ )
    {
        if( MKV_CHECKED_PTR_DECL_CONST( p_cpt, KaxChapterProcessTime, command[i] ) )
        {
            codec_time = static_cast<uint32>( *p_cpt );
            break;
        }
    }

    for( size_t i = 0; i < command.ListSize(); i++ )
    {
        if( MKV_CHECKED_PTR_DECL_CONST( p_cpd, KaxChapterProcessData, command[i] ) )
        {
            std::vector<KaxChapterProcessData*> *containers[] = {
                &during_cmds, /* codec_time = 0 */
                &enter_cmds,  /* codec_time = 1 */
                &leave_cmds   /* codec_time = 2 */
            };

            if( codec_time < 3 )
                containers[codec_time]->push_back( new KaxChapterProcessData( *p_cpd ) );
        }
    }
}

int16 dvd_chapter_codec_c::GetTitleNumber()
{
    if ( p_private_data->GetSize() >= 3)
    {
        const binary* p_data = p_private_data->GetBuffer();
        if ( p_data[0] == MATROSKA_DVD_LEVEL_SS )
        {
            return int16( (p_data[2] << 8) + p_data[3] );
        }
    }
    return -1;
}

bool dvd_chapter_codec_c::Enter()
{
    return EnterLeaveHelper( "Matroska DVD enter command", &enter_cmds );
}

bool dvd_chapter_codec_c::Leave()
{
    return EnterLeaveHelper( "Matroska DVD leave command", &leave_cmds );
}

bool dvd_chapter_codec_c::EnterLeaveHelper( char const * str_diag, std::vector<KaxChapterProcessData*> * p_container )
{
    bool f_result = false;
    std::vector<KaxChapterProcessData*>::iterator it = p_container->begin ();
    while( it != p_container->end() )
    {
        if( (*it)->GetSize() )
        {
            binary *p_data = (*it)->GetBuffer();
            size_t i_size  = std::min<size_t>( *p_data++, ( (*it)->GetSize() - 1 ) >> 3 ); // avoid reading too much
            for( ; i_size > 0; i_size -=1, p_data += 8 )
            {
                msg_Dbg( &sys.demuxer, "%s", str_diag);
                f_result |= sys.dvd_interpretor.Interpret( p_data );
            }
        }
        ++it;
    }
    return f_result;
}


std::string dvd_chapter_codec_c::GetCodecName( bool f_for_title ) const
{
    std::string result;
    if ( p_private_data->GetSize() >= 3)
    {
        const binary* p_data = p_private_data->GetBuffer();
/*        if ( p_data[0] == MATROSKA_DVD_LEVEL_TT )
        {
            uint16_t i_title = (p_data[1] << 8) + p_data[2];
            char psz_str[11];
            sprintf( psz_str, " %d  ---", i_title );
            result = "---  DVD Title";
            result += psz_str;
        }
        else */ if ( p_data[0] == MATROSKA_DVD_LEVEL_LU )
        {
            char psz_str[11];
            sprintf( psz_str, " (%c%c)  ---", p_data[1], p_data[2] );
            result = "---  DVD Menu";
            result += psz_str;
        }
        else if ( p_data[0] == MATROSKA_DVD_LEVEL_SS && f_for_title )
        {
            if ( p_data[1] == 0x00 )
                result = "First Played";
            else if ( p_data[1] == 0xC0 )
                result = "Video Manager";
            else if ( p_data[1] == 0x80 )
            {
                uint16_t i_title = (p_data[2] << 8) + p_data[3];
                char psz_str[20];
                sprintf( psz_str, " %d -----", i_title );
                result = "----- Title";
                result += psz_str;
            }
        }
    }

    return result;
}
class virtual_segment_c;
class chapter_item_c;

// see http://www.dvd-replica.com/DVD/vmcmdset.php for a description of DVD commands
bool dvd_command_interpretor_c::Interpret( const binary * p_command, size_t i_size )
{
    if ( i_size != 8 )
        return false;

    virtual_segment_c *p_vsegment = NULL;
    virtual_chapter_c *p_vchapter = NULL;
    bool f_result = false;
    uint16 i_command = ( p_command[0] << 8 ) + p_command[1];

    // handle register tests if there are some
    if ( (i_command & 0xF0) != 0 )
    {
        bool b_test_positive = true;//(i_command & CMD_DVD_IF_NOT) == 0;
        bool b_test_value    = (i_command & CMD_DVD_TEST_VALUE) != 0;
        uint8 i_test = i_command & 0x70;
        uint16 i_value;

        // see http://dvd.sourceforge.net/dvdinfo/vmi.html
        uint8  i_cr1;
        uint16 i_cr2;
        switch ( i_command >> 12 )
        {
        default:
            i_cr1 = p_command[3];
            i_cr2 = (p_command[4] << 8) + p_command[5];
            break;
        case 3:
        case 4:
        case 5:
            i_cr1 = p_command[6];
            i_cr2 = p_command[7];
            b_test_value = false;
            break;
        case 6:
        case 7:
            if ( ((p_command[1] >> 4) & 0x7) == 0)
            {
                i_cr1 = p_command[4];
                i_cr2 = (p_command[6] << 8) + p_command[7];
            }
            else
            {
                i_cr1 = p_command[5];
                i_cr2 = (p_command[6] << 8) + p_command[7];
            }
            break;
        }

        if ( b_test_value )
            i_value = i_cr2;
        else
            i_value = GetPRM( i_cr2 );

        switch ( i_test )
        {
        case CMD_DVD_IF_GPREG_EQUAL:
            // if equals
            msg_Dbg( &sys.demuxer, "IF %s EQUALS %s", GetRegTypeName( false, i_cr1 ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) == i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_NOT_EQUAL:
            // if not equals
            msg_Dbg( &sys.demuxer, "IF %s NOT EQUALS %s", GetRegTypeName( false, i_cr1 ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) != i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_INF:
            // if inferior
            msg_Dbg( &sys.demuxer, "IF %s < %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) < i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_INF_EQUAL:
            // if inferior or equal
            msg_Dbg( &sys.demuxer, "IF %s < %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) <= i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_AND:
            // if logical and
            msg_Dbg( &sys.demuxer, "IF %s & %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) & i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_SUP:
            // if superior
            msg_Dbg( &sys.demuxer, "IF %s >= %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) > i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_SUP_EQUAL:
            // if superior or equal
            msg_Dbg( &sys.demuxer, "IF %s >= %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) >= i_value ))
            {
                b_test_positive = false;
            }
            break;
        }

        if ( !b_test_positive )
            return false;
    }

    // strip the test command
    i_command &= 0xFF0F;

    switch ( i_command )
    {
    case CMD_DVD_NOP:
    case CMD_DVD_NOP2:
        {
            msg_Dbg( &sys.demuxer, "NOP" );
            break;
        }
    case CMD_DVD_BREAK:
        {
            msg_Dbg( &sys.demuxer, "Break" );
            // TODO
            break;
        }
    case CMD_DVD_JUMP_TT:
        {
            uint8 i_title = p_command[5];
            msg_Dbg( &sys.demuxer, "JumpTT %d", i_title );

            // find in the ChapProcessPrivate matching this Title level
            p_vchapter = sys.BrowseCodecPrivate( 1, MatchTitleNumber, &i_title, sizeof(i_title), p_vsegment );
            if ( p_vsegment != NULL && p_vchapter != NULL )
            {
                /* enter via the First Cell */
                uint8 i_cell = 1;
                p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD, MatchCellNumber, &i_cell, sizeof(i_cell) );
                if ( p_vchapter != NULL )
                {
                    sys.JumpTo( *p_vsegment, *p_vchapter );
                    f_result = true;
                }
            }

            break;
        }
    case CMD_DVD_CALLSS_VTSM1:
        {
            msg_Dbg( &sys.demuxer, "CallSS" );
            binary p_type;
            switch( (p_command[6] & 0xC0) >> 6 ) {
                case 0:
                    p_type = p_command[5] & 0x0F;
                    switch ( p_type )
                    {
                    case 0x00:
                        msg_Dbg( &sys.demuxer, "CallSS PGC (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x02:
                        msg_Dbg( &sys.demuxer, "CallSS Title Entry (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x03:
                        msg_Dbg( &sys.demuxer, "CallSS Root Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x04:
                        msg_Dbg( &sys.demuxer, "CallSS Subpicture Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x05:
                        msg_Dbg( &sys.demuxer, "CallSS Audio Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x06:
                        msg_Dbg( &sys.demuxer, "CallSS Angle Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x07:
                        msg_Dbg( &sys.demuxer, "CallSS Chapter Menu (rsm_cell %x)", p_command[4]);
                        break;
                    default:
                        msg_Dbg( &sys.demuxer, "CallSS <unknown> (rsm_cell %x)", p_command[4]);
                        break;
                    }
                    p_vchapter = sys.BrowseCodecPrivate( 1, MatchPgcType, &p_type, 1, p_vsegment );
                    if ( p_vsegment != NULL && p_vchapter != NULL )
                    {
                        /* enter via the first Cell */
                        uint8 i_cell = 1;
                        p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD, MatchCellNumber, &i_cell, sizeof(i_cell) );
                        if ( p_vchapter != NULL )
                        {
                            sys.JumpTo( *p_vsegment, *p_vchapter );
                            f_result = true;
                        }
                    }
                break;
                case 1:
                    msg_Dbg( &sys.demuxer, "CallSS VMGM (menu %d, rsm_cell %x)", p_command[5] & 0x0F, p_command[4]);
                break;
                case 2:
                    msg_Dbg( &sys.demuxer, "CallSS VTSM (menu %d, rsm_cell %x)", p_command[5] & 0x0F, p_command[4]);
                break;
                case 3:
                    msg_Dbg( &sys.demuxer, "CallSS VMGM (pgc %d, rsm_cell %x)", (p_command[2] << 8) + p_command[3], p_command[4]);
                break;
            }
            break;
        }
    case CMD_DVD_JUMP_SS:
        {
            msg_Dbg( &sys.demuxer, "JumpSS");
            binary p_type;
            switch( (p_command[5] & 0xC0) >> 6 ) {
                case 0:
                    msg_Dbg( &sys.demuxer, "JumpSS FP");
                break;
                case 1:
                    p_type = p_command[5] & 0x0F;
                    switch ( p_type )
                    {
                    case 0x02:
                        msg_Dbg( &sys.demuxer, "JumpSS VMGM Title Entry");
                        break;
                    case 0x03:
                        msg_Dbg( &sys.demuxer, "JumpSS VMGM Root Menu");
                        break;
                    case 0x04:
                        msg_Dbg( &sys.demuxer, "JumpSS VMGM Subpicture Menu");
                        break;
                    case 0x05:
                        msg_Dbg( &sys.demuxer, "JumpSS VMGM Audio Menu");
                        break;
                    case 0x06:
                        msg_Dbg( &sys.demuxer, "JumpSS VMGM Angle Menu");
                        break;
                    case 0x07:
                        msg_Dbg( &sys.demuxer, "JumpSS VMGM Chapter Menu");
                        break;
                    default:
                        msg_Dbg( &sys.demuxer, "JumpSS <unknown>");
                        break;
                    }
                    // find the VMG
                    p_vchapter = sys.BrowseCodecPrivate( 1, MatchIsVMG, NULL, 0, p_vsegment );
                    if ( p_vsegment != NULL )
                    {
                        p_vchapter = p_vsegment->BrowseCodecPrivate( 1, MatchPgcType, &p_type, 1 );
                        if ( p_vchapter != NULL )
                        {
                            sys.JumpTo( *p_vsegment, *p_vchapter );
                            f_result = true;
                        }
                    }
                break;
                case 2:
                    p_type = p_command[5] & 0x0F;
                    switch ( p_type )
                    {
                    case 0x02:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) Title Entry", p_command[4], p_command[3]);
                        break;
                    case 0x03:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) Root Menu", p_command[4], p_command[3]);
                        break;
                    case 0x04:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) Subpicture Menu", p_command[4], p_command[3]);
                        break;
                    case 0x05:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) Audio Menu", p_command[4], p_command[3]);
                        break;
                    case 0x06:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) Angle Menu", p_command[4], p_command[3]);
                        break;
                    case 0x07:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) Chapter Menu", p_command[4], p_command[3]);
                        break;
                    default:
                        msg_Dbg( &sys.demuxer, "JumpSS VTSM (vts %d, ttn %d) <unknown>", p_command[4], p_command[3]);
                        break;
                    }

                    p_vchapter = sys.BrowseCodecPrivate( 1, MatchVTSMNumber, &p_command[4], 1, p_vsegment );

                    if ( p_vsegment != NULL && p_vchapter != NULL )
                    {
                        // find the title in the VTS
                        p_vchapter = p_vchapter->BrowseCodecPrivate( 1, MatchTitleNumber, &p_command[3], 1 );
                        if ( p_vchapter != NULL )
                        {
                            // find the specified menu in the VTSM
                            p_vchapter = p_vsegment->BrowseCodecPrivate( 1, MatchPgcType, &p_type, 1 );
                            if ( p_vchapter != NULL )
                            {
                                sys.JumpTo( *p_vsegment, *p_vchapter );
                                f_result = true;
                            }
                        }
                        else
                            msg_Dbg( &sys.demuxer, "Title (%d) does not exist in this VTS", p_command[3] );
                    }
                    else
                        msg_Dbg( &sys.demuxer, "DVD Domain VTS (%d) not found", p_command[4] );
                break;
                case 3:
                    msg_Dbg( &sys.demuxer, "JumpSS VMGM (pgc %d)", (p_command[2] << 8) + p_command[3]);
                break;
            }
            break;
        }
    case CMD_DVD_JUMPVTS_PTT:
        {
            uint8 i_title = p_command[5];
            uint8 i_ptt = p_command[3];

            msg_Dbg( &sys.demuxer, "JumpVTS Title (%d) PTT (%d)", i_title, i_ptt);

            // find the current VTS content segment
            p_vchapter = sys.p_current_vsegment->BrowseCodecPrivate( 1, MatchIsDomain, NULL, 0 );
            if ( p_vchapter != NULL )
            {
                int16 i_curr_title = ( p_vchapter->p_chapter )? p_vchapter->p_chapter->GetTitleNumber() : 0;
                if ( i_curr_title > 0 )
                {
                    p_vchapter = sys.BrowseCodecPrivate( 1, MatchVTSNumber, &i_curr_title, sizeof(i_curr_title), p_vsegment );

                    if ( p_vsegment != NULL && p_vchapter != NULL )
                    {
                        // find the title in the VTS
                        p_vchapter = p_vchapter->BrowseCodecPrivate( 1, MatchTitleNumber, &i_title, sizeof(i_title) );
                        if ( p_vchapter != NULL )
                        {
                            // find the chapter in the title
                            p_vchapter = p_vchapter->BrowseCodecPrivate( 1, MatchChapterNumber, &i_ptt, sizeof(i_ptt) );
                            if ( p_vchapter != NULL )
                            {
                                sys.JumpTo( *p_vsegment, *p_vchapter );
                                f_result = true;
                            }
                        }
                    else
                        msg_Dbg( &sys.demuxer, "Title (%d) does not exist in this VTS", i_title );
                    }
                    else
                        msg_Dbg( &sys.demuxer, "DVD Domain VTS (%d) not found", i_curr_title );
                }
                else
                    msg_Dbg( &sys.demuxer, "JumpVTS_PTT command found but not in a VTS(M)");
            }
            else
                msg_Dbg( &sys.demuxer, "JumpVTS_PTT command but the DVD domain wasn't found");
            break;
        }
    case CMD_DVD_SET_GPRMMD:
        {
            msg_Dbg( &sys.demuxer, "Set GPRMMD [%d]=%d", (p_command[4] << 8) + p_command[5], (p_command[2] << 8) + p_command[3]);

            if ( !SetGPRM( (p_command[4] << 8) + p_command[5], (p_command[2] << 8) + p_command[3] ) )
                msg_Dbg( &sys.demuxer, "Set GPRMMD failed" );
            break;
        }
    case CMD_DVD_LINKPGCN:
        {
            uint16 i_pgcn = (p_command[6] << 8) + p_command[7];

            msg_Dbg( &sys.demuxer, "Link PGCN(%d)", i_pgcn );
            p_vchapter = sys.p_current_vsegment->BrowseCodecPrivate( 1, MatchPgcNumber, &i_pgcn, 2 );
            if ( p_vchapter != NULL )
            {
                sys.JumpTo( *sys.p_current_vsegment, *p_vchapter );
                f_result = true;
            }
            break;
        }
    case CMD_DVD_LINKCN:
        {
            uint8 i_cn = p_command[7];

            p_vchapter = sys.p_current_vsegment->CurrentChapter();

            msg_Dbg( &sys.demuxer, "LinkCN (cell %d)", i_cn );
            p_vchapter = p_vchapter->BrowseCodecPrivate( 1, MatchCellNumber, &i_cn, 1 );
            if ( p_vchapter != NULL )
            {
                sys.JumpTo( *sys.p_current_vsegment, *p_vchapter );
                f_result = true;
            }
            break;
        }
    case CMD_DVD_GOTO_LINE:
        {
            msg_Dbg( &sys.demuxer, "GotoLine (%d)", (p_command[6] << 8) + p_command[7] );
            // TODO
            break;
        }
    case CMD_DVD_SET_HL_BTNN1:
        {
            msg_Dbg( &sys.demuxer, "SetHL_BTN (%d)", p_command[4] );
            SetSPRM( 0x88, p_command[4] );
            break;
        }
    default:
        {
            msg_Dbg( &sys.demuxer, "unsupported command : %02X %02X %02X %02X %02X %02X %02X %02X"
                     ,p_command[0]
                     ,p_command[1]
                     ,p_command[2]
                     ,p_command[3]
                     ,p_command[4]
                     ,p_command[5]
                     ,p_command[6]
                     ,p_command[7]);
            break;
        }
    }

    return f_result;
}



bool dvd_command_interpretor_c::MatchIsDomain( const chapter_codec_cmds_c &data, const void *, size_t )
{
    return ( data.p_private_data != NULL && data.p_private_data->GetBuffer()[0] == MATROSKA_DVD_LEVEL_SS );
}

bool dvd_command_interpretor_c::MatchIsVMG( const chapter_codec_cmds_c &data, const void *, size_t )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 2 )
        return false;

    return ( data.p_private_data->GetBuffer()[0] == MATROSKA_DVD_LEVEL_SS && data.p_private_data->GetBuffer()[1] == 0xC0);
}

bool dvd_command_interpretor_c::MatchVTSNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 2 || data.p_private_data == NULL || data.p_private_data->GetSize() < 4 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_SS || data.p_private_data->GetBuffer()[1] != 0x80 )
        return false;

    uint16 i_gtitle = (data.p_private_data->GetBuffer()[2] << 8 ) + data.p_private_data->GetBuffer()[3];
    uint16 i_title = *static_cast<uint16 const*>( p_cookie );

    return (i_gtitle == i_title);
}

bool dvd_command_interpretor_c::MatchVTSMNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 1 || data.p_private_data == NULL || data.p_private_data->GetSize() < 4 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_SS || data.p_private_data->GetBuffer()[1] != 0x40 )
        return false;

    uint8 i_gtitle = data.p_private_data->GetBuffer()[3];
    uint8 i_title = *static_cast<uint8 const*>( p_cookie );

    return (i_gtitle == i_title);
}

bool dvd_command_interpretor_c::MatchTitleNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 1 || data.p_private_data == NULL || data.p_private_data->GetSize() < 4 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_TT )
        return false;

    uint16 i_gtitle = (data.p_private_data->GetBuffer()[1] << 8 ) + data.p_private_data->GetBuffer()[2];
    uint8 i_title = *static_cast<uint8 const*>( p_cookie );

    return (i_gtitle == i_title);
}

bool dvd_command_interpretor_c::MatchPgcType( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 1 || data.p_private_data == NULL || data.p_private_data->GetSize() < 8 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_PGC )
        return false;

    uint8 i_pgc_type = data.p_private_data->GetBuffer()[3] & 0x0F;
    uint8 i_pgc = *static_cast<uint8 const*>( p_cookie );

    return (i_pgc_type == i_pgc);
}

bool dvd_command_interpretor_c::MatchPgcNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 2 || data.p_private_data == NULL || data.p_private_data->GetSize() < 8 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_PGC )
        return false;

    uint16 const* i_pgc_n = static_cast<uint16 const*>( p_cookie );
    uint16 i_pgc_num = (data.p_private_data->GetBuffer()[1] << 8) + data.p_private_data->GetBuffer()[2];

    return (i_pgc_num == *i_pgc_n);
}

bool dvd_command_interpretor_c::MatchChapterNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 1 || data.p_private_data == NULL || data.p_private_data->GetSize() < 2 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_PTT )
        return false;

    uint8 i_chapter = data.p_private_data->GetBuffer()[1];
    uint8 i_ptt = *static_cast<uint8 const*>( p_cookie );

    return (i_chapter == i_ptt);
}

bool dvd_command_interpretor_c::MatchCellNumber( const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size )
{
    if ( i_cookie_size != 1 || data.p_private_data == NULL || data.p_private_data->GetSize() < 5 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_CN )
        return false;

    uint8 const* i_cell_n = static_cast<uint8 const*>( p_cookie );
    uint8 i_cell_num = data.p_private_data->GetBuffer()[3];

    return (i_cell_num == *i_cell_n);
}

const std::string matroska_script_interpretor_c::CMD_MS_GOTO_AND_PLAY = "GotoAndPlay";

// see http://www.matroska.org/technical/specs/chapters/index.html#mscript
//  for a description of existing commands
bool matroska_script_interpretor_c::Interpret( const binary * p_command, size_t i_size )
{
    bool b_result = false;

    std::string sz_command( reinterpret_cast<const char*> (p_command), i_size );

    msg_Dbg( &sys.demuxer, "command : %s", sz_command.c_str() );

    if ( sz_command.compare( 0, CMD_MS_GOTO_AND_PLAY.size(), CMD_MS_GOTO_AND_PLAY ) == 0 )
    {
        size_t i,j;

        // find the (
        for ( i=CMD_MS_GOTO_AND_PLAY.size(); i<sz_command.size(); i++)
        {
            if ( sz_command[i] == '(' )
            {
                i++;
                break;
            }
        }
        // find the )
        for ( j=i; j<sz_command.size(); j++)
        {
            if ( sz_command[j] == ')' )
            {
                i--;
                break;
            }
        }

        std::string st = sz_command.substr( i+1, j-i-1 );
        int64_t i_chapter_uid = atoll( st.c_str() );

        virtual_segment_c *p_vsegment;
        virtual_chapter_c *p_vchapter = sys.FindChapter( i_chapter_uid, p_vsegment );

        if ( p_vchapter == NULL )
            msg_Dbg( &sys.demuxer, "Chapter %" PRId64 " not found", i_chapter_uid);
        else
        {
            if ( !p_vchapter->EnterAndLeave( sys.p_current_vsegment->CurrentChapter() ) )
                p_vsegment->Seek( sys.demuxer, p_vchapter->i_mk_virtual_start_time, p_vchapter );
            b_result = true;
        }
    }

    return b_result;
}

bool matroska_script_codec_c::Enter()
{
    bool f_result = false;
    std::vector<KaxChapterProcessData*>::iterator index = enter_cmds.begin();
    while ( index != enter_cmds.end() )
    {
        if ( (*index)->GetSize() )
        {
            msg_Dbg( &sys.demuxer, "Matroska Script enter command" );
            f_result |= interpretor.Interpret( (*index)->GetBuffer(), (*index)->GetSize() );
        }
        ++index;
    }
    return f_result;
}

bool matroska_script_codec_c::Leave()
{
    bool f_result = false;
    std::vector<KaxChapterProcessData*>::iterator index = leave_cmds.begin();
    while ( index != leave_cmds.end() )
    {
        if ( (*index)->GetSize() )
        {
            msg_Dbg( &sys.demuxer, "Matroska Script leave command" );
            f_result |= interpretor.Interpret( (*index)->GetBuffer(), (*index)->GetSize() );
        }
        ++index;
    }
    return f_result;
}

} // namespace
