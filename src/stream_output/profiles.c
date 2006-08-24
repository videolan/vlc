/*****************************************************************************
 * profiles.c: Streaming profiles
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include <vlc_streaming.h>
#include <assert.h>

#define MAX_CHAIN 32768
#define CHAIN_APPEND( format, args... ) { \
    memcpy( psz_temp, psz_output, MAX_CHAIN ); \
    snprintf( psz_output, MAX_CHAIN - 1, "%s"  format , psz_temp, ## args ); }

#define DUPM p_module->typed.p_duplicate
#define STDM p_module->typed.p_std
#define DISM p_module->typed.p_display
#define TRAM p_module->typed.p_transcode

/**********************************************************************
 * General chain manipulation
 **********************************************************************/
sout_duplicate_t *streaming_ChainAddDup( sout_chain_t *p_chain )
{
    DECMALLOC_NULL( p_module, sout_module_t );
    MALLOC_NULL( DUPM, sout_duplicate_t );
    p_module->i_type = SOUT_MOD_DUPLICATE;
    DUPM->i_children = 0;
    DUPM->pp_children = NULL;
    INSERT_ELEM( p_chain->pp_modules, p_chain->i_modules, p_chain->i_modules,
                 p_module );
    return p_module->typed.p_duplicate;
}

sout_std_t *streaming_ChainAddStd( sout_chain_t *p_chain, char *psz_access,
                                   char *psz_mux, char *psz_url )
{
    DECMALLOC_NULL( p_module, sout_module_t );
    MALLOC_NULL( STDM, sout_std_t );
    p_module->i_type = SOUT_MOD_STD;
    STDM->psz_mux = strdup( psz_mux );
    STDM->psz_access = strdup( psz_access );
    STDM->psz_url = strdup( psz_url );
    INSERT_ELEM( p_chain->pp_modules, p_chain->i_modules, p_chain->i_modules,
                 p_module );
    return STDM;
}

sout_display_t *streaming_ChainAddDisplay( sout_chain_t *p_chain )
{
    DECMALLOC_NULL( p_module, sout_module_t );
    MALLOC_NULL( DISM, sout_display_t );
    p_module->i_type = SOUT_MOD_DISPLAY;
    return DISM;
}

sout_transcode_t *streaming_ChainAddTranscode( sout_chain_t *p_chain,
                        char *psz_vcodec, char * psz_acodec, char * psz_scodec,
                        int i_vb, float f_scale, int i_ab, int i_channels, 
                        vlc_bool_t b_soverlay, char *psz_additional )
{
    DECMALLOC_NULL( p_module, sout_module_t );
    MALLOC_NULL( TRAM, sout_transcode_t );
    p_module->i_type = SOUT_MOD_TRANSCODE;

    assert( !( b_soverlay && psz_scodec ) );
    if( psz_vcodec ) TRAM->psz_vcodec = strdup( psz_vcodec );
    if( psz_acodec ) TRAM->psz_acodec = strdup( psz_acodec );
    if( psz_scodec ) TRAM->psz_scodec = strdup( psz_scodec );
    TRAM->i_vb = i_vb; TRAM->i_ab = i_ab; TRAM->f_scale = f_scale;
    TRAM->i_channels = i_channels; TRAM->b_soverlay = b_soverlay;
    if( TRAM->psz_additional ) TRAM->psz_additional = strdup( psz_additional );
    return TRAM;
}
            
void streaming_DupAddChild( sout_duplicate_t *p_dup )
{
    if( p_dup )
    {
        sout_chain_t * p_child = streaming_ChainNew();
        INSERT_ELEM( p_dup->pp_children, p_dup->i_children,
                     p_dup->i_children, p_child );
    }
}

#define DUP_OR_CHAIN p_dup ? p_dup->pp_children[p_dup->i_children-1] : p_chain

