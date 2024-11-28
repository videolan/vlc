// Copyright (C) 2003-2024 VLC authors and VideoLAN
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// chapter_command_dvd.cpp : DVD codec for Matroska Chapter Codecs
// Authors: Laurent Aimar <fenrir@via.ecp.fr>
//          Steve Lhomme <steve.lhomme@free.fr>


#include "chapter_command_dvd.hpp"
#include "virtual_segment.hpp"

#include <vlc_subpicture.h> // vlc_spu_highlight_t

namespace mkv {

constexpr binary MATROSKA_DVD_LEVEL_SS   = 0x30;
constexpr binary MATROSKA_DVD_LEVEL_LU   = 0x2A;
constexpr binary MATROSKA_DVD_LEVEL_TT   = 0x28;
constexpr binary MATROSKA_DVD_LEVEL_PGC  = 0x20;
// constexpr binary MATROSKA_DVD_LEVEL_PG   = 0x18;
constexpr binary MATROSKA_DVD_LEVEL_PTT  = 0x10;
constexpr binary MATROSKA_DVD_LEVEL_CN   = 0x08;

int16_t dvd_chapter_codec_c::GetTitleNumber() const
{
    if ( p_private_data != nullptr && p_private_data->GetSize() >= 3)
    {
        const binary* p_data = p_private_data->GetBuffer();
        if ( p_data[0] == MATROSKA_DVD_LEVEL_SS )
        {
            return int16_t( (p_data[2] << 8) + p_data[3] );
        }
    }
    return -1;
}

bool dvd_chapter_codec_c::Enter()
{
    return EnterLeaveHelper( "Matroska DVD enter command", enter_cmds );
}

bool dvd_chapter_codec_c::Leave()
{
    return EnterLeaveHelper( "Matroska DVD leave command", leave_cmds );
}

bool dvd_chapter_codec_c::EnterLeaveHelper( char const * str_diag, ChapterProcess & p_container )
{
    bool f_result = false;
    ChapterProcess::iterator it = p_container.begin ();
    while( it != p_container.end() )
    {
        if( (*it).GetSize() )
        {
            binary *p_data = (*it).GetBuffer();
            size_t i_size  = std::min<size_t>( *p_data++, ( (*it).GetSize() - 1 ) >> 3 ); // avoid reading too much
            for( ; i_size > 0; i_size -=1, p_data += 8 )
            {
                vlc_debug( l, "%s", str_diag);
                f_result |= intepretor.Interpret( p_data );
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
            snprintf( psz_str, ARRAY_SIZE(psz_str), " (%c%c)  ---", p_data[1], p_data[2] );
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
                snprintf( psz_str, ARRAY_SIZE(psz_str), " %d -----", i_title );
                result = "----- Title";
                result += psz_str;
            }
        }
    }

    return result;
}

// see http://www.dvd-replica.com/DVD/vmcmdset.php for a description of DVD commands
bool dvd_command_interpretor_c::Interpret( const binary * p_command, size_t i_size )
{
    if ( i_size != 8 )
        return false;

    virtual_segment_c *p_vsegment = NULL;
    virtual_chapter_c *p_vchapter = NULL;
    bool f_result = false;
    uint16_t i_command = ( p_command[0] << 8 ) + p_command[1];

    // handle register tests if there are some
    if ( (i_command & 0xF0) != 0 )
    {
        bool b_test_positive = true;//(i_command & CMD_DVD_IF_NOT) == 0;
        bool b_test_value    = (i_command & CMD_DVD_TEST_VALUE) != 0;
        uint8_t i_test = i_command & 0x70;
        uint16_t i_value;

        // see http://dvd.sourceforge.net/dvdinfo/vmi.html
        uint8_t  i_cr1;
        uint16_t i_cr2;
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
            vlc_debug( l, "IF %s EQUALS %s", GetRegTypeName( false, i_cr1 ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) == i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_NOT_EQUAL:
            // if not equals
            vlc_debug( l, "IF %s NOT EQUALS %s", GetRegTypeName( false, i_cr1 ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) != i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_INF:
            // if inferior
            vlc_debug( l, "IF %s < %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) < i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_INF_EQUAL:
            // if inferior or equal
            vlc_debug( l, "IF %s < %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) <= i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_AND:
            // if logical and
            vlc_debug( l, "IF %s & %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) & i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_SUP:
            // if superior
            vlc_debug( l, "IF %s >= %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
            if (!( GetPRM( i_cr1 ) > i_value ))
            {
                b_test_positive = false;
            }
            break;
        case CMD_DVD_IF_GPREG_SUP_EQUAL:
            // if superior or equal
            vlc_debug( l, "IF %s >= %s", GetRegTypeName( false, p_command[3] ).c_str(), GetRegTypeName( b_test_value, i_value ).c_str() );
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
            vlc_debug( l, "NOP" );
            break;
        }
    case CMD_DVD_BREAK:
        {
            vlc_debug( l, "Break" );
            // TODO
            break;
        }
    case CMD_DVD_JUMP_TT:
        {
            uint8_t i_title = p_command[5];
            vlc_debug( l, "JumpTT %d", i_title );

            // find in the ChapProcessPrivate matching this Title level
            p_vchapter = vm.BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                [i_title](const chapter_codec_cmds_c &data) {
                    return MatchTitleNumber(data, i_title);
                }, p_vsegment );
            if ( p_vsegment != NULL && p_vchapter != NULL )
            {
                /* enter via the First Cell */
                uint8_t i_cell = 1;
                p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                    [i_cell](const chapter_codec_cmds_c &data) {
                        return MatchCellNumber( data, i_cell );
                    });
                if ( p_vchapter != NULL )
                {
                    vm.JumpTo( *p_vsegment, *p_vchapter );
                    f_result = true;
                }
            }

            break;
        }
    case CMD_DVD_CALLSS_VTSM1:
        {
            vlc_debug( l, "CallSS" );
            binary p_type;
            switch( (p_command[6] & 0xC0) >> 6 ) {
                case 0:
                    p_type = p_command[5] & 0x0F;
                    switch ( p_type )
                    {
                    case 0x00:
                        vlc_debug( l, "CallSS PGC (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x02:
                        vlc_debug( l, "CallSS Title Entry (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x03:
                        vlc_debug( l, "CallSS Root Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x04:
                        vlc_debug( l, "CallSS Subpicture Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x05:
                        vlc_debug( l, "CallSS Audio Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x06:
                        vlc_debug( l, "CallSS Angle Menu (rsm_cell %x)", p_command[4]);
                        break;
                    case 0x07:
                        vlc_debug( l, "CallSS Chapter Menu (rsm_cell %x)", p_command[4]);
                        break;
                    default:
                        vlc_debug( l, "CallSS <unknown> (rsm_cell %x)", p_command[4]);
                        break;
                    }
                    p_vchapter = vm.BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                        [p_type](const chapter_codec_cmds_c &data) {
                            return MatchPgcType( data, p_type );
                        }, p_vsegment );
                    if ( p_vsegment != NULL && p_vchapter != NULL )
                    {
                        /* enter via the first Cell */
                        uint8_t i_cell = 1;
                        p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                            [i_cell](const chapter_codec_cmds_c &data) {
                                return MatchCellNumber( data, i_cell ); } );
                        if ( p_vchapter != NULL )
                        {
                            vm.JumpTo( *p_vsegment, *p_vchapter );
                            f_result = true;
                        }
                    }
                break;
                case 1:
                    vlc_debug( l, "CallSS VMGM (menu %d, rsm_cell %x)", p_command[5] & 0x0F, p_command[4]);
                break;
                case 2:
                    vlc_debug( l, "CallSS VTSM (menu %d, rsm_cell %x)", p_command[5] & 0x0F, p_command[4]);
                break;
                case 3:
                    vlc_debug( l, "CallSS VMGM (pgc %d, rsm_cell %x)", (p_command[2] << 8) + p_command[3], p_command[4]);
                break;
            }
            break;
        }
    case CMD_DVD_JUMP_SS:
        {
            vlc_debug( l, "JumpSS");
            binary p_type;
            switch( (p_command[5] & 0xC0) >> 6 ) {
                case 0:
                    vlc_debug( l, "JumpSS FP");
                break;
                case 1:
                    p_type = p_command[5] & 0x0F;
                    switch ( p_type )
                    {
                    case 0x02:
                        vlc_debug( l, "JumpSS VMGM Title Entry");
                        break;
                    case 0x03:
                        vlc_debug( l, "JumpSS VMGM Root Menu");
                        break;
                    case 0x04:
                        vlc_debug( l, "JumpSS VMGM Subpicture Menu");
                        break;
                    case 0x05:
                        vlc_debug( l, "JumpSS VMGM Audio Menu");
                        break;
                    case 0x06:
                        vlc_debug( l, "JumpSS VMGM Angle Menu");
                        break;
                    case 0x07:
                        vlc_debug( l, "JumpSS VMGM Chapter Menu");
                        break;
                    default:
                        vlc_debug( l, "JumpSS <unknown>");
                        break;
                    }
                    // find the VMG
                    p_vchapter = vm.BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                        [](const chapter_codec_cmds_c &data) {
                            return MatchIsVMG( data); }, p_vsegment );
                    if ( p_vsegment != NULL )
                    {
                        p_vchapter = p_vsegment->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                            [p_type](const chapter_codec_cmds_c &data) {
                                return MatchPgcType( data, p_type ); } );
                        if ( p_vchapter != NULL )
                        {
                            vm.JumpTo( *p_vsegment, *p_vchapter );
                            f_result = true;
                        }
                    }
                break;
                case 2:
                    p_type = p_command[5] & 0x0F;
                    switch ( p_type )
                    {
                    case 0x02:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) Title Entry", p_command[4], p_command[3]);
                        break;
                    case 0x03:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) Root Menu", p_command[4], p_command[3]);
                        break;
                    case 0x04:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) Subpicture Menu", p_command[4], p_command[3]);
                        break;
                    case 0x05:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) Audio Menu", p_command[4], p_command[3]);
                        break;
                    case 0x06:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) Angle Menu", p_command[4], p_command[3]);
                        break;
                    case 0x07:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) Chapter Menu", p_command[4], p_command[3]);
                        break;
                    default:
                        vlc_debug( l, "JumpSS VTSM (vts %d, ttn %d) <unknown>", p_command[4], p_command[3]);
                        break;
                    }

                    {
                    uint8_t i_vts = p_command[4];
                    p_vchapter = vm.BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                        [i_vts](const chapter_codec_cmds_c &data) {
                            return MatchVTSMNumber( data,  i_vts ); }, p_vsegment );

                    if ( p_vsegment != NULL && p_vchapter != NULL )
                    {
                        // find the title in the VTS
                        uint8_t i_title = p_command[3];
                        p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                            [i_title](const chapter_codec_cmds_c &data) {
                                return MatchTitleNumber( data, i_title ); } );
                        if ( p_vchapter != NULL )
                        {
                            // find the specified menu in the VTSM
                            p_vchapter = p_vsegment->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                                [p_type](const chapter_codec_cmds_c &data) {
                                    return MatchPgcType( data, p_type ); } );
                            if ( p_vchapter != NULL )
                            {
                                vm.JumpTo( *p_vsegment, *p_vchapter );
                                f_result = true;
                            }
                        }
                        else
                            vlc_debug( l, "Title (%d) does not exist in this VTS", i_title );
                    }
                    else
                        vlc_debug( l, "DVD Domain VTS (%d) not found", i_vts );
                    }
                break;
                case 3:
                    vlc_debug( l, "JumpSS VMGM (pgc %d)", (p_command[2] << 8) + p_command[3]);
                break;
            }
            break;
        }
    case CMD_DVD_JUMPVTS_PTT:
        {
            uint8_t i_title = p_command[5];
            uint8_t i_ptt = p_command[3];

            vlc_debug( l, "JumpVTS Title (%d) PTT (%d)", i_title, i_ptt);

            // find the current VTS content segment
            p_vchapter = vm.GetCurrentVSegment()->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                [](const chapter_codec_cmds_c &data) {
                    return MatchIsDomain( data); } );
            if ( p_vchapter != NULL )
            {
                int16_t i_curr_title = ( p_vchapter->p_chapter )? p_vchapter->p_chapter->GetTitleNumber() : 0;
                if ( i_curr_title > 0 )
                {
                    p_vchapter = vm.BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                        [i_curr_title](const chapter_codec_cmds_c &data) {
                            return MatchVTSNumber( data, i_curr_title ); }, p_vsegment );

                    if ( p_vsegment != NULL && p_vchapter != NULL )
                    {
                        // find the title in the VTS
                        p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                            [i_title](const chapter_codec_cmds_c &data) {
                                return MatchTitleNumber( data, i_title ); } );
                        if ( p_vchapter != NULL )
                        {
                            // find the chapter in the title
                            p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                                [i_ptt](const chapter_codec_cmds_c &data) {
                                    return MatchChapterNumber( data, i_ptt ); } );
                            if ( p_vchapter != NULL )
                            {
                                vm.JumpTo( *p_vsegment, *p_vchapter );
                                f_result = true;
                            }
                        }
                    else
                        vlc_debug( l, "Title (%d) does not exist in this VTS", i_title );
                    }
                    else
                        vlc_debug( l, "DVD Domain VTS (%d) not found", i_curr_title );
                }
                else
                    vlc_debug( l, "JumpVTS_PTT command found but not in a VTS(M)");
            }
            else
                vlc_debug( l, "JumpVTS_PTT command but the DVD domain wasn't found");
            break;
        }
    case CMD_DVD_SET_GPRMMD:
        {
            vlc_debug( l, "Set GPRMMD [%d]=%d", (p_command[4] << 8) + p_command[5], (p_command[2] << 8) + p_command[3]);

            if ( !SetGPRM( (p_command[4] << 8) + p_command[5], (p_command[2] << 8) + p_command[3] ) )
                vlc_debug( l, "Set GPRMMD failed" );
            break;
        }
    case CMD_DVD_LINKPGCN:
        {
            uint16_t i_pgcn = (p_command[6] << 8) + p_command[7];

            vlc_debug( l, "Link PGCN(%d)", i_pgcn );
            p_vchapter = vm.GetCurrentVSegment()->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                [i_pgcn](const chapter_codec_cmds_c &data) {
                    return MatchPgcNumber( data, i_pgcn ); } );
            if ( p_vchapter != NULL )
            {
                vm.JumpTo( *vm.GetCurrentVSegment(), *p_vchapter );
                f_result = true;
            }
            break;
        }
    case CMD_DVD_LINKCN:
        {
            uint8_t i_cn = p_command[7];

            p_vchapter = vm.GetCurrentVSegment()->CurrentChapter();

            vlc_debug( l, "LinkCN (cell %d)", i_cn );
            p_vchapter = p_vchapter->BrowseCodecPrivate( MATROSKA_CHAPTER_CODEC_DVD,
                [i_cn](const chapter_codec_cmds_c &data) {
                    return MatchCellNumber( data, i_cn ); } );
            if ( p_vchapter != NULL )
            {
                vm.JumpTo( *vm.GetCurrentVSegment(), *p_vchapter );
                f_result = true;
            }
            break;
        }
    case CMD_DVD_GOTO_LINE:
        {
            vlc_debug( l, "GotoLine (%d)", (p_command[6] << 8) + p_command[7] );
            // TODO
            break;
        }
    case CMD_DVD_SET_HL_BTNN1:
        {
            vlc_debug( l, "SetHL_BTN (%d)", p_command[4] );
            SetSPRM( 0x88, p_command[4] );
            break;
        }
    default:
        {
            vlc_debug( l, "unsupported command : %02X %02X %02X %02X %02X %02X %02X %02X"
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

bool dvd_command_interpretor_c::ProcessNavAction( uint16_t button )
{
    const pci_t & pci = pci_packet;

    if( button <= 0 || button > pci.hli.hl_gi.btn_ns )
        return false;

    SetSPRM( 0x88, button );
    const btni_t & button_ptr = pci.hli.btnit[button-1];
    if ( button_ptr.auto_action_mode )
    {
        // process the button action
        return Interpret( button_ptr.cmd.bytes, 8 );
    }
    return false;
}

bool dvd_command_interpretor_c::HandleKeyEvent( NavivationKey key )
{
    const pci_t & pci = pci_packet;
    uint16_t i_curr_button = GetSPRM( 0x88 );

    if( i_curr_button <= 0 || i_curr_button > pci.hli.hl_gi.btn_ns )
        return false;

    const btni_t & button_ptr = pci.hli.btnit[i_curr_button-1];

    switch( key )
    {
    case LEFT:  return ProcessNavAction( button_ptr.left );
    case RIGHT: return ProcessNavAction( button_ptr.right );
    case UP:    return ProcessNavAction( button_ptr.up );
    case DOWN:  return ProcessNavAction( button_ptr.down );
    case OK:
        // process the button action
        return Interpret( button_ptr.cmd.bytes, 8 );
    case MENU:
    case POPUP:
        return false;
    }
    vlc_assert_unreachable();
}

void dvd_command_interpretor_c::HandleMousePressed( unsigned x, unsigned y )
{
    const pci_t & pci = pci_packet;

    int32_t button;
    int32_t best,dist,d;
    int32_t mx,my,dx,dy;

    // get current button
    best = 0;
    dist = 0x08000000; /* >> than  (720*720)+(567*567); */
    for(button = 1; button <= pci.hli.hl_gi.btn_ns; button++)
    {
        const btni_t & button_ptr = pci.hli.btnit[button-1];

        if((x >= button_ptr.x_start)
            && (x <= button_ptr.x_end)
            && (y >= button_ptr.y_start)
            && (y <= button_ptr.y_end))
        {
            mx = (button_ptr.x_start + button_ptr.x_end)/2;
            my = (button_ptr.y_start + button_ptr.y_end)/2;
            dx = mx - x;
            dy = my - y;
            d = (dx*dx) + (dy*dy);
            /* If the mouse is within the button and the mouse is closer
            * to the center of this button then it is the best choice. */
            if(d < dist) {
                dist = d;
                best = button;
            }
        }
    }

    if ( best == 0)
        return;

    const btni_t & button_ptr = pci.hli.btnit[best-1];
    uint16_t i_curr_button = GetSPRM( 0x88 );

    vlc_debug( l, "Clicked button %d", best );

    // process the button action
    SetSPRM( 0x88, best );
    Interpret( button_ptr.cmd.bytes, 8 );

    vlc_debug( l, "Processed button %d", best );

    // select new button
    if ( best != i_curr_button )
    {
        // TODO: make sure we do not overflow in the conversion
        vlc_spu_highlight_t spu_hl = vlc_spu_highlight_t();

        spu_hl.x_start = (int)button_ptr.x_start;
        spu_hl.y_start = (int)button_ptr.y_start;

        spu_hl.x_end = (int)button_ptr.x_end;
        spu_hl.y_end = (int)button_ptr.y_end;

        uint32_t i_palette;

        if(button_ptr.btn_coln != 0) {
            i_palette = pci.hli.btn_colit.btn_coli[button_ptr.btn_coln-1][1];
        } else {
            i_palette = 0;
        }

        for( int i = 0; i < 4; i++ )
        {
            uint32_t i_yuv = 0xFF;//p_sys->clut[(hl.palette>>(16+i*4))&0x0f];
            uint8_t i_alpha = (i_palette>>(i*4))&0x0f;
            i_alpha = i_alpha == 0xf ? 0xff : i_alpha << 4;

            spu_hl.palette.palette[i][0] = (i_yuv >> 16) & 0xff;
            spu_hl.palette.palette[i][1] = (i_yuv >> 0) & 0xff;
            spu_hl.palette.palette[i][2] = (i_yuv >> 8) & 0xff;
            spu_hl.palette.palette[i][3] = i_alpha;
        }

        vm.SetHighlight( spu_hl );
    }
}

bool dvd_command_interpretor_c::MatchIsDomain( const chapter_codec_cmds_c &data )
{
    return ( data.p_private_data != NULL && data.p_private_data->GetBuffer()[0] == MATROSKA_DVD_LEVEL_SS );
}

bool dvd_command_interpretor_c::MatchIsVMG( const chapter_codec_cmds_c &data )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 2 )
        return false;

    return ( data.p_private_data->GetBuffer()[0] == MATROSKA_DVD_LEVEL_SS && data.p_private_data->GetBuffer()[1] == 0xC0);
}

