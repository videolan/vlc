/*****************************************************************************
 * themeloader.h: ThemeLoader class
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: themeloader.h,v 1.2 2003/03/18 04:56:58 ipkiss Exp $
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/


#ifndef VLC_SKIN_THEMELOADER
#define VLC_SKIN_THEMELOADER

//--- GENERAL ---------------------------------------------------------------
#include <string>
using namespace std;

//---------------------------------------------------------------------------
struct intf_thread_t;
int gzopen_frontend( char *pathname, int oFlags, int mode );
//---------------------------------------------------------------------------
class ThemeLoader
{
    private:
        intf_thread_t *p_intf;

#if defined( HAVE_LIBTAR_H ) && defined( HAVE_ZLIB_H )
        bool ExtractTarGz( const string tarfile, const string rootdir );
        bool Extract( const string FileName );
        void DeleteTempFiles( const string Path );
#endif
        void CleanTheme();
        bool Parse( const string XmlFile );
    public:
        ThemeLoader( intf_thread_t *_p_intf );
        ~ThemeLoader();

        bool Load( const string FileName );
};

#endif

