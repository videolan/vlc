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
    else
    {
        cmd.value = "";
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
    else
    {
        cmd.value = "";
    }
}

function update_vlm_add_schedule()
{
}

function clear_vlm_add()
{
    document.getElementById( 'vlm_command' ).value = "";
    document.getElementById( 'vlm_broadcast_name' ).value = "";
    document.getElementById( 'vlm_vod_name' ).value = "";
}

function clear_children( elt )
{
    if( elt )
        while( elt.hasChildNodes() )
            elt.removeChild( elt.firstChild );
}

function parse_vlm_cmd()
{
    if( req.readyState == 4 )
    {
        if( req.status == 200 )
        {
            vlm_answer = req.responseXML.documentElement;
            error_tag = vlm_answer.getElementsByTagName( 'error' )[0];
            vlme = document.getElementById( 'vlm_error' );
            clear_children( vlme );
            if( error_tag.hasChildNodes() )
            {
                vlme.appendChild( document.createTextNode( 'Error: ' + error_tag.firstChild.data ) );
                vlme.style.color = "#f00";
            }
            else
            {
                vlme.appendChild( document.createTextNode( 'Command succesful (' + value( 'vlm_command' ) + ') ' ) );
                vlme.style.color = "#0f0";
                clear_vlm_add();
            }
            link = document.createElement( "input" );
            link.setAttribute( 'type', 'button' );
            link.setAttribute( 'onclick', 'clear_children( document.getElementById( "vlm_error" ) );' );
            link.setAttribute( 'value', 'clear' );
            vlme.appendChild( link );

            vlm_get_elements();
        }
    }
}

