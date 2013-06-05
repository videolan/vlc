/*****************************************************************************
 * theme_repository.cpp
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "theme_repository.hpp"
#include "os_factory.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_dialogs.hpp"
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _WIN32 )
#   include <direct.h>
#endif

#include <fstream>


ThemeRepository *ThemeRepository::instance( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_repository == NULL )
    {
        pIntf->p_sys->p_repository = new ThemeRepository( pIntf );
    }

    return pIntf->p_sys->p_repository;
}


void ThemeRepository::destroy( intf_thread_t *pIntf )
{
    delete pIntf->p_sys->p_repository;
    pIntf->p_sys->p_repository = NULL;
}


ThemeRepository::ThemeRepository( intf_thread_t *pIntf ): SkinObject( pIntf )
{
    vlc_value_t val, text;

    // Create a variable to add items in wxwindows popup menu
    var_Create( pIntf, "intf-skins", VLC_VAR_STRING |
                VLC_VAR_HASCHOICE | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Select skin");
    var_Change( pIntf, "intf-skins", VLC_VAR_SETTEXT, &text, NULL );

    // Scan vlt files in the resource path
    OSFactory *pOsFactory = OSFactory::instance( pIntf );
    list<string> resPath = pOsFactory->getResourcePath();
    list<string>::const_iterator it;
    for( it = resPath.begin(); it != resPath.end(); ++it )
    {
        parseDirectory( *it );
    }

    // retrieve skins from skins directories and locate default skins
    map<string,string>::const_iterator itmap, itdefault;
    bool b_default_found = false;
    for( itmap = m_skinsMap.begin(); itmap != m_skinsMap.end(); ++itmap )
    {
        string name = itmap->first;
        string path = itmap->second;
        val.psz_string = (char*) path.c_str();
        text.psz_string = (char*) name.c_str();
        var_Change( getIntf(), "intf-skins", VLC_VAR_ADDCHOICE, &val,
                    &text );

        if( name == "Default" )
        {
            itdefault = itmap;
            b_default_found = true;
        }
    }

    // retrieve last skins stored or skins requested by user
    char* psz_current = var_InheritString( getIntf(), "skins2-last" );
    string current = string( psz_current ? psz_current : "" );
    free( psz_current );

    // check if skins exists and is readable
    bool b_readable = !ifstream( current.c_str() ).fail();

    msg_Dbg( getIntf(), "requested skins %s is %s accessible",
                         current.c_str(), b_readable ? "" : "NOT" );

    // set the default skins if given skins not accessible
    if( !b_readable && b_default_found )
        current = itdefault->second;

    // save this valid skins for reuse
    config_PutPsz( getIntf(), "skins2-last", current.c_str() );

    // Update repository
    updateRepository();

    // Set the callback
    var_AddCallback( pIntf, "intf-skins", changeSkin, this );

    // variable for opening a dialog box to change skins
    var_Create( pIntf, "intf-skins-interactive", VLC_VAR_VOID |
                VLC_VAR_ISCOMMAND );
    text.psz_string = _("Open skin ...");
    var_Change( pIntf, "intf-skins-interactive", VLC_VAR_SETTEXT, &text, NULL );

    // Set the callback
    var_AddCallback( pIntf, "intf-skins-interactive", changeSkin, this );

}


ThemeRepository::~ThemeRepository()
{
    m_skinsMap.clear();

    var_DelCallback( getIntf(), "intf-skins", changeSkin, this );
    var_DelCallback( getIntf(), "intf-skins-interactive", changeSkin, this );

    var_Destroy( getIntf(), "intf-skins" );
    var_Destroy( getIntf(), "intf-skins-interactive" );
}


void ThemeRepository::parseDirectory( const string &rDir_locale )
{
    DIR *pDir;
    char *pszDirContent;
    // Path separator
    const string &sep = OSFactory::instance( getIntf() )->getDirSeparator();

    // Open the dir
    // FIXME: parseDirectory should be invoked with UTF-8 input instead!!
    string rDir = sFromLocale( rDir_locale );
    pDir = vlc_opendir( rDir.c_str() );

    if( pDir == NULL )
    {
        // An error occurred
        msg_Dbg( getIntf(), "cannot open directory %s", rDir.c_str() );
        return;
    }

    // While we still have entries in the directory
    while( ( pszDirContent = vlc_readdir( pDir ) ) != NULL )
    {
        string name = pszDirContent;
        string extension;
        if( name.size() > 4 )
        {
            extension = name.substr( name.size() - 4, 4 );
        }
        if( extension == ".vlt" || extension == ".wsz" )
        {
            string path = rDir + sep + name;
            string shortname = name.substr( 0, name.size() - 4 );
            for( string::size_type i = 0; i < shortname.size(); i++ )
                shortname[i] = ( i == 0 ) ?
                               toupper( (unsigned char)shortname[i] ) :
                               tolower( (unsigned char)shortname[i] );
            m_skinsMap[shortname] = path;

            msg_Dbg( getIntf(), "found skin %s", path.c_str() );
        }

        free( pszDirContent );
    }

    closedir( pDir );
}



int ThemeRepository::changeSkin( vlc_object_t *pIntf, char const *pVariable,
                                 vlc_value_t oldval, vlc_value_t newval,
                                 void *pData )
{
    (void)pIntf; (void)oldval;
    ThemeRepository *pThis = (ThemeRepository*)(pData);

    if( !strcmp( pVariable, "intf-skins-interactive" ) )
    {
        CmdDlgChangeSkin cmd( pThis->getIntf() );
        cmd.execute();
    }
    else if( !strcmp( pVariable, "intf-skins" ) )
    {
        // Try to load the new skin
        CmdChangeSkin *pCmd = new CmdChangeSkin( pThis->getIntf(),
                                                 newval.psz_string );
        AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
        pQueue->push( CmdGenericPtr( pCmd ) );
    }

    return VLC_SUCCESS;
}


void ThemeRepository::updateRepository()
{
    vlc_value_t val, text;

    // retrieve the current skin
    char* psz_current = config_GetPsz( getIntf(), "skins2-last" );
    if( !psz_current )
        return;

    val.psz_string = psz_current;
    text.psz_string = psz_current;

    // add this new skins if not yet present in repository
    string current( psz_current );
    map<string,string>::const_iterator it;
    for( it = m_skinsMap.begin(); it != m_skinsMap.end(); ++it )
    {
        if( it->second == current )
            break;
    }
    if( it == m_skinsMap.end() )
    {
        var_Change( getIntf(), "intf-skins", VLC_VAR_ADDCHOICE, &val,
                    &text );
        string name = psz_current;
        m_skinsMap[name] = name;
    }

    // mark this current skins as 'checked' in list
    var_Change( getIntf(), "intf-skins", VLC_VAR_SETVALUE, &val, NULL );

    free( psz_current );
}