bool dvd_command_interpretor_c::MatchVTSNumber( const chapter_codec_cmds_c &data, uint16_t i_title )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 4 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_SS || data.p_private_data->GetBuffer()[1] != 0x80 )
        return false;

    uint16_t i_gtitle = (data.p_private_data->GetBuffer()[2] << 8 ) + data.p_private_data->GetBuffer()[3];

    return (i_gtitle == i_title);
}

bool dvd_command_interpretor_c::MatchVTSMNumber( const chapter_codec_cmds_c &data, uint8_t i_title )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 4 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_SS || data.p_private_data->GetBuffer()[1] != 0x40 )
        return false;

    uint8_t i_gtitle = data.p_private_data->GetBuffer()[3];

    return (i_gtitle == i_title);
}

bool dvd_command_interpretor_c::MatchTitleNumber( const chapter_codec_cmds_c &data, uint8_t i_title )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 4 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_TT )
        return false;

    uint16_t i_gtitle = (data.p_private_data->GetBuffer()[1] << 8 ) + data.p_private_data->GetBuffer()[2];

    return (i_gtitle == i_title);
}

bool dvd_command_interpretor_c::MatchPgcType( const chapter_codec_cmds_c &data, uint8_t i_pgc )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 8 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_PGC )
        return false;

    uint8_t i_pgc_type = data.p_private_data->GetBuffer()[3] & 0x0F;

    return (i_pgc_type == i_pgc);
}

