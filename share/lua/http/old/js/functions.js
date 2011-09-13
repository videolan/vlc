/*****************************************************************************
 * functions.js: VLC media player web interface
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
 * $Id: functions.js 21264 2007-08-19 17:48:28Z dionoea $
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
 * Global variables
 *********************************************************************/

var old_time = 0;
var pl_cur_id;
var albumart_id = -1;

/**********************************************************************
 * Slider functions
 *********************************************************************/
 
var slider_mouse_down = 0;
var slider_dx = 0;

var input_options = new Array();

/* findPosX() from http://www.quirksmode.rg/js/indpos.html */
function findPosX(obj)
{
    var curleft = 0;
    if (obj.offsetParent)
    {
        while (obj.offsetParent)
        {
            curleft += obj.offsetLeft
            obj = obj.offsetParent;
        }
    }
    else if (obj.x)
        curleft += obj.x;
    return curleft;
}

function slider_seek( e, bar )
{
    seek(Math.floor(( e.clientX + document.body.scrollLeft - findPosX( bar )) / 4)+"%25");
}
function slider_down( e, point )
{
    slider_mouse_down = 1;
    slider_dx = e.clientX - findPosX( point );
}
function slider_up( e, bar )
{
    slider_mouse_down = 0;
    /* slider_seek( e, bar ); */
}
function slider_move( e, bar )
{
    if( slider_mouse_down == 1 )
    {
        var slider_position  = Math.floor( e.clientX - slider_dx + document.body.scrollLeft - findPosX( bar ));
        document.getElementById( 'main_slider_point' ).style.left = slider_position+"px";
        slider_seek( e, bar );
    }
}

/**********************************************************************
 * Misc utils
 *********************************************************************/

/* XMLHttpRequest wrapper */
function loadXMLDoc( url, callback )
{
  // branch for native XMLHttpRequest object
  if ( window.XMLHttpRequest )
  {
    req = new XMLHttpRequest();
    req.onreadystatechange = callback;
    req.open( "GET", url, true );
    req.send( null );
  // branch for IE/Windows ActiveX version
  }
  else if ( window.ActiveXObject )
  {
    req = new ActiveXObject( "Microsoft.XMLHTTP" );
    if ( req )
    {
      req.onreadystatechange = callback;
      req.open( "GET", url, true );
      req.send();
    }
  }
}

/* fomat time in second as hh:mm:ss */
function format_time( s )
{
    var hours = Math.floor(s/3600);
    var minutes = Math.floor((s/60)%60);
    var seconds = Math.floor(s%60);
    if( hours < 10 ) hours = "0"+hours;
    if( minutes < 10 ) minutes = "0"+minutes;
    if( seconds < 10 ) seconds = "0"+seconds;
    return hours+":"+minutes+":"+seconds;
}

/* delete all a tag's children and add a text child node */
function set_text( id, val )
{
    var elt = document.getElementById( id );
    while( elt.hasChildNodes() )
        elt.removeChild( elt.firstChild );
    elt.appendChild( document.createTextNode( val ) );
}

/* set item's 'element' attribute to value */
function set_css( item, element, value )
{
    for( var j = 0; j < document.styleSheets.length; j++ )
    {
        var cssRules = document.styleSheets[j].cssRules;
        if( !cssRules ) cssRules = document.styleSheets[j].rules;
        for( var i = 0; i < cssRules.length; i++)
        {
            if( cssRules[i].selectorText == item )
            {
                if( cssRules[i].style.setProperty )
                    cssRules[i].style.setProperty( element, value, null );
                else
                    cssRules[i].style.setAttribute( toCamelCase( element ), value );
                return;
            }
        }
    }
}

/* get item's 'element' attribute */
function get_css( item, element )
{
    for( var j = 0; j < document.styleSheets.length; j++ )
    {
        var cssRules = document.styleSheets[j].cssRules;
        if( !cssRules ) cssRules = document.styleSheets[j].rules;
        for( var i = 0; i < cssRules.length; i++)
        {
            if( cssRules[i].selectorText == item )
            {
                if( cssRules[i].style.getPropertyValue )
                    return cssRules[i].style.getPropertyValue( element );
                else
                    return cssRules[i].style.getAttribute( toCamelCase( element ) );
            }
        }
    }
}

function toggle_show( id )
{
    var element = document.getElementById( id );
    if( element.style.display == 'block' || element.style.display == '' )
    {
        element.style.display = 'none';
    }
    else
    {
        element.style.display = 'block';
    }
}
function toggle_show_node( id )
{
    var element = document.getElementById( 'pl_'+id );
    var img = document.getElementById( 'pl_img_'+id );
    if( element.style.display == 'block' || element.style.display == '' )
    {
        element.style.display = 'none';
        img.setAttribute( 'src', '/old/images/plus.png' );
        img.setAttribute( 'alt', '[+]' );
    }
    else
    {
        element.style.display = 'block';
        img.setAttribute( 'src', '/old/images/minus.png' );
        img.setAttribute( 'alt', '[-]' );
    }
}