function parse_vlm_elements()
{
    if( req.readyState == 4 )
    {
        if( req.status == 200 )
        {
            vlmb = document.getElementById( 'vlm_broadcast_list' );
            vlmv = document.getElementById( 'vlm_vod_list' );
            vlms = document.getElementById( 'vlm_schedule_list' );

            clear_children( vlmb );
            clear_children( vlmv );
            clear_children( vlms );

            answer = req.responseXML.documentElement;

            elt = answer.firstChild;

            while( elt )
            {
                if( elt.nodeName == "broadcast" || elt.nodeName == "vod" )
                {
                    nb = document.createElement( 'div' );
                    nb.setAttribute( 'class', 'list_element' );
                    if( elt.nodeName == "broadcast" )
                    {
                        vlmb.appendChild( nb );
                    }
                    else
                    {
                        vlmv.appendChild( nb );
                    }
                    nbname = document.createElement( 'b' );
                    nbname.appendChild( document.createTextNode( elt.getAttribute( 'name' ) ) );
                    nb.appendChild( nbname );
                    
                    link = document.createElement( 'input' );
                    link.setAttribute( 'type', 'button' );
                    if( elt.getAttribute( 'enabled' ) == 'yes' )
                    {
                        nb.appendChild( document.createTextNode( " enabled " ) );
                        link.setAttribute( 'onclick', 'vlm_disable("'+elt.getAttribute( 'name' ) + '");' );
                        link.setAttribute( 'value', "Disable" );
                    }
                    else
                    {
                        nb.appendChild( document.createTextNode( " disabled " ) );
                        link.setAttribute( 'onclick', 'vlm_enable("'+elt.getAttribute( 'name' ) + '");' );
                        link.setAttribute( 'value', "Enable" );
                    }
                    nb.appendChild( link );
                    
                    if( elt.nodeName == "broadcast" )
                    {
                        link = document.createElement( 'input' );
                        link.setAttribute( 'type', 'button' );
                        if( elt.getAttribute( 'loop' ) == 'yes' )
                        {
                            nb.appendChild( document.createTextNode( " loop " ) );

                            link.setAttribute( 'onclick', 'vlm_unloop("'+elt.getAttribute( 'name' ) + '");' );
                            link.setAttribute( 'value', "Un-loop" );
                        }
                        else
                        {
                            nb.appendChild( document.createTextNode( " play once " ) );
                            
                            link.setAttribute( 'onclick', 'vlm_loop("'+elt.getAttribute( 'name' ) + '");' );
                            link.setAttribute( 'value', "Loop" );
                        }
                        nb.appendChild( link );

                        if( elt.getAttribute( 'enabled' ) == 'yes' )
                        {
                            nb.appendChild( document.createTextNode( " " ) );
                            link = document.createElement( 'input' );
                            link.setAttribute( 'type', 'button' );
                            link.setAttribute( 'onclick', 'vlm_play("'+elt.getAttribute('name')+'");' );
                            link.setAttribute( 'value', 'Play' );
                            nb.appendChild( link );
                        }

                        nb.appendChild( document.createTextNode( " " ) );
                        link = document.createElement( 'input' );
                        link.setAttribute( 'type', 'button' );
                        link.setAttribute( 'onclick', 'vlm_pause("'+elt.getAttribute('name')+'");' );
                        link.setAttribute( 'value', 'Pause' );
                        nb.appendChild( link );

                        nb.appendChild( document.createTextNode( " " ) );
                        link = document.createElement( 'input' );
                        link.setAttribute( 'type', 'button' );
                        link.setAttribute( 'onclick', 'vlm_stop("'+elt.getAttribute('name')+'");' );
                        link.setAttribute( 'value', 'Stop' );
                        nb.appendChild( link );
                    }
                    
                    nb.appendChild( document.createTextNode( " " ) );
                    link = document.createElement( 'input' );
                    link.setAttribute( 'type', 'button' );
                    link.setAttribute( 'onclick', 'vlm_delete("'+elt.getAttribute( 'name' ) + '");' );
                    link.setAttribute( 'value', "Delete" );
                    nb.appendChild( link );

                    list = document.createElement( "ul" );
                    /* begin input list */
                    inputs = elt.getElementsByTagName( 'input' );
                    for( i = 0; i < inputs.length; i++ )
                    {
                        item = document.createElement( "li" );
                        item.appendChild( document.createTextNode( "Input: " + inputs[i].firstChild.data + " " ) );
                        link = document.createElement( "input" );
                        link.setAttribute( 'type', 'button' );
                        link.setAttribute( 'onclick', 'vlm_delete_input("' + elt.getAttribute( 'name' ) + '", '+(i+1)+' );' );
                        link.setAttribute( 'value', "Delete" );
                        item.appendChild( link );
                        list.appendChild( item );
                    }

                    /* Add input */
                    item = document.createElement( "li" );
                    text = document.createElement( "input" );
                    text.setAttribute( 'type', 'text' );
                    text.setAttribute( 'size', '40' );
                    text.setAttribute( 'id', 'vlm_elt_'+elt.getAttribute('name')+'_input' );
                    item.appendChild( text );
                    item.appendChild( document.createTextNode( ' ' ) );
                    edit = document.createElement( "input" );
                    edit.setAttribute( 'type', 'button' );
                    edit.setAttribute( 'value', 'Edit' );
                    edit.setAttribute( 'onclick', 'vlm_input_edit("vlm_elt_'+elt.getAttribute('name')+'_input");');
                    item.appendChild( edit );
                    item.appendChild( document.createTextNode( ' ' ) );
                    link = document.createElement( "input" );
                    link.setAttribute( 'type', 'button' );
                    link.setAttribute( 'onclick', 'vlm_add_input("'+elt.getAttribute('name')+'",document.getElementById("vlm_elt_'+elt.getAttribute('name')+'_input").value );' );
                    link.setAttribute( 'value', 'Add input' );
                    item.appendChild( link );
                    
                    list.appendChild( item );
                    /* end of input list */
                    
                    /* output */
                    item = document.createElement( "li" );
                    outputelt = elt.getElementsByTagName( 'output' )[0];
                    if( outputelt.hasChildNodes() )
                    {
                        output = outputelt.firstChild.data;
                    }
                    else
                    {
                        output = "";
                    }
                    item.appendChild( document.createTextNode( 'Output: ' ) );
                    text = document.createElement( "input" );
                    text.setAttribute( 'type', 'text' );
                    text.setAttribute( 'id', 'vlm_elt_'+elt.getAttribute('name')+'_output' );
                    text.setAttribute( 'value', output );
                    item.appendChild( text );

                    item.appendChild( document.createTextNode( ' ' ) );

                    edit = document.createElement( "input" );
                    edit.setAttribute( 'type', 'button' );
                    edit.setAttribute( 'value', 'Edit' );
                    edit.setAttribute( 'onclick', 'vlm_output_edit("vlm_elt_'+elt.getAttribute('name')+'_output");');
                    item.appendChild( edit );
                    list.appendChild( item );
                    item.appendChild( document.createTextNode( ' ' ) );
                    link = document.createElement( "input" );
                    link.setAttribute( 'type', 'button' );
                    link.setAttribute( 'onclick', 'vlm_output("'+elt.getAttribute( 'name' )+ '",document.getElementById("vlm_elt_'+elt.getAttribute( 'name' )+'_output").value);' );
                    link.setAttribute( 'value', 'Change output' );
                    item.appendChild( link );
                    /* end of output */

                    /* begin options list */
                    options = elt.getElementsByTagName( 'option' );
                    for( i = 0; i < options.length; i++ )
                    {
                        item = document.createElement( "li" );
                        item.appendChild( document.createTextNode( "Option: " + options[i].firstChild.data ) );
                        list.appendChild( item );
                    }

                    /* Add option */
                    item = document.createElement( "li" );
                    item.appendChild( document.createTextNode( ' ' ) );
                    text = document.createElement( "input" );
                    text.setAttribute( 'type', 'text' );
                    text.setAttribute( 'size', '40' );
                    text.setAttribute( 'id', 'vlm_elt_'+elt.getAttribute('name')+'_option' );
                    item.appendChild( text );
                    item.appendChild( document.createTextNode( ' ' ) );
                    link = document.createElement( "input" );
                    link.setAttribute( 'type', 'button' );
                    link.setAttribute( 'onclick', 'vlm_option("'+elt.getAttribute('name')+'",document.getElementById("vlm_elt_'+elt.getAttribute('name')+'_option").value );' );
                    link.setAttribute( 'value', 'Add option' );
                    item.appendChild( link );
                    
                    list.appendChild( item );
                    /* end of options */
                    
                    nb.appendChild( list );
                    
                }
                else if( elt.nodeName == "schedule" )
                {
                }
                elt = elt.nextSibling;
            }
        }
    }
}

