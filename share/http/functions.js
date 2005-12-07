/*****************************************************************************
 * functions.js: VLC media player web interface
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
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

function format_time( s )
{
    hours = Math.floor(s/3600);
    minutes = Math.floor((s/60)%60);
    seconds = s%60;
    if( hours < 10 ) hours = "0"+hours;
    if( minutes < 10 ) minutes = "0"+minutes;
    if( seconds < 10 ) seconds = "0"+seconds;
    return hours+":"+minutes+":"+seconds;
}


function in_play()
{
    input = value('input_mrl');
    if( value('sout_mrl') != '' )
        input += ' '+value('sout_mrl');
    url = 'requests/status.xml?command=in_play&input='+escape(input);
    loadXMLDoc( url, parse_status );
    setTimeout( 'update_playlist()', 1000 );
}
function in_enqueue()
{
    input = value('input_mrl');
    if( value('sout_mrl') != '' )
        input += ' '+value('sout_mrl');
    url = 'requests/status.xml?command=in_enqueue&input='+escape(input);
    loadXMLDoc( url, parse_status );
    setTimeout( 'update_playlist()', 1000 );
}
function pl_play( id )
{
    loadXMLDoc( 'requests/status.xml?command=pl_play&id='+id, parse_status );
    setTimeout( 'update_playlist()', 1000 );
}
function pl_pause()
{
    loadXMLDoc( 'requests/status.xml?command=pl_pause', parse_status );
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
function volume_down()
{
    loadXMLDoc( 'requests/status.xml?command=volume&val=-20', parse_status );
}
function volume_up()
{
    loadXMLDoc( 'requests/status.xml?command=volume&val=%2B20', parse_status );
}
function fullscreen()
{
    loadXMLDoc( 'requests/status.xml?command=fullscreen', parse_status );
}
function update_status()
{
    loadXMLDoc( 'requests/status.xml', parse_status );
}
function update_playlist()
{
    loadXMLDoc( 'requests/playlist.xml', parse_playlist );
}

function parse_status()
{
    if( req.readyState == 4 )
    {
        if( req.status == 200 )
        {
            status = req.responseXML.documentElement;
            document.getElementById( 'time' ).textContent = format_time( status.getElementsByTagName( 'time' )[0].firstChild.data );
            document.getElementById( 'length' ).textContent = format_time( status.getElementsByTagName( 'length' )[0].firstChild.data );
            if( status.getElementsByTagName( 'volume' ).length != 0 )
            document.getElementById( 'volume' ).textContent = Math.floor(status.getElementsByTagName( 'volume' )[0].firstChild.data/5.12)+"%";
            document.getElementById( 'state' ).textContent = status.getElementsByTagName( 'state' )[0].firstChild.data;
            if( status.getElementsByTagName( 'state' )[0].firstChild.data == "playing" )
            {
                document.getElementById( 'btn_pause_img' ).setAttribute( 'src', 'images/pause.png' );
            }
            else
            {
                document.getElementById( 'btn_pause_img' ).setAttribute( 'src', 'images/play.png' );
            }
        }
        else
        {
            /*alert( 'Error! HTTP server replied: ' + req.status );*/
        }
    }
}

function parse_playlist()
{
    if( req.readyState == 4 )
    {
        if( req.status == 200 )
        {
            answer = req.responseXML.documentElement;
            playtree = document.getElementById( 'playtree' );
            /* pos = playtree; */
            pos = document.createElement( "div" );
            pos_top = pos;
            elt = answer.firstChild;
            while( elt )
            {
                if( elt.nodeName == "node" )
                {
                    pos.appendChild( document.createElement( "div" ) );
                    pos.lastChild.setAttribute( 'class', 'pl_node' );
                    /*pos.lastChild.setAttribute( 'onclick', 'pl_play('+elt.getAttribute( 'id' )+');' );*/
                    pos.lastChild.setAttribute( 'id', 'pl_'+elt.getAttribute( 'id' ) );
                    pos.lastChild.appendChild( document.createTextNode( elt.getAttribute( 'name' ) ) );
                }
                else if( elt.nodeName == "leaf" )
                {
                    pos.appendChild( document.createElement( "a" ) );
                    pl = pos.lastChild;
                    pl.setAttribute( 'class', 'pl_leaf' );
                    pl.setAttribute( 'href', 'javascript:pl_play('+elt.getAttribute( 'id' )+');' );
                    pl.setAttribute( 'id', 'pl_'+elt.getAttribute( 'id' ) );
                    if( elt.getAttribute( 'current' ) == 'current' )
                    {
                        pl.setAttribute( 'style', 'font-weight: bold;' );
                        document.getElementById( 'nowplaying' ).textContent
                            = elt.getAttribute( 'name' );
                    }
                    pl.setAttribute( 'title', elt.getAttribute( 'uri' ));
                    pl.appendChild( document.createTextNode( elt.getAttribute( 'name' ) ) );
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
                    elt = elt.parentNode.nextSibling;
                    pos = pos.parentNode;
                }
            }
            while( playtree.hasChildNodes() )
                playtree.removeChild( playtree.firstChild );
            playtree.appendChild( pos_top );
        }
        else
        {
            /*alert( 'Error! HTTP server replied: ' + req.status );*/
        }
    }
}