function show( id ){ document.getElementById( id ).style.display = 'block'; }
function showinline( id ){ document.getElementById( id ).style.display = 'inline'; }

function hide( id ){ document.getElementById( id ).style.display = 'none'; }

function checked( id ){ return document.getElementById( id ).checked; }

function value( id ){ return document.getElementById( id ).value; }

function setclass( obj, value )
{
    obj.setAttribute( 'class', value ); /* Firefox */
    obj.setAttribute( 'className', value ); /* IE */
}

function radio_value( name )
{
    var radio = document.getElementsByName( name );
    for( var i = 0; i < radio.length; i++ )
    {
        if( radio[i].checked )
        {
            return radio[i].value;
        }
    }
    return "";
}

function check_and_replace_int( id, val )
{
    var objRegExp = /^\d+$/;
    if( value( id ) != ''
        && ( !objRegExp.test( value( id ) )
             || parseInt( value( id ) ) < 1 ) )
        return document.getElementById( id ).value = val;
    return document.getElementById( id ).value;
}

function addslashes( str ){ return str.replace(/\'/g, '\\\''); }
function escapebackslashes( str ){ return str.replace(/\\/g, '\\\\'); }

function toCamelCase( str )
{
    str = str.split( '-' );
    var cml = str[0];
    for( var i=1; i<str.length; i++)
        cml += str[i].charAt(0).toUpperCase()+str[i].substring(1);
    return cml;
}

function disable( id ){ document.getElementById( id ).disabled = true; }

function enable( id ){ document.getElementById( id ).disabled = false; }

function button_over( element ){ element.style.border = "1px solid #000"; }

function button_out( element ){ element.style.border = "1px solid #fff"; }
function button_out_menu( element ){ element.style.border = "1px solid transparent"; }

function show_menu( id ){ document.getElementById(id).style.display = 'block'; }
function hide_menu( id ){ document.getElementById(id).style.display = 'none'; }

/* toggle show help under the buttons */
function toggle_btn_text()
{
    if( get_css( '.btn_text', 'display' ) == 'none' )
    {
        set_css( '.btn_text', 'display', 'block' );
    }
    else
    {
        set_css( '.btn_text', 'display', 'none' );
    }
}

function clear_children( elt )
{   
    if( elt )
        while( elt.hasChildNodes() )
            elt.removeChild( elt.firstChild );
}

/**********************************************************************
 * Interface actions
 *********************************************************************/
/* input actions */
function in_playenqueue( cmd )
{
    var input = value('input_mrl');
    var url = 'requests/status.xml?command=in_'+cmd+'&input='+encodeURIComponent( addslashes(escapebackslashes(input)) );
    for( i in input_options )
        if( input_options[i] != ':option=value' )
            url += '&option='+encodeURIComponent( addslashes(escapebackslashes(input_options[i]) ));
    loadXMLDoc( url, parse_status );
    setTimeout( 'update_playlist()', 1000 );
}

function in_play()
{
    in_playenqueue( 'play' );
}

function in_enqueue()
{
    in_playenqueue( 'enqueue' );
}

/* playlist actions */
function pl_play( id )
{
    loadXMLDoc( 'requests/status.xml?command=pl_play&id='+id, parse_status );
    pl_cur_id = id;
    setTimeout( 'update_playlist()', 1000 );
}
function pl_pause()
{
    loadXMLDoc( 'requests/status.xml?command=pl_pause&id='+pl_cur_id, parse_status );
}
function pl_stop()
{
    loadXMLDoc( 'requests/status.xml?command=pl_stop', parse_status );
    setTimeout( 'update_playlist()', 1000 );
}
function pl_next()
{
    loadXMLDoc( 'requests/status.xml?command=pl_next', parse_status );
    setTimeout( 'update_playlist()', 1000 );
}
function pl_previous()
{
    loadXMLDoc( 'requests/status.xml?command=pl_previous', parse_status );
    setTimeout( 'update_playlist()', 1000 );
}
function pl_delete( id )
{
    loadXMLDoc( 'requests/status.xml?command=pl_delete&id='+id, parse_status );
    setTimeout( 'update_playlist()', 1000 );
}
function pl_empty()
{
    loadXMLDoc( 'requests/status.xml?command=pl_empty', parse_status );
    setTimeout( 'update_playlist()', 1000 );
}
function pl_sort( sort, order )
{
    loadXMLDoc( 'requests/status.xml?command=pl_sort&id='+order+'&val='+sort, parse_status );
    setTimeout( 'update_playlist()', 1000 );
}
function pl_shuffle()
{
    loadXMLDoc( 'requests/status.xml?command=pl_random', parse_status );
    setTimeout( 'update_playlist()', 1000 );
}
function pl_loop()
{
    loadXMLDoc( 'requests/status.xml?command=pl_loop', parse_status );
}
function pl_repeat()
{
    loadXMLDoc( 'requests/status.xml?command=pl_repeat', parse_status );
}
function pl_sd( value )
{
    loadXMLDoc( 'requests/status.xml?command=pl_sd&val='+value, parse_status );
}