function vlm_cmd( cmd )
{
    loadXMLDoc( 'requests/vlm_cmd.xml?command='+cmd.replace(/\#/g, '%23'), parse_vlm_cmd );
}

function vlm_get_elements( )
{
    loadXMLDoc( 'requests/vlm.xml', parse_vlm_elements );
}

/* helper functions */

function vlm_disable( name )
{
    document.getElementById( 'vlm_command' ).value = "setup "+name+" disabled";
    vlm_cmd( value( 'vlm_command' ) );
}

function vlm_enable( name )
{
    document.getElementById( 'vlm_command' ).value = "setup "+name+" enabled";
    vlm_cmd( value( 'vlm_command' ) );
}

function vlm_loop( name )
{
    document.getElementById( 'vlm_command' ).value = "setup "+name+" loop";
    vlm_cmd( value( 'vlm_command' ) );
}

function vlm_unloop( name )
{
    document.getElementById( 'vlm_command' ).value = "setup "+name+" unloop";
    vlm_cmd( value( 'vlm_command' ) );
}

function vlm_play( name )
{
    document.getElementById( 'vlm_command' ).value = "control "+name+" play";
    vlm_cmd( value( 'vlm_command' ) );
}

function vlm_pause( name )
{
    document.getElementById( 'vlm_command' ).value = "control "+name+" pause";
    vlm_cmd( value( 'vlm_command' ) );
}

function vlm_stop( name )
{
    document.getElementById( 'vlm_command' ).value = "control "+name+" stop";
    vlm_cmd( value( 'vlm_command' ) );
}

function vlm_delete( name )
{
    document.getElementById( 'vlm_command' ).value = "del "+name;
    vlm_cmd( value( 'vlm_command' ) );
}

function vlm_delete_input( name, num )
{
    document.getElementById( 'vlm_command' ).value = "setup "+name+" inputdeln "+num;
    vlm_cmd( value( 'vlm_command' ) );
}

function vlm_add_input( name, input )
{
    document.getElementById( 'vlm_command' ).value = "setup "+name+" input "+input;
    vlm_cmd( value( 'vlm_command' ) );
}

function vlm_output( name, output )
{
    document.getElementById( 'vlm_command' ).value = "setup "+name+" output "+output;
    vlm_cmd( value( 'vlm_command' ) );
}

function vlm_option( name, option )
{
    document.getElementById( 'vlm_command' ).value = "setup "+name+" option "+option;
    vlm_cmd( value( 'vlm_command' ) );
}

function vlm_send( )
{
    vlm_cmd( value( 'vlm_command' ) );
}
