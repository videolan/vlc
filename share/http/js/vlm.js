/*****************************************************************************
 * vlm.js: VLC media player web interface
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
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

/* replace quotes and spaces by underscores */
function addunderscores( str ){ return str.replace(/\'|\"| /g, '_'); }

/**********************************************************************
 * Input dialog functions
 *********************************************************************/

function vlm_input_edit( dest )
{
    document.getElementById( 'input_dest' ).value = dest;
    show( 'input' );
}

function vlm_input_change()
{
    hide( 'input' );
    document.getElementById( value( 'input_dest' ) ).value = value( 'input_mrl' );
}

function vlm_output_edit( dest )
{
    document.getElementById( 'sout_dest' ).value = dest;
    show( 'sout' );
}

function vlm_output_change()
{
    hide( 'sout' );
    document.getElementById( value( 'sout_dest' ) ).value = value( 'sout_mrl' ).substr(6); /* substr <-> remove :sout= */
}

function hide_vlm_add()
{
    document.getElementById( 'vlm_add_broadcast' ).style.display = 'none';
    document.getElementById( 'vlm_add_vod' ).style.display = 'none';
    document.getElementById( 'vlm_add_schedule' ).style.display = 'none';
}

function update_vlm_add_broadcast()
{
    cmd = document.getElementById( 'vlm_command' );

    if( value( 'vlm_broadcast_name' ) )
    {
        cmd.value = "new " + addunderscores( value( 'vlm_broadcast_name' ) )
                    + " broadcast";

        if( checked( 'vlm_broadcast_enabled' ) )
        {
            cmd.value += " enabled";
        }
        
        if( checked( 'vlm_broadcast_loop' ) )
        {
            cmd.value += " loop";
        }

        if( value( 'vlm_broadcast_input' ) )
        {
            cmd.value += " input " + value( 'vlm_broadcast_input' );
        }

        if( value( 'vlm_broadcast_output' ) )
        {
            cmd.value += " output " + value( 'vlm_broadcast_output' );
        }
    }
}

function update_vlm_add_vod()
{
    cmd = document.getElementById( 'vlm_command' );

    if( value( 'vlm_vod_name' ) )
    {
        cmd.value = "new " + addunderscores( value( 'vlm_vod_name' ) )
                    + " vod";

        if( checked( 'vlm_vod_enabled' ) )
        {
            cmd.value += " enabled";
        }
        
        if( value( 'vlm_vod_input' ) )
        {
            cmd.value += " input " + value( 'vlm_vod_input' );
        }

        if( value( 'vlm_vod_output' ) )
        {
            cmd.value += " output " + value( 'vlm_vod_output' );
        }
    }
}

function update_vlm_add_schedule()
{
}

function parse_vlm_cmd()
{
    if( req.readyState == 4 )
    {
        if( req.status == 200 )
        {
            vlm_answer = req.responseXML.documentElement;
            error_tag = vlm_answer.getElementsByTagName( 'error' )[0];
            if( error_tag.hasChildNodes() )
            {
                vlm_error = error_tag.firstChild.data;
                alert( vlm_error );
            }
        }
    }
}

function vlm_cmd( cmd )
{
    loadXMLDoc( 'requests/vlm_cmd.xml?command='+cmd, parse_vlm_cmd );
}

function vlm_send( )
{
    vlm_cmd( value( 'vlm_command' ) );
}