/* misc actions */
function volume_down()
{
    loadXMLDoc( 'requests/status.xml?command=volume&val=-20', parse_status );
}
function volume_up()
{
    loadXMLDoc( 'requests/status.xml?command=volume&val=%2B20', parse_status );
}
function volume_mute()
{
    loadXMLDoc( 'requests/status.xml?command=volume&val=0', parse_status );
}
function seek( pos )
{
    loadXMLDoc( 'requests/status.xml?command=seek&val='+pos, parse_status );
}
function fullscreen()
{
    loadXMLDoc( 'requests/status.xml?command=fullscreen', parse_status );
}
function snapshot()
{
    loadXMLDoc( 'requests/status.xml?command=snapshot', parse_status );
}
function hotkey( str )
{
    /* Use hotkey name (without the "key-" part) as the argument to simulate a hotkey press */
    loadXMLDoc( 'requests/status.xml?command=key&val='+str, parse_status );
}
function update_status()
{
    loadXMLDoc( 'requests/status.xml', parse_status );
}
function update_playlist()
{
    loadXMLDoc( 'requests/playlist.xml', parse_playlist );
}
function update_playlist_search(key)
{
    loadXMLDoc( 'requests/playlist.xml?search='+encodeURIComponent(key), parse_playlist )
}
function reset_search()
{
    var search = document.getElementById('search')
    if( search )
    {
        search.value = '<search>'
        update_playlist_search('')
    }
}
function audiodelay_down()
{
    var curdel = parseFloat(req.responseXML.documentElement.getElementsByTagName( 'audiodelay' )[0].firstChild.data);
    var curdelnew = curdel - 0.05;
    curdelnew=curdelnew.toFixed(2);
    loadXMLDoc( 'requests/status.xml?command=audiodelay&val='+encodeURIComponent(curdelnew), parse_status );
}
function audiodelay_up()
{
    var curdel = parseFloat(req.responseXML.documentElement.getElementsByTagName( 'audiodelay' )[0].firstChild.data);
    var curdelnew = curdel + 0.05;
    curdelnew=curdelnew.toFixed(2);
    loadXMLDoc( 'requests/status.xml?command=audiodelay&val='+encodeURIComponent(curdelnew), parse_status );
}
function playrate_down()
{
    var currate = parseFloat(req.responseXML.documentElement.getElementsByTagName( 'rate' )[0].firstChild.data);
    var curratenew = currate - 0.05;
    curratenew=curratenew.toFixed(2);
    loadXMLDoc( 'requests/status.xml?command=rate&val='+encodeURIComponent(curratenew), parse_status );
}
function playrate_up()
{
    var currate = parseFloat(req.responseXML.documentElement.getElementsByTagName( 'rate' )[0].firstChild.data);
    var curratenew = currate + 0.05;
    curratenew=curratenew.toFixed(2);
    loadXMLDoc( 'requests/status.xml?command=rate&val='+encodeURIComponent(curratenew), parse_status );
}
function subdel_down()
{
    var curdel = parseFloat(req.responseXML.documentElement.getElementsByTagName( 'subtitledelay' )[0].firstChild.data);
    var curdelnew = curdel - 0.05;
    curdelnew=curdelnew.toFixed(2);
    loadXMLDoc( 'requests/status.xml?command=subdelay&val='+encodeURIComponent(curdelnew), parse_status );
}
function subdel_up()
{
    var curdel = parseFloat(req.responseXML.documentElement.getElementsByTagName( 'subtitledelay' )[0].firstChild.data);
    var curdelnew = curdel + 0.05;
    curdelnew=curdelnew.toFixed(2);
    loadXMLDoc( 'requests/status.xml?command=subdelay&val='+encodeURIComponent(curdelnew), parse_status );
}

/**********************************************************************
 * Parse xml replies to XMLHttpRequests
 *********************************************************************/