#define ADD_OPT( format, args... ) { \
    char *psz_opt; asprintf( &psz_opt, format, ##args ); \
    INSERT_ELEM( p_chain->ppsz_options, p_chain->i_options, p_chain->i_options,\
                 psz_opt );\
    free( psz_opt ); }

void streaming_ChainClean( sout_chain_t *p_chain )
{
    int i,j;
    sout_module_t *p_module;
    if( p_chain->i_modules )
    {
        for( i = p_chain->i_modules -1; i >= 0 ; i-- )
        {
            p_module = p_chain->pp_modules[i];
            switch( p_module->i_type )
            {
            case SOUT_MOD_DUPLICATE:
                if( DUPM->i_children == 0 ) break;
                for( j = DUPM->i_children - 1 ; j >= 0; j-- )
                {
                    streaming_ChainClean( DUPM->pp_children[j] );
                }
                break;
            case SOUT_MOD_STD:
                FREENULL( STDM->psz_url );
                FREENULL( STDM->psz_name );
                FREENULL( STDM->psz_group );
                break;
            case SOUT_MOD_TRANSCODE:
                FREENULL( TRAM->psz_vcodec );
                FREENULL( TRAM->psz_acodec );
                FREENULL( TRAM->psz_scodec );
                FREENULL( TRAM->psz_venc );
                FREENULL( TRAM->psz_aenc );
                FREENULL( TRAM->psz_additional );
                break;
            }
            REMOVE_ELEM( p_chain->pp_modules, p_chain->i_modules, i );
            free( p_module );
        }
    }
}

/**********************************************************************
 * Interaction with streaming GUI descriptors
 **********************************************************************/
#define DO_ENABLE_ACCESS \
    if( !strcmp( STDM->psz_access, "file" ) )\
    { \
        pd->b_file = VLC_TRUE; pd->psz_file = strdup( STDM->psz_url ); \
    } \
    else if(  !strcmp( STDM->psz_access, "http" ) )\
    { \
        pd->b_http = VLC_TRUE; pd->psz_http = strdup( STDM->psz_url ); \
    } \
    else if(  !strcmp( STDM->psz_access, "mms" ) )\
    { \
        pd->b_mms = VLC_TRUE; pd->psz_mms = strdup( STDM->psz_url ); \
    } \
    else if(  !strcmp( STDM->psz_access, "udp" ) )\
    { \
        pd->b_udp = VLC_TRUE; pd->psz_udp = strdup( STDM->psz_url ); \
    } \
    else \
    { \
        msg_Err( p_this, "unahandled access %s", STDM->psz_access ); \
    }


/**
 * Try to convert a chain to a gui descriptor. This is only possible for 
 * "simple" chains. 
 * \param p_this vlc object
 * \param p_chain the source streaming chain
 * \param pd the destination gui descriptor object
 * \return TRUE if the conversion succeeded, false else
 */
