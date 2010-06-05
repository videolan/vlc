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

function toggle_show_vlm_helper()
{
    var vlmh = document.getElementById( "vlm_helper" );
    var vlmhctrl = document.getElementById( "vlm_helper_controls" );
    var btn = document.getElementById( "btn_vlm_helper_toggle" );
    if( vlmh.style.display == 'block' || vlmh.style.display == '')
    {
        vlmh.style.display = 'none';
        vlmhctrl.style.display = 'none';
        btn.removeChild( btn.firstChild );
        btn.appendChild( document.createTextNode( 'Show VLM helper' ) );
    }
    else
    {
        vlmh.style.display = 'block';
        vlmhctrl.style.display = 'inline';
        btn.removeChild( btn.firstChild );
        btn.appendChild( document.createTextNode( 'Hide VLM helper' ) );
    }
}

function vlm_input_edit( dest )
{
    document.getElementById( 'input_dest' ).value = dest;
    show( 'input' );
}

function vlm_input_change()
{
    document.getElementById( value( 'input_dest' ) ).value = value( 'input_mrl' ).replace( /\ :/g, " option " );
    hide( 'input' );
    document.getElementById( value( 'input_dest' ) ).focus();
}

function vlm_output_edit( dest )
{
    document.getElementById( 'sout_dest' ).value = dest;
    show( 'sout' );
}

function vlm_output_change()
{
    document.getElementById( value( 'sout_dest' ) ).value = value( 'sout_mrl' ).substr(6).replace( /\ :/g, " option " ); /* substr <-> remove :sout= */
    hide( 'sout' );
    document.getElementById( value( 'sout_dest' ) ).focus();
}

function hide_vlm_add()
{
    document.getElementById( 'vlm_add_broadcast' ).style.display = 'none';
    document.getElementById( 'vlm_add_vod' ).style.display = 'none';
    document.getElementById( 'vlm_add_schedule' ).style.display = 'none';
    document.getElementById( 'vlm_add_other' ).style.display = 'none';
}

function toggle_schedule_date()
{
    if( checked( 'vlm_schedule_now' ) )
    {
        disable( 'vlm_schedule_year' );
        disable( 'vlm_schedule_month' );
        disable( 'vlm_schedule_day' );
        disable( 'vlm_schedule_hour' );
        disable( 'vlm_schedule_minute' );
        disable( 'vlm_schedule_second' );
    }
    else
    {
        enable( 'vlm_schedule_year' );
        enable( 'vlm_schedule_month' );
        enable( 'vlm_schedule_day' );
        enable( 'vlm_schedule_hour' );
        enable( 'vlm_schedule_minute' );
        enable( 'vlm_schedule_second' );
    }
}

function toggle_schedule_repeat()
{
    if( checked( 'vlm_schedule_repeat' ) )
    {
        enable( 'vlm_schedule_period_year' );
        enable( 'vlm_schedule_period_month' );
        enable( 'vlm_schedule_period_day' );
        enable( 'vlm_schedule_period_hour' );
        enable( 'vlm_schedule_period_minute' );
        enable( 'vlm_schedule_period_second' );
        enable( 'vlm_schedule_repeat_times' );
    }
    else
    {
        disable( 'vlm_schedule_period_year' );
        disable( 'vlm_schedule_period_month' );
        disable( 'vlm_schedule_period_day' );
        disable( 'vlm_schedule_period_hour' );
        disable( 'vlm_schedule_period_minute' );
        disable( 'vlm_schedule_period_second' );
        disable( 'vlm_schedule_repeat_times' );
    }
}

function vlm_schedule_type_change( name )
{
    var act = document.getElementById( 'vlm_elt_' + name + '_action' ).value;
    var itemname = document.getElementById( 'vlm_elt_' + name + '_name' );
    var opt = document.getElementById( 'vlm_elt_' + name + '_opt' );
    if( act == "play" || act == "pause" || act == "stop" )
    {
        itemname.style.display = "";
        opt.style.display = "none";
    }
    else if( act == "seek" )
    {
        itemname.style.display = "";
        opt.style.display = "";
    }
    else
    {
        itemname.style.display = "none";
        opt.style.display = "";
    }
}