/* parse request/status.xml */
function parse_status()
{
    if( req.readyState == 4 )
    {
        if( req.status == 200 )
        {
            var status = req.responseXML.documentElement;
            var timetag = status.getElementsByTagName( 'time' );
            if( timetag.length > 0 )
            {
                var new_time = timetag[0].firstChild.data;
            }
            else
            {
                new_time = old_time;
            }
            var lengthtag = status.getElementsByTagName( 'length' );
            var length;
            if( lengthtag.length > 0 )
            {
                length = lengthtag[0].firstChild.data;
            }
            else
            {
                length = 0;
            }
            var slider_position;
            positiontag = status.getElementsByTagName( 'position' );
            if( length < 100 && positiontag.length > 0 )
            {
                slider_position = ( positiontag[0].firstChild.data * 4 ) + "px";
            }
            else if( length > 0 )
            {
                /* this is more precise if length > 100 */
                slider_position = Math.floor( ( new_time * 400 ) / length ) + "px";
            }
            else
            {
                slider_position = 0;
            }
            if( old_time > new_time )
                setTimeout('update_playlist()',50);
            old_time = new_time;
            set_text( 'time', format_time( new_time ) );
            set_text( 'length', format_time( length ) );
            var audio_delay = (parseFloat(req.responseXML.documentElement.getElementsByTagName( 'audiodelay' )[0].firstChild.data).toFixed(2))*1000;
            set_text( 'a_del', audio_delay ); 
            var play_rate = (parseFloat(req.responseXML.documentElement.getElementsByTagName( 'rate' )[0].firstChild.data).toFixed(2));
            set_text( 'p_rate', play_rate ); 
            var subs_delay = (parseFloat(req.responseXML.documentElement.getElementsByTagName( 'subtitledelay' )[0].firstChild.data).toFixed(2))*1000;
            set_text( 's_del', subs_delay ); 
            if( status.getElementsByTagName( 'volume' ).length != 0 )
                set_text( 'volume', Math.floor(status.getElementsByTagName( 'volume' )[0].firstChild.data/5.12)+'%' );
            var statetag = status.getElementsByTagName( 'state' );
            if( statetag.length > 0 )
            {
            	set_text( 'state', statetag[0].firstChild.data );
            }
            else
            {
                set_text( 'state', '(?)' );
            }
            if( slider_mouse_down == 0 )
            {
                document.getElementById( 'main_slider_point' ).style.left = slider_position;
            }
            var statustag = status.getElementsByTagName( 'state' );
            if( statustag.length > 0 ? statustag[0].firstChild.data == "playing" : 0 )
            {
                document.getElementById( 'btn_pause_img' ).setAttribute( 'src', '/old/images/pause.png' );
                document.getElementById( 'btn_pause_img' ).setAttribute( 'alt', 'Pause' );
                document.getElementById( 'btn_pause' ).setAttribute( 'title', 'Pause' );
            }
            else
            {
                document.getElementById( 'btn_pause_img' ).setAttribute( 'src', '/old/images/play.png' );
                document.getElementById( 'btn_pause_img' ).setAttribute( 'alt', 'Play' );
                document.getElementById( 'btn_pause' ).setAttribute( 'title', 'Play' );
            }

            var randomtag = status.getElementsByTagName( 'random' );
            if( randomtag.length > 0 ? randomtag[0].firstChild.data == "1" : 0)
                setclass( document.getElementById( 'btn_shuffle'), 'on' );
            else
                setclass( document.getElementById( 'btn_shuffle'), 'off' );
               
            var looptag = status.getElementsByTagName( 'loop' );
            if( looptag.length > 0 ? looptag[0].firstChild.data == "1" : 0)
                setclass( document.getElementById( 'btn_loop'), 'on' );
            else
                setclass( document.getElementById( 'btn_loop'), 'off' );

            var repeattag = status.getElementsByTagName( 'repeat' );
            if( repeattag.length > 0 ? repeattag[0].firstChild.data == "1" : 0 )
                setclass( document.getElementById( 'btn_repeat'), 'on' );
            else
                setclass( document.getElementById( 'btn_repeat'), 'off' );

            var tree = document.createElement( "ul" );
            var categories = status.getElementsByTagName( 'category' );
            var i;
            for( i = 0; i < categories.length; i++ )
            {
                var item = document.createElement( "li" );
                item.appendChild( document.createTextNode( categories[i].getAttribute( 'name' ) ) );
                var subtree = document.createElement( "dl" );
                var infos = categories[i].getElementsByTagName( 'info' );
                var j;
                for( j = 0; j < infos.length; j++ )
                {
                    var subitem = document.createElement( "dt" );
                    subitem.appendChild( document.createTextNode( infos[j].getAttribute( 'name' ) ) );
                    subtree.appendChild( subitem );
                    if( infos[j].hasChildNodes() )
                    {
                        var subitem = document.createElement( "dd" );
                        subitem.appendChild( document.createTextNode( infos[j].firstChild.data ) );
                        subtree.appendChild( subitem );
                    }
                }
                item.appendChild( subtree );
                tree.appendChild( item );
            }
            var infotree = document.getElementById('infotree' );
            clear_children( infotree );
            infotree.appendChild( tree );
            
        }
        else
        {
            /*alert( 'Error! HTTP server replied: ' + req.status );*/
        }
    }
}