vlc_bool_t streaming_ChainToGuiDesc( vlc_object_t *p_this,
                                  sout_chain_t *p_chain, sout_gui_descr_t *pd )
{
    int j, i_last = 0;
    sout_module_t *p_module;
    if( p_chain->i_modules == 0 || p_chain->i_modules > 2 ) return VLC_FALSE;

    if( p_chain->pp_modules[0]->i_type == SOUT_MOD_TRANSCODE )
    {
        if( p_chain->i_modules == 1 ) return VLC_FALSE;
        p_module = p_chain->pp_modules[0];
        i_last++;

        pd->b_soverlay = TRAM->b_soverlay;
        pd->i_vb = TRAM->i_vb; pd->i_ab = TRAM->i_ab;
        pd->i_channels = TRAM->i_channels; pd->f_scale = TRAM->f_scale;
        if( TRAM->psz_vcodec ) pd->psz_vcodec = strdup( TRAM->psz_vcodec );
        if( TRAM->psz_acodec ) pd->psz_acodec = strdup( TRAM->psz_acodec );
        if( TRAM->psz_scodec ) pd->psz_scodec = strdup( TRAM->psz_scodec );
    }
    if( p_chain->pp_modules[i_last]->i_type == SOUT_MOD_DUPLICATE )
    {
        p_module = p_chain->pp_modules[i_last];
        
        // Nothing allowed after duplicate. Duplicate mustn't be empty
        if( p_chain->i_modules > i_last +1 || !DUPM->i_children ) 
            return VLC_FALSE;
        for( j = 0 ; j<  DUPM->i_children ; j++ )
        {
            sout_chain_t *p_child = DUPM->pp_children[j];
            if( p_child->i_modules != 1 ) return VLC_FALSE;
            p_module = p_child->pp_modules[0];
            if( p_module->i_type == SOUT_MOD_STD )
            {
                DO_ENABLE_ACCESS
            }
            else if( p_module->i_type == SOUT_MOD_DISPLAY )
                pd->b_local = VLC_TRUE;
            else if( p_module->i_type == SOUT_MOD_RTP )
            {
                msg_Err( p_this, "RTP unhandled" );
                return VLC_FALSE;
            }
        }
        i_last++;
    }
    if( p_chain->pp_modules[i_last]->i_type == SOUT_MOD_STD )
    {
        p_module = p_chain->pp_modules[i_last];
        DO_ENABLE_ACCESS;
    }
    else if( p_chain->pp_modules[i_last]->i_type == SOUT_MOD_DISPLAY )
    {
        pd->b_local = VLC_TRUE;
    }
    else if( p_chain->pp_modules[i_last]->i_type == SOUT_MOD_RTP )
    {
        msg_Err( p_this, "RTP unhandled" );
        return VLC_FALSE;

    }
    return VLC_TRUE;

}