function sanitize_input( str )
{
    return str.replace( /\"/g, '\\\"' ).replace( /^/, '"' ).replace( /$/, '"' ).replace( /\ option\ /g, '" option "' );
}

function update_vlm_add_broadcast()
{
    var cmd = document.getElementById( 'vlm_command' );

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
            cmd.value += " input " + sanitize_input( value( 'vlm_broadcast_input' ) );
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
    var cmd = document.getElementById( 'vlm_command' );

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
            cmd.value += " input " + sanitize_input( value( 'vlm_vod_input' ) );
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
    var cmd = document.getElementById( 'vlm_command' );

    check_and_replace_int( 'vlm_schedule_year', '0000' );
    check_and_replace_int( 'vlm_schedule_month', '00' );
    check_and_replace_int( 'vlm_schedule_day', '00' );
    check_and_replace_int( 'vlm_schedule_hour', '00' );
    check_and_replace_int( 'vlm_schedule_minute', '00' );
    check_and_replace_int( 'vlm_schedule_second', '00' );
    check_and_replace_int( 'vlm_schedule_period_year', '0000' );
    check_and_replace_int( 'vlm_schedule_period_month', '00' );
    check_and_replace_int( 'vlm_schedule_period_day', '00' );
    check_and_replace_int( 'vlm_schedule_period_hour', '00' );
    check_and_replace_int( 'vlm_schedule_period_minute', '00' );
    check_and_replace_int( 'vlm_schedule_period_second', '00' );

    if( value( 'vlm_schedule_name' ) )
    {
        cmd.value = "new " + addunderscores( value( 'vlm_schedule_name' ) ) + " schedule";

        if( checked( 'vlm_schedule_enabled' ) )
        {
            cmd.value += " enabled";
        }

        if( checked( 'vlm_schedule_now' ) )
        {
            cmd.value += " date now";
        }
        else
        {
            cmd.value += " date " + value( 'vlm_schedule_year' ) + "/" + value( 'vlm_schedule_month' ) + "/" + value( 'vlm_schedule_day' ) + '-' + value( 'vlm_schedule_hour' ) + ':' + value( 'vlm_schedule_minute' ) + ':' + value( 'vlm_schedule_second' );
        }

        if( checked( 'vlm_schedule_repeat' ) )
        {
            cmd.value += " period " + value( 'vlm_schedule_period_year' ) + "/" + value( 'vlm_schedule_period_month' ) + "/" + value( 'vlm_schedule_period_day' ) + '-' + value( 'vlm_schedule_period_hour' ) + ':' + value( 'vlm_schedule_period_minute' ) + ':' + value( 'vlm_schedule_period_second' );

            if( value( 'vlm_schedule_repeat_times' ) != 0 )
            {
                cmd.value += " repeat " + (value( 'vlm_schedule_repeat_times' ) - 1 );
            }
        }
            
    }
    else
    {
        cmd.value = "";
    }
}

function update_vlm_add_other()
{
    var cmd = document.getElementById( 'vlm_command' );
    cmd.value = "";
}

function clear_vlm_add()
{
    document.getElementById( 'vlm_command' ).value = "";
    document.getElementById( 'vlm_broadcast_name' ).value = "";
    document.getElementById( 'vlm_vod_name' ).value = "";
}

function create_button( caption, action )
{
/*    var link = document.createElement( "input" );
    link.setAttribute( 'type', 'button' );*/
    /* link.setAttribute( 'onclick', action ); */
    /* Above doesn't work on ie. You need to use something like
     * link.onclick = function() { alert( 'pouet' ); };
     * instead ... conclusion: IE is crap */
   /* link.setAttribute( 'value', caption );*/

    var d = document.createElement( 'div' );
    d.innerHTML = "<input type='button' onclick='"+action+"' value='"+caption+"' />"; /* other IE work around  ... still crap. Use double quotes only in action */
    var link = d.firstChild;
    return link;
}
function create_option( caption, value )
{
    var opt = document.createElement( 'option' );
    opt.setAttribute( 'value', value );
    opt.appendChild( document.createTextNode( caption ) );
    return opt;
}

function parse_vlm_cmd()
{
    if( req.readyState == 4 )
    {
        if( req.status == 200 )
        {
            var vlm_answer = req.responseXML.documentElement;
            var error_tag = vlm_answer.getElementsByTagName( 'error' )[0];
            var vlme = document.getElementById( 'vlm_error' );
            clear_children( vlme );
            if( error_tag.hasChildNodes() )
            {
                vlme.appendChild( document.createTextNode( 'Error: ' + error_tag.firstChild.data ) );
                vlme.style.color = "#f00";
            }
            else
            {
                vlme.appendChild( document.createTextNode( 'Command successful (' + value( 'vlm_command' ) + ') ' ) );
                vlme.style.color = "#0f0";
                clear_vlm_add();
            }
            vlme.appendChild( create_button( 'clear', 'clear_children( document.getElementById( "vlm_error" ) );' ) );

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
            var vlmb = document.getElementById( 'vlm_broadcast_list' );
            var vlmv = document.getElementById( 'vlm_vod_list' );
            var vlms = document.getElementById( 'vlm_schedule_list' );

            clear_children( vlmb );
            clear_children( vlmv );
            clear_children( vlms );

            answer = req.responseXML.documentElement;

            var elt = answer.firstChild;

            while( elt )
            {
                if( elt.nodeName == "broadcast" || elt.nodeName == "vod" )
                {
                    var nb = document.createElement( 'div' );
                    setclass( nb, 'list_element' );
                    if( elt.nodeName == "broadcast" )
                    {
                        vlmb.appendChild( nb );
                    }
                    else
                    {
                        vlmv.appendChild( nb );
                    }
                    var nbname = document.createElement( 'b' );
                    nbname.appendChild( document.createTextNode( elt.getAttribute( 'name' ) ) );
                    nb.appendChild( nbname );
                    
                    if( elt.getAttribute( 'enabled' ) == 'yes' )
                    {
                        nb.appendChild( document.createTextNode( " enabled " ) );
                        nb.appendChild( create_button( "Disable", 'vlm_disable("'+elt.getAttribute( 'name' ) + '");' ) );
                    }
                    else
                    {
                        nb.appendChild( document.createTextNode( " disabled " ) );
                        nb.appendChild( create_button( "Enable", 'vlm_enable("'+elt.getAttribute( 'name' ) + '");' ) );
                    }
                    
                    if( elt.nodeName == "broadcast" )
                    {
                        if( elt.getAttribute( 'loop' ) == 'yes' )
                        {
                            nb.appendChild( document.createTextNode( " loop " ) );

                            nb.appendChild( create_button( 'Un-loop', 'vlm_unloop("'+elt.getAttribute( 'name' ) + '");' ) );
                        }
                        else
                        {
                            nb.appendChild( document.createTextNode( " play once " ) );
                            nb.appendChild( create_button( 'Loop', 'vlm_loop("'+elt.getAttribute( 'name' ) + '");' ) );
                            
                        }

                        if( elt.getAttribute( 'enabled' ) == 'yes' )
                        {
                            nb.appendChild( document.createTextNode( " " ) );
                            nb.appendChild( create_button( 'Play', 'vlm_play("'+elt.getAttribute('name')+'");' ) );
                        }

                        nb.appendChild( document.createTextNode( " " ) );
                        nb.appendChild( create_button( 'Pause', 'vlm_pause("'+elt.getAttribute('name')+'");' ) );

                        nb.appendChild( document.createTextNode( " " ) );
                        nb.appendChild( create_button( 'Stop', 'vlm_stop("'+elt.getAttribute('name')+'");' ) );
                    }
                    
                    nb.appendChild( document.createTextNode( " " ) );
                    nb.appendChild( create_button( 'Delete', 'vlm_delete("'+elt.getAttribute( 'name' ) + '");' ) );

                    var list = document.createElement( "ul" );

                    /* begin input list */
                    var item = document.createElement( "li" );
                    list.appendChild( item );
                    item.appendChild( document.createTextNode( "Inputs: " ) );
                    var text = document.createElement( "input" );
                    text.setAttribute( 'type', 'text' );
                    text.setAttribute( 'size', '40' );
                    text.setAttribute( 'id', 'vlm_elt_'+elt.getAttribute('name')+'_input' );
                    text.setAttribute( 'onkeypress', 'if( event.keyCode == 13 ) vlm_add_input("'+elt.getAttribute('name')+'",document.getElementById("vlm_elt_'+elt.getAttribute('name')+'_input").value );' );
                    item.appendChild( text );
                    item.appendChild( document.createTextNode( ' ' ) );
                    item.appendChild( create_button( 'Edit', 'vlm_input_edit("vlm_elt_'+elt.getAttribute('name')+'_input");') );
                    item.appendChild( document.createTextNode( ' ' ) );
                    item.appendChild( create_button( 'Add input', 'vlm_add_input("'+elt.getAttribute('name')+'",document.getElementById("vlm_elt_'+elt.getAttribute('name')+'_input").value );' ) );
                    
                    var inputs = elt.getElementsByTagName( 'input' );
                    if( inputs.length > 0 )
                    {
                        var ilist = document.createElement( "ol" );
                        ilist.setAttribute( 'start', '1' );
                        item.appendChild( ilist );
                        for( i = 0; i < inputs.length; i++ )
                        {
                            var item = document.createElement( "li" );
                            item.appendChild( document.createTextNode( inputs[i].firstChild.data + " " ) );
                            item.appendChild( create_button( "Delete", 'vlm_delete_input("' + elt.getAttribute( 'name' ) + '", '+(i+1)+' );' ) );
                            ilist.appendChild( item );
                        }
                    }
                    /* end of input list */
                    
                    /* output */
                    var item = document.createElement( "li" );
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
                    var text = document.createElement( "input" );
                    text.setAttribute( 'type', 'text' );
                    text.setAttribute( 'id', 'vlm_elt_'+elt.getAttribute('name')+'_output' );
                    text.setAttribute( 'value', output );
                    text.setAttribute( 'onkeypress', 'if( event.keyCode == 13 )  vlm_output("'+elt.getAttribute( 'name' )+ '",document.getElementById("vlm_elt_'+elt.getAttribute( 'name' )+'_output").value);' );
                    item.appendChild( text );

                    item.appendChild( document.createTextNode( ' ' ) );

                    item.appendChild( create_button( 'Edit', 'vlm_output_edit("vlm_elt_'+elt.getAttribute('name')+'_output");' ) );
                    item.appendChild( document.createTextNode( ' ' ) );
                    item.appendChild( create_button( 'Change output', 'vlm_output("'+elt.getAttribute( 'name' )+ '",document.getElementById("vlm_elt_'+elt.getAttribute( 'name' )+'_output").value);' ) );
                    list.appendChild( item );
                    /* end of output */

                    /* begin options list */
                    var item = document.createElement( "li" );
                    list.appendChild( item );
                    item.appendChild( document.createTextNode( "Options: " ) );
                    /* Add option */
                    var text = document.createElement( "input" );
                    text.setAttribute( 'type', 'text' );
                    text.setAttribute( 'size', '40' );
                    text.setAttribute( 'id', 'vlm_elt_'+elt.getAttribute('name')+'_option' );
                    text.setAttribute( 'onkeypress', 'if( event.keyCode == 13 ) vlm_option("'+elt.getAttribute('name')+'",document.getElementById("vlm_elt_'+elt.getAttribute('name')+'_option").value );' );
                    item.appendChild( text );
                    item.appendChild( document.createTextNode( ' ' ) );
                    item.appendChild( create_button( 'Add option', 'vlm_option("'+elt.getAttribute('name')+'",document.getElementById("vlm_elt_'+elt.getAttribute('name')+'_option").value );' ) );
                    
                    var options = elt.getElementsByTagName( 'option' );
                    if( options.length > 0 )
                    {
                        var olist = document.createElement( "ul" );
                        item.appendChild( olist );
                        for( i = 0; i < options.length; i++ )
                        {
                            var item = document.createElement( "li" );
                            item.appendChild( document.createTextNode( options[i].firstChild.data ) );
                            olist.appendChild( item );
                        }
                    }
                    /* end of options */

                    /* Instances list */
                    var instances = elt.getElementsByTagName( 'instance' );
                    if( instances.length > 0 )
                    {
                        var item = document.createElement("li");
                        var ilist = document.createElement("ul");
                        list.appendChild( item );
                        item.appendChild(document.createTextNode("Instances:")); 
                        item.appendChild( ilist );
                        for( i = 0; i < instances.length; i++ )
                        {
                            var iname = instances[i].getAttribute( 'name' );
                            var istate = instances[i].getAttribute( 'state' );
                            var iposition = Number( instances[i].getAttribute( 'position' ) * 100);
                            var itime = Math.floor( instances[i].getAttribute( 'time' ) / 1000000);
                            var ilength = Math.floor( instances[i].getAttribute( 'length' ) / 1000000);
                            var irate = instances[i].getAttribute( 'rate' );
                            var ititle = instances[i].getAttribute( 'title' );
                            var ichapter = instances[i].getAttribute( 'chapter' );
                            var iseekable = instances[i].getAttribute( 'seekable' );
                            var iplaylistindex = instances[i].getAttribute( 'playlistindex' );
                            
                            var item = document.createElement( "li" );
                            item.appendChild( document.createTextNode( iname + ": " + istate + " (" + iplaylistindex + ") " + (iposition.toFixed(2)) + "%" + " " + format_time( itime ) + "/" + format_time( ilength ) ) );
                            ilist.appendChild( item );
                        }
                    }
                    /* end of instances list */
                    
                    nb.appendChild( list );
                    
                }
                else if( elt.nodeName == "schedule" )
                {
                    var nb = document.createElement( 'div' );
                    setclass( nb, 'list_element' );
                    vlms.appendChild( nb );

                    var nbname = document.createElement( 'b' );
                    nbname.appendChild( document.createTextNode( elt.getAttribute( 'name' ) ) );
                    nb.appendChild( nbname );
                    
                    if( elt.getAttribute( 'enabled' ) == 'yes' )
                    {
                        nb.appendChild( document.createTextNode( " enabled " ) );
                        nb.appendChild( create_button( "Disable", 'vlm_disable("'+elt.getAttribute( 'name' ) + '");' ) );
                    }
                    else
                    {
                        nb.appendChild( document.createTextNode( " disabled " ) );
                        nb.appendChild( create_button( "Enable", 'vlm_enable("'+elt.getAttribute( 'name' ) + '");' ) );
                    }

                    nb.appendChild( document.createTextNode( " " ) );
                    nb.appendChild( create_button( "Delete", 'vlm_delete("'+elt.getAttribute( 'name' ) + '");' ) );

                    var list = document.createElement( 'ul' );

                    var item = document.createElement( 'li' );
                    item.appendChild( document.createTextNode( "Date: " + elt.getAttribute( 'date' ) ) );
                    list.appendChild( item );

                    var item = document.createElement( 'li' );
                    item.appendChild( document.createTextNode( "Period (in seconds): " + elt.getAttribute( 'period' ) ) );
                    list.appendChild( item );
                    
                    var item = document.createElement( 'li' );
                    if( elt.getAttribute( 'repeat' ) == -1 )
                    {
                        item.appendChild( document.createTextNode( "Number of repeats left: for ever" ) );
                    }
                    else
                    {
                        item.appendChild( document.createTextNode( "Number of repeats left: " + elt.getAttribute( 'repeat' ) ) );
                    }
                    list.appendChild( item );
                    
                    var commands = elt.getElementsByTagName( 'command' );
                    for( i = 0; i < commands.length; i++ )
                    {
                        var item = document.createElement( "li" );
                        item.appendChild( document.createTextNode( "Command: " + commands[i].firstChild.data + " " ) );
                        list.appendChild( item );
                    }
                    
                    var item = document.createElement( 'li' );
                    var sel = document.createElement( 'select' );
                    sel.setAttribute( 'id', 'vlm_elt_'+elt.getAttribute('name')+'_action' );
                    sel.setAttribute( 'onchange', 'vlm_schedule_type_change("'+elt.getAttribute('name')+'");');
                    sel.appendChild( create_option( 'play', 'play' ) );
                    sel.appendChild( create_option( 'pause', 'pause' ) );
                    sel.appendChild( create_option( 'stop', 'stop' ) );
                    sel.appendChild( create_option( 'seek', 'seek' ) );
                    sel.appendChild( create_option( '(other)', '' ) );
                    item.appendChild( sel );

                    item.appendChild( document.createTextNode( " " ) );
                    var text = document.createElement( 'input' );
                    text.setAttribute( 'type', 'text' );
                    text.setAttribute( 'id', 'vlm_elt_'+elt.getAttribute('name')+'_name' );
                    text.setAttribute( 'size', '10' );
                    text.setAttribute( 'value', '(name)' );
                    text.setAttribute( 'onfocus', 'if( this.value == "(name)" ) this.value = "";' );
                    text.setAttribute( 'onblur', 'if( this.value == "" ) this.value = "(name)";' );
                    item.appendChild( text );

                    item.appendChild( document.createTextNode( " " ) );
                    text = document.createElement( 'input' );
                    text.setAttribute( 'type', 'text' );
                    text.setAttribute( 'id', 'vlm_elt_'+elt.getAttribute('name')+'_opt' );
                    text.setAttribute( 'size', '30' );
                    text.setAttribute( 'value', '(options)' );
                    text.setAttribute( 'onfocus', 'if( this.value == "(options)" ) this.value = "";' );
                    text.setAttribute( 'onblur', 'if( this.value == "" ) this.value = "(options)";' );
                    item.appendChild( text );
                    item.appendChild( document.createTextNode( " " ) );
                    item.appendChild( create_button( "Append command", 'vlm_schedule_append("' + elt.getAttribute( 'name' ) + '");') );
                    
                    list.appendChild( item );

                    nb.appendChild( list );
                    vlm_schedule_type_change( elt.getAttribute('name') );
                    
                }
                elt = elt.nextSibling;
            }
        }
    }
}

function vlm_cmd( cmd )
{
    loadXMLDoc( 'requests/vlm_cmd.xml?command='+encodeURIComponent(cmd), parse_vlm_cmd );
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
    document.getElementById( 'vlm_command' ).value = "setup "+name+" input "+sanitize_input( input );
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

function vlm_batch( batch )
{
    var i;
    var commands = batch.split( '\n' );
    for( i = 0; i < commands.length; i++ )
    {
        document.getElementById( 'vlm_command' ).value = commands[i];
        vlm_cmd( value( 'vlm_command' ) );
    }
}

function vlm_schedule_append( name )
{
    var act = document.getElementById( 'vlm_elt_' + name + '_action' ).value;
    document.getElementById( 'vlm_command' ).value = "setup " + name + " append ";

    var itemname = document.getElementById( 'vlm_elt_' + name + '_name' ).value;
    if( itemname == "(name)" ) itemname = "";

    var opt = document.getElementById( 'vlm_elt_' + name + '_opt' ).value;
    if( opt == "(options)" ) opt = "";
        
    if( act == '' )
    {
        document.getElementById( 'vlm_command' ).value += opt;
    }
    else
    {
        document.getElementById( 'vlm_command' ).value += 'control ' + itemname + " " + act + " " + opt;
    }
    vlm_cmd( value( 'vlm_command' ) );
}
function vlm_send( )
{
    vlm_cmd( value( 'vlm_command' ) );
}