/* parse playlist.xml */
function parse_playlist()
{
    if( req.readyState == 4 )
    {
        if( req.status == 200 )
        {
            var answer = req.responseXML.documentElement;
            var playtree = document.getElementById( 'playtree' );
            var pos = document.createElement( "div" );
            var pos_top = pos;
            var elt = answer.firstChild;
            
            pl_cur_id = 0;  /* changed to the current id is there actually
                             * is a current id */
            while( elt )
            {
                if( elt.nodeName == "node" )
                {
                    if( pos.hasChildNodes() )
                        pos.appendChild( document.createElement( "br" ) );
                    var nda = document.createElement( 'a' );
                    nda.setAttribute( 'href', 'javascript:toggle_show_node(\''+elt.getAttribute( 'id' )+'\');' );
                    var ndai = document.createElement( 'img' );
                    ndai.setAttribute( 'src', '/old/images/minus.png' );
                    ndai.setAttribute( 'alt', '[-]' );
                    ndai.setAttribute( 'id', 'pl_img_'+elt.getAttribute( 'id' ) );
                    nda.appendChild( ndai );
                    pos.appendChild( nda );
                    pos.appendChild( document.createTextNode( ' ' + elt.getAttribute( 'name' ) ) );

                    if( elt.getAttribute( 'ro' ) == 'rw' )
                    {
                        pos.appendChild( document.createTextNode( ' ' ) );
                        var del = document.createElement( "a" );
                        del.setAttribute( 'href', 'javascript:pl_delete('+elt.getAttribute( 'id' )+')' );
                            var delimg = document.createElement( "img" );
                            delimg.setAttribute( 'src', '/old/images/delete_small.png' );
                            delimg.setAttribute( 'alt', '(delete)' );
                        del.appendChild( delimg );
                        pos.appendChild( del );
                    }

                    var nd = document.createElement( "div" );
                    setclass( nd, 'pl_node' );
                    nd.setAttribute( 'id', 'pl_'+elt.getAttribute( 'id' ) );
                    pos.appendChild( nd );
                }
                else if( elt.nodeName == "leaf" )
                {
                    if( pos.hasChildNodes() )
                    pos.appendChild( document.createElement( "br" ) );
                    var pl = document.createElement( "a" );
                    setclass( pl, 'pl_leaf' );
                    pl.setAttribute( 'href', 'javascript:pl_play('+elt.getAttribute( 'id' )+');' );
                    pl.setAttribute( 'id', 'pl_'+elt.getAttribute( 'id' ) );
                    if( elt.getAttribute( 'current' ) == 'current' )
                    {
                        pl.style.fontWeight = 'bold';
                        var nowplaying = document.getElementById( 'nowplaying' );
                        clear_children( nowplaying );
                        nowplaying.appendChild( document.createTextNode( elt.getAttribute( 'name' ) ) );
                        pl.appendChild( document.createTextNode( '* '));
                        pl_cur_id = elt.getAttribute( 'id' );
                    }
                    pl.setAttribute( 'title', elt.getAttribute( 'uri' ));
                    pl.appendChild( document.createTextNode( elt.getAttribute( 'name' ) ) );
                    var duration = elt.getAttribute( 'duration' );
                    if( duration > 0 )
                        pl.appendChild( document.createTextNode( " (" + format_time( elt.getAttribute( 'duration' ) ) + ")" ) );
                    pos.appendChild( pl );

                    if( elt.getAttribute( 'ro' ) == 'rw' )
                    {
                        pos.appendChild( document.createTextNode( ' ' ) );
                        var del = document.createElement( "a" );
                        del.setAttribute( 'href', 'javascript:pl_delete('+elt.getAttribute( 'id' )+')' );
                            var delimg = document.createElement( "img" );
                            delimg.setAttribute( 'src', '/old/images/delete_small.png' );
                            delimg.setAttribute( 'alt', '(delete)' );
                        del.appendChild( delimg );
                        pos.appendChild( del );
                    }
                }
                if( elt.firstChild )
                {
                    elt = elt.firstChild;
                    pos = pos.lastChild;
                }
                else if( elt.nextSibling )
                {
                    elt = elt.nextSibling;
                    pos = pos;
                }
                else
                {
                    while( ! elt.parentNode.nextSibling )
                    {
                        elt = elt.parentNode;
                        if( ! elt.parentNode ) break;
                        pos = pos.parentNode;
                    }
                    if( ! elt.parentNode ) break;
                    elt = elt.parentNode.nextSibling;
                    pos = pos.parentNode;
                }
            }
            clear_children( playtree );
            playtree.appendChild( pos_top );
        }
        else
        {
            /*alert( 'Error! HTTP server replied: ' + req.status );*/
        }
    }
}

