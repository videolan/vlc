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

function mosaic_size_change()
{
    check_and_replace_int( "mosaic_rows", "1" );
    mr = value( "mosaic_rows" );
    check_and_replace_int( "mosaic_cols", "1" );
    mc = value( "mosaic_cols" );
    
    mlayout = document.getElementById( "mosaic_layout" );
    while( mlayout.hasChildNodes() )
        mlayout.removeChild( mlayout.firstChild );

    if( mc && mr )
    {
        for( y = 0; y < mr; y++ )
        {
            mrow = document.createElement( 'div' );
            mrow.setAttribute( 'class', 'mosaic_row' );
            for( x = 0; x < mc; x++ )
            {
                melt = document.createElement( 'input' );
                melt.setAttribute( 'type', 'button' );
                melt.setAttribute( 'id', 'mosaic_'+x+'_'+y );
                melt.setAttribute( 'class', 'mosaic_element' );
                melt.setAttribute( 'onclick', 'mosaic_elt_choose(\'mosaic_'+x+'_'+y+'\');' );
                melt.setAttribute( 'value', '(click)' );
                mrow.appendChild( melt );
            }
            mlayout.appendChild( mrow );
        }
    }
}

function mosaic_add_input()
{
    mlist = document.getElementById( "mosaic_list" );
    minput = document.createElement( 'a' );
    minput.setAttribute( 'href', 'javascript:mosaic_elt_select(\'mosaic_'+value('mosaic_input_name')+'\');');
    minput.setAttribute( 'id', 'mosaic_'+value('mosaic_input_name') );
    minput.setAttribute( 'value', value('mosaic_input') );
    minput.setAttribute( 'title', value('mosaic_input') );
    minputtxt = document.createTextNode( value('mosaic_input_name') );
    minput.appendChild( minputtxt );
    mlist.appendChild( minput );
    mlist.appendChild( document.createElement( 'br' ) );
}

function mosaic_elt_select( id )
{
    hide( 'mosaic_list' );
    document.getElementById( document.getElementById( 'mosaic_list' ).value ).value =
        document.getElementById( id ).getAttribute( 'value' );
}

function mosaic_elt_choose( id )
{
    document.getElementById( 'mosaic_list' ).value = id;
    show( 'mosaic_list' );
}
