/*****************************************************************************
 * mosaic.js: VLC media player web interface - Mosaic specific functions
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

/**********************************************************************
 * 
 *********************************************************************/

var mosaic_alpha    = 255;
var mosaic_height   = 0;
var mosaic_width    = 0;
var mosaic_align    = 5;
var mosaic_xoffset  = 0;
var mosaic_yoffset  = 0;
var mosaic_vborder  = 0;
var mosaic_hborder  = 0;
var mosaic_position = 1;
var mosaic_rows     = 0;
var mosaic_cols     = 0;
var mosaic_delay    = 0;

var cell_width  = 0;
var cell_height = 0;

var streams = Object();
var cells   = Object();

function mosaic_init()
{
    document.getElementById( 'sout_transcode_extra' ).value = ",sfilter=mosaic}:bridge-in{offset=100";
    mosaic_size_change();

    /* Force usage of transcode in sout */
    document.getElementById( 'sout_vcodec_s' ).checked = 'checked';
    disable( 'sout_vcodec_s' );
    update_sout();
}

function mosaic_size_change()
{
    var x,y;

    var bg_width    = check_and_replace_int( "bg_width", "400" );
    var bg_height   = check_and_replace_int( "bg_height", "300" );

    mosaic_height   = check_and_replace_int( "mosaic_height", "100" );
    mosaic_width    = check_and_replace_int( "mosaic_width", "100" );
    mosaic_xoffset  = check_and_replace_int( "mosaic_xoffset", "10" );
    mosaic_yoffset  = check_and_replace_int( "mosaic_yoffset", "10" );
    mosaic_vborder  = check_and_replace_int( "mosaic_vborder", "5" );
    mosaic_hborder  = check_and_replace_int( "mosaic_hborder", "10" );
    mosaic_rows     = check_and_replace_int( "mosaic_rows", "1" );
    mosaic_cols     = check_and_replace_int( "mosaic_cols", "1" );
    
    cell_width  = Math.floor((mosaic_width-(mosaic_cols-1)*mosaic_hborder)/mosaic_cols);
    cell_height = Math.floor((mosaic_height-(mosaic_rows-1)*mosaic_vborder)/mosaic_rows);
    
    var mlayout = document.getElementById( "mosaic_layout" );

    while( mlayout.hasChildNodes() )
        mlayout.removeChild( mlayout.firstChild );

    mlayout.style.width = bg_width + "px";
    mlayout.style.height = bg_height + "px";
    if( mosaic_cols && mosaic_rows )
    {
        var mdt = document.createElement( 'div' );
        mdt.setAttribute( 'id',    'mosaic_dt'  );
        setclass( mdt, 'mosaic_tbl' );
        
        mdt.style.width  = mosaic_width   + "px";
        mdt.style.height = mosaic_height  + "px";
        mdt.style.top    = mosaic_yoffset + "px";
        mdt.style.left   = mosaic_xoffset + "px";

        var mtable = document.createElement( 'table' );
        mtable.setAttribute( 'id', 'mosaic_table' );
        mtable.style.top    = "-" + mosaic_vborder + "px";
        mtable.style.left   = "-" + mosaic_hborder + "px";
        mtable.style.width  = (1*mosaic_width +2*mosaic_hborder)  + "px";
        mtable.style.height = (1*mosaic_height+2*mosaic_vborder) + "px";
        mtable.style.borderSpacing = mosaic_hborder + "px " +
                                     mosaic_vborder + "px";

        var mtbody = document.createElement( 'tbody' );

        for( y = 0; y < mosaic_rows; y++ )
        {
            var mrow = document.createElement( 'tr' );
            for( x = 0; x < mosaic_cols; x++ )
            {
                var mcell = document.createElement( 'td' );
                setclass( mcell, 'mosaic_itm' );
                mcell.style.width  = cell_width  + "px";
                mcell.style.height = cell_height + "px";
                
                var id = x+'_'+y;
                var melt = create_button( cells[id] ? cells[id] : '?', 'mosaic_elt_choose(\"'+id+'\");' );
                melt.setAttribute( 'id', id );
                melt.setAttribute( 'title', 'Click to choose stream' );
                
                mcell.appendChild( melt );
                mrow.appendChild( mcell );
            }
            mtbody.appendChild( mrow );
        }
        mtable.appendChild( mtbody );
        mdt.appendChild( mtable );
        mlayout.appendChild( mdt );
    }
    mosaic_code_update();
}

function mosaic_add_input()
{
    streams[ addunderscores( value('mosaic_input_name') ) ] =
        value('mosaic_input');

    mosaic_feedback( addunderscores( value('mosaic_input_name') ) + " ( " + value('mosaic_input') + " ) added to input list.", true );

    var mlist = document.getElementById( "mosaic_list_content" );
    while( mlist.hasChildNodes() )
        mlist.removeChild( mlist.firstChild );
    
    for( var name in streams )
    {
        var mrl = streams[name];
        
        var minput = document.createElement( 'a' );
        minput.setAttribute( 'href', 'javascript:mosaic_elt_select(\''+name+'\');');
        minput.setAttribute( 'id', name );
        minput.setAttribute( 'value', mrl );
        
        var minputtxt = document.createTextNode( name );

        minput.appendChild( minputtxt );
        mlist.appendChild( minput );
        mlist.appendChild( document.createTextNode( " ( "+mrl+" )" ) );
        mlist.appendChild( document.createElement( 'br' ) );
    }
}