#define HANDLE_GUI_URL( type, access ) if( pd->b_##type ) { \
        streaming_DupAddChild( p_dup ); \
        if( pd->i_##type > 0 ) \
        { \
            char *psz_url; \
            asprintf( &psz_url, "%s:%i", pd->psz_##type, pd->i_##type ); \
            streaming_ChainAddStd( DUP_OR_CHAIN, access, pd->psz_mux,\
                                   psz_url ); \
            free( psz_url ); \
        } \
        else \
        { \
            streaming_ChainAddStd( DUP_OR_CHAIN, access, pd->psz_mux,\
                                   pd->psz_##type );\
        }\
    }

void streaming_GuiDescToChain( vlc_object_t *p_obj, sout_chain_t *p_chain,
                               sout_gui_descr_t *pd )
{
    sout_duplicate_t *p_dup = NULL;
    /* Clean up the chain */
    streaming_ChainClean( p_chain );

    /* Transcode */
    if( pd->psz_vcodec || pd->psz_acodec || pd->psz_scodec || pd->b_soverlay )
    {
        streaming_ChainAddTranscode( p_chain, pd->psz_vcodec, pd->psz_acodec,
                                     pd->psz_scodec, pd->i_vb, pd->f_scale,
                                     pd->i_ab, pd->i_channels, 
                                     pd->b_soverlay, NULL );
    }
    /* #std{} */
    if( pd->b_local + pd->b_file + pd->b_http + pd->b_mms + pd->b_rtp + 
        pd->b_udp > 1 )
    {
        p_dup = streaming_ChainAddDup( p_chain );
    }
    if( pd->b_local )
    {
        streaming_DupAddChild( p_dup );
        streaming_ChainAddDisplay(  DUP_OR_CHAIN );
    }
    if( pd->b_file )
    {
        streaming_DupAddChild( p_dup );
        streaming_ChainAddStd( DUP_OR_CHAIN, "file", pd->psz_mux,
                               pd->psz_file );
    }
    if( pd->b_udp )
    {
        sout_std_t *p_std;
        streaming_DupAddChild( p_dup );
        if( pd->i_udp > 0 )
        {
            char *psz_url;
            asprintf( &psz_url, "%s:%i", pd->psz_udp, pd->i_udp );
            p_std = streaming_ChainAddStd( DUP_OR_CHAIN, "udp",
                                           pd->psz_mux, psz_url );
            free( psz_url );
        }
        else
        {
            p_std = streaming_ChainAddStd( DUP_OR_CHAIN, "udp",
                                           pd->psz_mux, pd->psz_udp );
        }
        if( pd->i_ttl ) ADD_OPT( "ttl=%i", pd->i_ttl );
        if( pd->b_sap )
        {
            pd->b_sap = VLC_TRUE;
            p_std->psz_name = strdup( pd->psz_name );       
            p_std->psz_group = pd->psz_group ? strdup( pd->psz_group ) : NULL;
        }
    }
    HANDLE_GUI_URL( http, "http" )
    HANDLE_GUI_URL( mms, "mms" )
}
#undef HANDLE_GUI_URL

/**********************************************************************
 * Create a sout string from a chain
 **********************************************************************/
char * streaming_ChainToPsz( sout_chain_t *p_chain )
{
    int i;
    char psz_output[MAX_CHAIN];
    char psz_temp[MAX_CHAIN];
    sprintf( psz_output, "#" );
    for( i = 0 ; i< p_chain->i_modules; i++ )
    {
        sout_module_t *p_module = p_chain->pp_modules[i];
        switch( p_module->i_type )
        {
        case SOUT_MOD_TRANSCODE:
            CHAIN_APPEND( "transcode{" );
            if( TRAM->psz_vcodec )
            {
                CHAIN_APPEND( "vcodec=%s,vb=%i,scale=%f", TRAM->psz_vcodec,
                                     TRAM->i_vb, TRAM->f_scale );
                if( TRAM->psz_acodec || TRAM->psz_scodec || TRAM->b_soverlay )
                    CHAIN_APPEND( "," );
            }
            if( TRAM->psz_acodec )
            {
                CHAIN_APPEND( "acodec=%s,ab=%i,channels=%i", TRAM->psz_acodec,
                              TRAM->i_ab, TRAM->i_channels );
                if( TRAM->psz_scodec || TRAM->b_soverlay )
                    CHAIN_APPEND( "," );
            }
            assert( !(TRAM->psz_scodec && TRAM->b_soverlay) );
            if( TRAM->psz_scodec )
                CHAIN_APPEND( "scodec=%s", TRAM->psz_scodec) ;
            if( TRAM->b_soverlay )
                CHAIN_APPEND( "soverlay" );
            CHAIN_APPEND( "}" );
            break;

        case SOUT_MOD_DISPLAY:
            CHAIN_APPEND( "display" )
            break;
        case SOUT_MOD_STD:
            CHAIN_APPEND( "std{access=%s,url=%s,mux=%s}", STDM->psz_access,
                          STDM->psz_url, STDM->psz_mux );
        }
    }
    return strdup( psz_output );
}

/**********************************************************************
 * Handle streaming profiles
 **********************************************************************/

/**
 * List the available profiles. Fills the pp_profiles list with preinitialized
 * values. Only metadata is decoded
 * \param p_this vlc object
 * \param pi_profiles number of listed profiles
 * \param pp_profiles array of profiles
 */
void streaming_ProfilesList( vlc_object_t *p_this, int *pi_profiles, 
                             streaming_profile_t **pp_profiles )
{
}


/** Parse a profile */
int streaming_ProfileParse( vlc_object_t *p_this,streaming_profile_t *p_profile,
                            const char *psz_profile )
{
    DECMALLOC_ERR( p_parser, profile_parser_t );
    module_t *p_module;
    assert( p_profile ); assert( psz_profile );

    p_parser->psz_profile = strdup( psz_profile );
    p_parser->p_profile = p_profile;

    p_this->p_private = (void *)p_parser;

    /* And call the module ! All work is done now */
    p_module = module_Need( p_this, "profile parser", "", VLC_TRUE );
    if( !p_module )
    {
        msg_Warn( p_this, "parsing profile failed" );
    }
}