bool dvd_command_interpretor_c::MatchPgcNumber( const chapter_codec_cmds_c &data, uint16_t i_pgc_n )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 8 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_PGC )
        return false;

    uint16_t i_pgc_num = (data.p_private_data->GetBuffer()[1] << 8) + data.p_private_data->GetBuffer()[2];

    return (i_pgc_num == i_pgc_n);
}

bool dvd_command_interpretor_c::MatchChapterNumber( const chapter_codec_cmds_c &data, uint8_t i_ptt )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 2 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_PTT )
        return false;

    uint8_t i_chapter = data.p_private_data->GetBuffer()[1];

    return (i_chapter == i_ptt);
}

bool dvd_command_interpretor_c::MatchCellNumber( const chapter_codec_cmds_c &data, uint8_t i_cell_n )
{
    if ( data.p_private_data == NULL || data.p_private_data->GetSize() < 5 )
        return false;

    if ( data.p_private_data->GetBuffer()[0] != MATROSKA_DVD_LEVEL_CN )
        return false;

    uint8_t i_cell_num = data.p_private_data->GetBuffer()[3];

    return (i_cell_num == i_cell_n);
}

void dvd_command_interpretor_c::SetPci(const uint8_t *data, unsigned size)
{
    if (size < sizeof(pci_packet))
        return;

    memcpy(&pci_packet, data, sizeof(pci_packet));

#ifndef WORDS_BIGENDIAN
    for( uint8_t button = 1; button <= pci_packet.hli.hl_gi.btn_ns &&
            button < ARRAY_SIZE(pci_packet.hli.btnit); button++) {
        btni_t & button_ptr = pci_packet.hli.btnit[button-1];
        binary *p_data = (binary*) &button_ptr;

        uint16_t i_x_start = ((p_data[0] & 0x3F) << 4 ) + ( p_data[1] >> 4 );
        uint16_t i_x_end   = ((p_data[1] & 0x03) << 8 ) + p_data[2];
        uint16_t i_y_start = ((p_data[3] & 0x3F) << 4 ) + ( p_data[4] >> 4 );
        uint16_t i_y_end   = ((p_data[4] & 0x03) << 8 ) + p_data[5];
        button_ptr.x_start = i_x_start;
        button_ptr.x_end   = i_x_end;
        button_ptr.y_start = i_y_start;
        button_ptr.y_end   = i_y_end;

    }
    for ( uint8_t i = 0; i<3; i++ )
        for ( uint8_t j = 0; j<2; j++ )
            pci_packet.hli.btn_colit.btn_coli[i][j] = U32_AT( &pci_packet.hli.btn_colit.btn_coli[i][j] );
#endif
}

} // namespace