function mosaic_elt_select( id )
{
    hide( 'mosaic_list' );
    var ml = document.getElementById( 'mosaic_list' ).value;
    if( ml )
    {
        document.getElementById( ml ).value = id;
        cells[ ml ] = id;
        mosaic_code_update();
    }
}

function mosaic_elt_choose( id )
{
    document.getElementById( 'mosaic_list' ).value = id;
    show( 'mosaic_list' );
}

function mosaic_code_update()
{

    var code = document.getElementById( 'mosaic_code' );
    code.value =
"##################################\n"+
"## HTTP interface mosaic wizard ##\n"+
"##################################\n"+
"\n"+
"# Comment the following line if you don't want to reset your VLM configuration\n"+
"del all\n"+
"\n"+
"# Background options\n"+
"new   bg broadcast enabled\n"+
"setup bg input     " + sanitize_input( value( 'mosaic_bg_input' ) ) + "\n";
    if( value( 'mosaic_output' ) )
    {
        code.value +=
"setup bg output    " + value( 'mosaic_output' )+ "\n";
    }
    var o = /.*transcode.*/;
    if(! o.test( value( 'mosaic_output' ) ) )
    {
        code.value +=
"setup bg option    sub-filter=mosaic\n"+
"setup bg output    #bridge-in{offset=100}:display\n";
    }
    code.value+=
"\n"+
"# Mosaic options\n"+
"setup bg option mosaic-alpha="    + mosaic_alpha    + "\n"+
"setup bg option mosaic-height="   + mosaic_height   + "\n"+
"setup bg option mosaic-width="    + mosaic_width    + "\n"+
"setup bg option mosaic-align="    + mosaic_align    + "\n"+
"setup bg option mosaic-xoffset="  + mosaic_xoffset  + "\n"+
"setup bg option mosaic-yoffset="  + mosaic_yoffset  + "\n"+
"setup bg option mosaic-vborder="  + mosaic_vborder  + "\n"+
"setup bg option mosaic-hborder="  + mosaic_hborder  + "\n"+
"setup bg option mosaic-position=" + mosaic_position + "\n"+
"setup bg option mosaic-rows="     + mosaic_rows     + "\n"+
"setup bg option mosaic-cols="     + mosaic_cols     + "\n"+
"setup bg option mosaic-order=";
    for( y = 0; y < mosaic_rows; y++ )
    {
        for( x = 0; x < mosaic_cols; x++ )
        {
            var id = x+'_'+y;
            if( cells[id] )
                code.value += cells[id];
            else
                code.value += '_';
            if( y != mosaic_rows - 1 || x != mosaic_cols - 1 )
                code.value += ',';
        }
    }
    code.value += "\n"+
"setup bg option mosaic-delay="    + mosaic_delay    + "\n"+
"setup bg option mosaic-keep-picture\n"+
"\n"+
"# Input options\n";
    var x, y;
    for( y = 0; y < mosaic_rows; y++ )
    {
        for( x = 0; x < mosaic_cols; x++ )
        {
            var id = x+'_'+y;
            if( cells[id] )
            {
                var s = cells[id];
                code.value +=
"new   " + s + " broadcast enabled\n"+
"setup " + s + " input     " + sanitize_input( streams[s] ) + "\n"+
"setup " + s + " output #duplicate{dst=mosaic-bridge{id=" + s + ",width="+cell_width+",height="+cell_height+"},select=video,dst=bridge-out{id="+(y*mosaic_cols+x)+"},select=audio}\n"+
"\n";
            }
        }
    }
    code.value +=
"# Launch everything\n"+
"control bg play\n";
    for( y = 0; y < mosaic_rows; y++ )
    {
        for( x = 0; x < mosaic_cols; x++ )
        {
            var id = x+'_'+y;
            if( cells[id] )
            {
                var s = cells[id];
                code.value +=
"control " + s + " play\n";
            }
        }
    }
    code.value +=
"\n"+
"# end of mosaic batch\n";
}

function mosaic_batch( batch )
{
    var i;
    var commands = batch.split( '\n' );
    for( i = 0; i < commands.length; i++ )
    {
        mosaic_cmd( commands[i] );
    }
}

function mosaic_cmd( cmd )
{
    loadXMLDoc( 'requests/vlm_cmd.xml?command='+cmd.replace(/\#/g, '%23'), parse_mosaic_cmd );
}

function parse_mosaic_cmd()
{
    /* TODO */
}

function mosaic_stop()
{
    var cmd;
    cmd = "control bg stop\n";
    var x,y;
    for( y = 0; y < mosaic_rows; y++ )
    {
        for( x = 0; x < mosaic_cols; x++ )
        {
            var id = x+'_'+y;
            if( cells[id] )
            {
                var s = cells[id];
                cmd += "control " + s + " stop\n";
            }
        }
    }
    mosaic_batch( cmd );
}

function mosaic_feedback( msg, ok )
{
    var f = document.getElementById( "mosaic_feedback" );
    while( f.hasChildNodes() )
        f.removeChild( f.firstChild );

    f.style.fontWeight = "bold";
    if( ok )
        f.style.color = "#0f0";
    else
        f.style.color = "#f00";

    var t = document.createTextNode( ( ok ? "Info: " : "Error: " ) + msg );
    f.appendChild( t );

}