/* parse browse.xml */
function parse_browse_dir( )
{
    if( req.readyState == 4 )
    {
        if( req.status == 200 )
        {
            var answer = req.responseXML.documentElement;
            if( !answer ) return;
            var browser = document.getElementById( 'browser' );
            var pos = document.createElement( "div" );
            var elt = answer.firstChild;
            while( elt )
            {
                if( elt.nodeName == "element" )
                {
                    var item = document.createElement( "a" );
                    setclass( item, 'browser' );
                    if( elt.getAttribute( 'type' ) == 'dir' )
                    {
                        item.setAttribute( 'href', 'javascript:browse_dir(\''+addslashes(escapebackslashes(elt.getAttribute( 'path' )))+'\');');
                    }
                    else
                    {
                        item.setAttribute( 'href', 'javascript:browse_path(\''+addslashes(escapebackslashes(elt.getAttribute( 'path' )))+'\');' );
                    }
                    item.appendChild( document.createTextNode( elt.getAttribute( 'name' ) ) );
                    pos.appendChild( item );
                    if( elt.getAttribute( 'type' ) == 'dir' )
                    {
                        pos.appendChild( document.createTextNode( ' ' ) );
                        var item = document.createElement( "a" );
                        setclass( item, 'browser' );
                        item.setAttribute( 'href', 'javascript:browse_path(\''+addslashes(escapebackslashes(elt.getAttribute( 'path' )))+'\');');
                        item.appendChild( document.createTextNode( '(select)' ) );
                        pos.appendChild( item );
                    }
                    pos.appendChild( document.createElement( "br" ) );
                }
                elt = elt.nextSibling;
            }
            clear_children( browser );
            browser.appendChild( pos );
        }
        else
        {
            /*alert( 'Error! HTTP server replied: ' + req.status );*/
        }
    }
}

/**********************************************************************
 * Input dialog functions
 *********************************************************************/
function hide_input( )
{
    document.getElementById( 'input_file' ).style.display = 'none';
    document.getElementById( 'input_disc' ).style.display = 'none';
    document.getElementById( 'input_network' ).style.display = 'none';
    document.getElementById( 'input_fake' ).style.display = 'none';
}

/* update the input MRL using data from the input file helper */
/* FIXME ... subs support */
function update_input_file()
{
    var mrl = document.getElementById( 'input_mrl' );

    mrl.value = value( 'input_file_filename' );
}

/* update the input MRL using data from the input disc helper */
function update_input_disc()
{
    var mrl     = document.getElementById( 'input_mrl' );
    var type    = radio_value( "input_disc_type" );
    var device  = value( "input_disc_dev" );

    var title   = check_and_replace_int( 'input_disc_title', 0 );
    var chapter = check_and_replace_int( 'input_disc_chapter', 0 );
    var subs    = check_and_replace_int( 'input_disc_subtrack', '' );
    var audio   = check_and_replace_int( 'input_disc_audiotrack', 0 );

    mrl.value = "";

    if( type == "dvd" )
    {
        mrl.value += "dvd://";
    }
    else if( type == "dvdsimple" )
    {
        mrl.value += "dvdsimple://";
    }
    else if( type == "vcd" )
    {
        mrl.value += "vcd://";
    }
    else if( type == "cdda" )
    {
        mrl.value += "cdda://";
    }

    mrl.value += device;

    if( title )
    {
        mrl.value += "@"+title;
        if( chapter && type != "cdda" )
            mrl.value += ":"+chapter;
    }

    remove_input_options( ':sub-track' );
    remove_input_options( ':audio-track' );

    if( type != "cdda" )
    {
        if( subs != '' )
            add_input_option( ":sub-track="+subs );
        if( audio != '' )
            add_input_option( ":audio-track="+audio );
    }

}

/* update the input MRL using data from the input network helper */
function update_input_net()
{
    var mrl = document.getElementById( 'input_mrl' );
    var type = radio_value( "input_net_type" );
    
    check_and_replace_int( 'input_net_udp_port', 1234 );
    check_and_replace_int( 'input_net_udpmcast_port', 1234 );

    mrl.value = "";

    if( type == "udp" )
    {
        mrl.value += "udp://";
        if( value( 'input_net_udp_port' ) )
            mrl.value += ":"+value( 'input_net_udp_port' );
    }
    else if( type == "udpmcast" )
    {
        mrl.value += "udp://@"+value( 'input_net_udpmcast_address');
        if( value( 'input_net_udpmcast_port' ) )
            mrl.value += ":"+value( 'input_net_udpmcast_port' );
    }
    else if( type == "http" )
    {
        var url = value( 'input_net_http_url' );
        if( url.substring(0,7) != "http://"
            && url.substring(0,8) != "https://"
            && url.substring(0,6) != "ftp://"
            && url.substring(0,6) != "mms://"
            && url.substring(0,7) != "mmsh://" )
            mrl.value += "http://";
        mrl.value += url;
    }
    else if( type == "rtsp" )
    {
        var url = value( 'input_net_rtsp_url' );
        if( url.substring(0,7) != "rtsp://" )
            mrl.value += "rtsp://";
        mrl.value += url;
    }

    remove_input_options( ':access-filter' );
    if( checked( "input_net_timeshift" ) )
        add_input_option( ":access-filter=timeshift" );
}

/* update the input MRL using data from the input fake helper */
function update_input_fake()
{
    remove_input_options( ":fake" );
    var mrl = document.getElementById( 'input_mrl' );

    mrl.value = "fake://";

    add_input_option( ":fake-file=" + value( "input_fake_filename" ) );

    if( value( "input_fake_width" ) )
        add_input_option( ":fake-width=" + value( "input_fake_width" ) );
    if( value( "input_fake_height" ) )
        add_input_option( ":fake-height=" + value( "input_fake_height" ) );
    if( value( "input_fake_ar" ) )
        add_input_option( ":fake-ar=" + value( "input_fake_ar" ) );
}