function set_css( item, element, value )
{
    for( j = 0; j<document.styleSheets.length; j++ )
    {
        cssRules = document.styleSheets[j].cssRules;
        for( i = 0; i<cssRules.length; i++)
        {
            if( cssRules[i].selectorText == item )
            {
                cssRules[i].style.setProperty( element, value, null );
                return;
            }
        }
    }
}

function get_css( item, element )
{
    for( j = 0; j<document.styleSheets.length; j++ )
    {
        cssRules = document.styleSheets[j].cssRules;
        for( i = 0; i<cssRules.length; i++)
        {
            if( cssRules[i].selectorText == item )
            {
                return cssRules[i].style.getPropertyValue( element );
            }
        }
    }
}

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

function toggle_show_sout_helper()
{
    element = document.getElementById( "sout_helper" );
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

function toggle_show( id )
{
    element = document.getElementById( id );
    if( element.style.display == 'block' || element.style.display == '' )
    {
        element.style.display = 'none';
    }
    else
    {
        element.style.display = 'block';
    }
}

function show( id )
{
    document.getElementById( id ).style.display = 'block';
}
function hide( id )
{
    document.getElementById( id ).style.display = 'none';
}

function hide_input( )
{
    document.getElementById( 'input_file' ).style.display = 'none';
    document.getElementById( 'input_disc' ).style.display = 'none';
    document.getElementById( 'input_network' ).style.display = 'none';
}

function checked( id )
{
    return document.getElementById( id ).checked;
}
function value( id )
{
    return document.getElementById( id ).value;
}
function radio_value( name )
{
    radio = document.getElementsByName( name );
    for( i = 0; i<radio.length; i++ )
    {
        if( radio[i].checked )
        {
            return radio[i].value;
        }
    }
    return "";
}
function disable( id )
{/* FIXME */
    return document.getElementById( id ).value;
}

function check_and_replace_int( id, val )
{
    var objRegExp = /^\d\d*$/;
    if( value( id ) != ''
        && ( !objRegExp.test( value( id ) )
             || parseInt( value( id ) ) < 1 ) )
        document.getElementById( id ).value = val;

}

function disable( id )
{
    document.getElementById( id ).disabled = true;
}

function enable( id )
{
    document.getElementById( id ).disabled = false;
}

function button_over( element )
{
    element.style.border = "1px solid black";
}

function button_out( element )
{
    element.style.border = "1px none black";
}

/* update the input MRL using data from the input file helper */
/* FIXME ... subs support */
function update_input_file()
{
    mrl = document.getElementById( 'input_mrl' );

    mrl.value = value( 'input_file_filename' );
}

/* update the input MRL using data from the input disc helper */
function update_input_disc()
{
    mrl = document.getElementById( 'input_mrl' );
    type = radio_value( "input_disc_type" );
    device = value( "input_disc_dev" );

    check_and_replace_int( 'input_disc_title', 0 );
    check_and_replace_int( 'input_disc_chapter', 0 );
    check_and_replace_int( 'input_disc_subtrack', '' );
    check_and_replace_int( 'input_disc_audiotrack', 0 );

    title = value( 'input_disc_title' );
    chapter = value( 'input_disc_chapter' );
    subs = value( 'input_disc_subtrack' );
    audio = value( 'input_disc_audiotrack' );

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

    if( type != "cdda" )
    {
        if( subs != '' )
            mrl.value += " :sub-track="+subs;
        if( audio != '' )
            mrl.value += " :audio-track="+audio;
    }

}

/* update the input MRL using data from the input network helper */
function update_input_net()
{
    mrl = document.getElementById( 'input_mrl' );
    type = radio_value( "input_net_type" );
    
    check_and_replace_int( 'input_net_udp_port', 1234 );
    check_and_replace_int( 'input_net_udpmcast_port', 1234 );

    mrl.value = "";

    if( type == "udp" )
    {
        mrl.value += "udp://";
        if( checked( 'input_net_udp_forceipv6' ) )
            mrl.value += "[::]";
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
        url = value( 'input_net_http_url' );
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
        url = value( 'input_net_rtsp_url' );
        if( url.substring(0,7) != "rtsp://" )
            mrl.value += "rtsp://";
        mrl.value += url;
    }

    if( checked( "input_net_timeshift" ) )
        mrl.value += " :access-filter=timeshift";
}

/* update the sout MRL using data from the sout_helper */
function update_sout()
{
    mrl = document.getElementById( 'sout_mrl' );
    mrl.value = "";

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

    transcode =  checked( 'sout_vcodec_s' ) || checked( 'sout_acodec_s' )
                 || checked( 'sout_sub' ) || checked( 'sout_soverlay' );

    if( transcode )
    {
        mrl.value += ":sout=#transcode{";
        alot = false; /* alot == at least one transcode */
        if( checked( 'sout_vcodec_s' ) )
        {
            mrl.value += "vcodec="+value( 'sout_vcodec' )+",vb="+value( 'sout_vb' )+",scale="+value( 'sout_scale' );
            alot = true;
        }
        if( checked( 'sout_acodec_s' ) )
        {
            if( alot ) mrl.value += ",";
            mrl.value += "acodec="+value( 'sout_acodec' )+",ab="+value( 'sout_ab' );
            if( value( 'sout_channels' ) )
                mrl.value += ",channels="+value( 'sout_channels' );
            alot = true;
        }
        if( checked( 'sout_soverlay' ) )
        {
            if( alot ) mrl.value += ",";
            mrl.value += "soverlay";
            alot = true;
        }
        else if( checked( 'sout_sub' ) )
        {
            if( alot ) mrl.value += ",";
            mrl.value += "scodec="+value( 'sout_scodec' );
            alot = true;
        }
        mrl.value += "}";
    }

    output = checked( 'sout_display' ) + checked( 'sout_file' )
             + checked( 'sout_http' ) + checked( 'sout_mmsh' )
             + checked( 'sout_rtp' )  + checked( 'sout_udp' );

    if( output )
    {
        if( transcode )
            mrl.value += ":";
        else
            mrl.value += ":sout=#";
        aloo = false; /* aloo == at least one output */
        mux = radio_value( 'sout_mux' );
        ttl = parseInt( value( 'sout_ttl' ) );
        if( output > 1 ) mrl.value += "duplicate{";
        if( checked( 'sout_display' ) )
        {
            if( output > 1 ) mrl.value += "dst="
            mrl.value += "display";
            aloo = true;
        }
        if( checked( 'sout_file' ) )
        {
            if( aloo ) mrl.value += ",";
            if( output > 1 ) mrl.value += "dst="
            mrl.value += "std{access=file,mux="+mux+",dst="+value( 'sout_file_filename' )+"}";
            aloo = true;
        }
        if( checked( 'sout_http' ) )
        {
            if( aloo ) mrl.value += ",";
            if( output > 1 ) mrl.value += "dst="
            mrl.value += "std{access=http,mux="+mux+",dst="+value( 'sout_http_addr' );
            if( value( 'sout_http_port' ) )
                mrl.value += ":"+value( 'sout_http_port' );
            mrl.value += "}";
            aloo = true;
        }
        if( checked( 'sout_mmsh' ) )
        {
            if( aloo ) mrl.value += ",";
            if( output > 1 ) mrl.value += "dst="
            mrl.value += "std{access=mmsh,mux="+mux+",dst="+value( 'sout_mmsh_addr' );
            if( value( 'sout_mmsh_port' ) )
                mrl.value += ":"+value( 'sout_mmsh_port' );
            mrl.value += "}";
            aloo = true;
        }
        if( checked( 'sout_rtp' ) )
        {
            if( aloo ) mrl.value += ",";
            if( output > 1 ) mrl.value += "dst="
            mrl.value += "std{access=rtp";
            if( ttl ) mrl.value += "{ttl="+ttl+"}";
            mrl.value += ",mux="+mux+",dst="+value( 'sout_rtp_addr' );
            if( value( 'sout_rtp_port' ) )
                mrl.value += ":"+value( 'sout_rtp_port' );
            if( checked( 'sout_sap' ) )
            {
                mrl.value += ",sap";
                if( value( 'sout_sap_group' ) != '' )
                {
                    mrl.value += ",group=\""+value( 'sout_sap_group' )+"\"";
                }
                mrl.value += ",name=\""+value( 'sout_sap_name' )+"\"";
            }
            mrl.value += "}";
            aloo = true;
        }
        if( checked( 'sout_udp' ) )
        {
            if( aloo ) mrl.value += ",";
            if( output > 1 ) mrl.value += "dst="
            mrl.value += "std{access=udp";
            if( ttl ) mrl.value += "{ttl="+ttl+"}";
            mrl.value += ",mux="+mux+",dst="+value( 'sout_udp_addr' );
            if( value('sout_udp_port' ) )
                mrl.value += ":"+value( 'sout_udp_port' );
            if( checked( 'sout_sap' ) )
            {
                mrl.value += ",sap";
                if( value( 'sout_sap_group' ) != '' )
                {
                    mrl.value += ",group=\""+value( 'sout_sap_group' )+"\"";
                }
                mrl.value += ",name=\""+value( 'sout_sap_name' )+"\"";
            }
            mrl.value += "}";
            aloo = true;
        }
        if( output > 1 ) mrl.value += "}";
    }

    if( ( transcode || output ) && checked( 'sout_all' ) )
        mrl.value += " :sout-all";
}

/* reset sout mrl value */
function reset_sout()
{
    document.getElementById('sout_mrl').value = value('sout_old_mrl');
}

function save_sout()
{
    document.getElementById('sout_old_mrl').value = value('sout_mrl');
}

function loop_refresh_status()
{
    setTimeout( 'loop_refresh_status()', 1000 );
    update_status();
}
function loop_refresh_playlist()
{
 /*   setTimeout( 'loop_refresh_playlist()', 10000 ); */
    update_playlist();
}
function loop_refresh()
{
    setTimeout('loop_refresh_status()',0);
    setTimeout('loop_refresh_playlist()',0);
}

function browse( dest )
{
    document.getElementById( 'browse_dest' ).value = dest;
    browse_dir( document.getElementById( 'browse_lastdir' ).value );
    show( 'browse' );
}
function browse_dir( dir )
{
    document.getElementById( 'browse_lastdir' ).value = dir;
    loadXMLDoc( 'requests/browse.xml?dir='+dir, parse_browse_dir );
}
function browse_path( p )
{
    document.getElementById( document.getElementById( 'browse_dest' ).value ).value = p;
    hide( 'browse' );
    document.getElementById( document.getElementById( 'browse_dest' ).value ).focus();
}
function parse_browse_dir( )
{
    if( req.readyState == 4 )
    {
        if( req.status == 200 )
        {
            answer = req.responseXML.documentElement;
            browser = document.getElementById( 'browser' );
            pos = document.createElement( "div" );
            elt = answer.firstChild;
            while( elt )
            {
                if( elt.nodeName == "element" )
                {
                    pos.appendChild( document.createElement( "a" ) );
                    pos.lastChild.setAttribute( 'class', 'browser' );
                    if( elt.getAttribute( 'type' ) == 'directory' )
                    {
                        pos.lastChild.setAttribute( 'href', 'javascript:browse_dir(\''+elt.getAttribute( 'path' )+'\');');
                    }
                    else
                    {
                        pos.lastChild.setAttribute( 'href', 'javascript:browse_path(\''+elt.getAttribute( 'path' )+'\');' );
                    }
                    pos.lastChild.appendChild( document.createTextNode( elt.getAttribute( 'name' ) ) );
                    if( elt.getAttribute( 'type' ) == 'directory' )
                    {
                        pos.appendChild( document.createTextNode( ' ' ) );
                        pos.appendChild( document.createElement( "a" ) );
                        pos.lastChild.setAttribute( 'class', 'browser' );
                        pos.lastChild.setAttribute( 'href', 'javascript:browse_path(\''+elt.getAttribute( 'path' )+'\');');
                        pos.lastChild.appendChild( document.createTextNode( '(select)' ) );
                    }
                    pos.appendChild( document.createElement( "br" ) );
                }
                elt = elt.nextSibling;
            }
            while( browser.hasChildNodes() )
                browser.removeChild( browser.firstChild );
            browser.appendChild( pos );
        }
        else
        {
            /*alert( 'Error! HTTP server replied: ' + req.status );*/
        }
    }
}