/**********************************************************************
 * Sout dialog functions
 *********************************************************************/
/* toggle show the full sout interface */
function toggle_show_sout_helper()
{
    var element = document.getElementById( "sout_helper" );
    if( element.style.display == 'block' )
    {
        element.style.display = 'none';
        document.getElementById( "sout_helper_toggle" ).value = 'Full sout interface';
    }
    else
    {
        element.style.display = 'block';
        document.getElementById( "sout_helper_toggle" ).value = 'Hide sout interface';
    }
}

/* update the sout MRL using data from the sout_helper */
function update_sout()
{
    var option = "";
    /* Remove all options starting with :sout since we're going to write them
     * again. */
    remove_input_options( ":sout" );

    check_and_replace_int( 'sout_http_port', 8080 );
    check_and_replace_int( 'sout_mmsh_port', 8080 );
    check_and_replace_int( 'sout_rtp_port', 1234 );
    check_and_replace_int( 'sout_udp_port', 1234 );
    check_and_replace_int( 'sout_ttl', 1 );

    if( checked( 'sout_soverlay' ) )
    {
        disable( 'sout_scodec' );
        disable( 'sout_sub' );
    }
    else
    {
        enable( 'sout_scodec' );
        enable( 'sout_sub' );
    }

    var transcode =  checked( 'sout_vcodec_s' ) || checked( 'sout_acodec_s' )
                  || checked( 'sout_sub' )      || checked( 'sout_soverlay' );

    if( transcode )
    {
        option = ":sout=#transcode{";
        var alot = false; /* alot == at least one transcode */
        if( checked( 'sout_vcodec_s' ) )
        {
            option += "vcodec="+value( 'sout_vcodec' )+",vb="+value( 'sout_vb' )+",scale="+value( 'sout_scale' );
            alot = true;
        }
        if( checked( 'sout_acodec_s' ) )
        {
            if( alot ) option += ",";
            option += "acodec="+value( 'sout_acodec' )+",ab="+value( 'sout_ab' );
            if( value( 'sout_channels' ) )
                option += ",channels="+value( 'sout_channels' );
            alot = true;
        }
        if( checked( 'sout_soverlay' ) )
        {
            if( alot ) option += ",";
            option += "soverlay";
            alot = true;
        }
        else if( checked( 'sout_sub' ) )
        {
            if( alot ) option += ",";
            option += "scodec="+value( 'sout_scodec' );
            alot = true;
        }
        option += value( 'sout_transcode_extra' );
            
        option += "}";

    }

    var output = checked( 'sout_display' ) + checked( 'sout_file' )
               + checked( 'sout_http' )    + checked( 'sout_mmsh' )
               + checked( 'sout_rtp' )     + checked( 'sout_udp' );

    if( output )
    {
        if( transcode )
            option += ":";
        else
            option += ":sout=#";
        var aloo = false; /* aloo == at least one output */
        var mux = radio_value( 'sout_mux' );
        var ttl = parseInt( value( 'sout_ttl' ) );
        if( output > 1 ) option += "duplicate{";
        if( checked( 'sout_display' ) )
        {
            if( output > 1 ) option += "dst="
            option += "display";
            aloo = true;
        }
        if( checked( 'sout_file' ) )
        {
            if( aloo ) option += ",";
            if( output > 1 ) option += "dst="
            option += "std{access=file,mux="+mux+",dst="+value( 'sout_file_filename' )+"}";
            aloo = true;
        }
        if( checked( 'sout_http' ) )
        {
            if( aloo ) option += ",";
            if( output > 1 ) option += "dst="
            option += "std{access=http,mux="+mux+",dst="+value( 'sout_http_addr' );
            if( value( 'sout_http_port' ) )
                option += ":"+value( 'sout_http_port' );
            option += "}";
            aloo = true;
        }
        if( checked( 'sout_mmsh' ) )
        {
            if( aloo ) option += ",";
            if( output > 1 ) option += "dst="
            option += "std{access=mmsh,mux="+mux+",dst="+value( 'sout_mmsh_addr' );
            if( value( 'sout_mmsh_port' ) )
                option += ":"+value( 'sout_mmsh_port' );
            option += "}";
            aloo = true;
        }
        if( checked( 'sout_rtp' ) )
        {
            if( aloo ) option += ",";
            if( output > 1 ) option += "dst="
            option += "std{access=rtp";
            if( ttl ) option += "{ttl="+ttl+"}";
            option += ",mux="+mux+",dst="+value( 'sout_rtp_addr' );
            if( value( 'sout_rtp_port' ) )
                option += ":"+value( 'sout_rtp_port' );
            if( checked( 'sout_sap' ) )
            {
                option += ",sap";
                if( value( 'sout_sap_group' ) != '' )
                {
                    option += ",group=\""+value( 'sout_sap_group' )+"\"";
                }
                option += ",name=\""+value( 'sout_sap_name' )+"\"";
            }
            option += "}";
            aloo = true;
        }
        if( checked( 'sout_udp' ) )
        {
            if( aloo ) option += ",";
            if( output > 1 ) option += "dst="
            option += "std{access=udp";
            if( ttl ) option += "{ttl="+ttl+"}";
            option += ",mux="+mux+",dst="+value( 'sout_udp_addr' );
            if( value('sout_udp_port' ) )
                option += ":"+value( 'sout_udp_port' );
            if( checked( 'sout_sap' ) )
            {
                option += ",sap";
                if( value( 'sout_sap_group' ) != '' )
                {
                    option += ",group=\""+value( 'sout_sap_group' )+"\"";
                }
                option += ",name=\""+value( 'sout_sap_name' )+"\"";
            }
            option += "}";
            aloo = true;
        }
        if( output > 1 ) option += "}";
    }

    if( option != "" )
        input_options.push( option );

    if( ( transcode || output ) && checked( 'sout_all' ) )
        input_options.push( ":sout-all" );

    var mrl = document.getElementById( 'sout_mrl' );
    mrl.value = option;

    refresh_input_options_list();
}

/* reset sout mrl value */
function reset_sout()
{
    document.getElementById('sout_mrl').value = value('sout_old_mrl');
}

/* save sout mrl value */
function save_sout()
{
    document.getElementById('sout_old_mrl').value = value('sout_mrl');
}

function refresh_input_options_list()
{
    var iol = document.getElementById( 'input_options_list' );
    clear_children( iol );
    input_options.sort();
    for( i in input_options )
    {
        var o = document.createElement( 'div' );
        var ot = document.createElement( 'input' );
        ot.setAttribute( 'type', 'text' );
        ot.setAttribute( 'size', '60' );
        ot.setAttribute( 'value', input_options[i] );
        ot.setAttribute( 'id', 'input_option_item_'+i );
        ot.setAttribute( 'onchange', 'javascript:save_input_option('+i+',this.value);' );
        ot.setAttribute( 'onfocus', 'if( this.value == ":option=value" ) this.value = ":";' );
        ot.setAttribute( 'onblur', 'if( this.value == ":" ) this.value = ":option=value";' );
        o.appendChild( ot );
        var od = document.createElement( 'a' );
        od.setAttribute( 'href', 'javascript:delete_input_option('+i+');' );
        var delimg = document.createElement( "img" );
        delimg.setAttribute( 'src', '/old/images/delete_small.png' );
        delimg.setAttribute( 'alt', '(delete)' );
        od.appendChild( delimg );
        o.appendChild( od );
        iol.appendChild( o );
    }
}

function delete_input_option( i )
{
    input_options.splice(i,1);
    refresh_input_options_list();
}

function save_input_option( i, value )
{
    input_options[i] = value;
    refresh_input_options_list();
}

function add_input_option( value )
{
    input_options.push( value );
    refresh_input_options_list();
}

function remove_input_options( prefix )
{
    for( i in input_options )
        if( input_options[i].substring( 0, prefix.length ) == prefix )
        {
            delete input_options[i];
            i--;
        }
}


/**********************************************************************
 * Browser dialog functions
 *********************************************************************/
/* only browse() should be called directly */
function browse( dest )
{
    document.getElementById( 'browse_dest' ).value = dest;
    document.getElementById( 'browse_lastdir' ).value;
    browse_dir( document.getElementById( 'browse_lastdir' ).value );
    show( 'browse' );
}
function browse_dir( dir )
{
    document.getElementById( 'browse_lastdir' ).value = dir;
    loadXMLDoc( 'requests/browse.xml?dir='+encodeURIComponent(dir), parse_browse_dir );
}
function browse_path( p )
{
    document.getElementById( value( 'browse_dest' ) ).value = p;
    hide( 'browse' );
    document.getElementById( value( 'browse_dest' ) ).focus();
}
function refresh_albumart( force )
{
    if( albumart_id != pl_cur_id || force )
    {
        var now = new Date();
        var albumart = document.getElementById( 'albumart' );
        albumart.src = '/art?timestamp=' + now.getTime();
        albumart_id = pl_cur_id;
    }
}
/**********************************************************************
 * Periodically update stuff in the interface
 *********************************************************************/
function loop_refresh_status()
{
    setTimeout( 'loop_refresh_status()', 1000 );
    update_status();
}
function loop_refresh_playlist()
{
    /* setTimeout( 'loop_refresh_playlist()', 10000 ); */
    update_playlist();
}
function loop_refresh_albumart()
{
    setTimeout( 'loop_refresh_albumart()', 1000 );
    refresh_albumart( false );
}
function loop_refresh()
{
    setTimeout( 'loop_refresh_status()', 1 );
    setTimeout( 'loop_refresh_playlist()', 1 );
    setTimeout( 'loop_refresh_albumart()', 1 );
}

