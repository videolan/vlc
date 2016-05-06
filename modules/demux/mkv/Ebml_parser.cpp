
/*****************************************************************************
 * EbmlParser for the matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "Ebml_parser.hpp"
#include "stream_io_callback.hpp"

/*****************************************************************************
 * Ebml Stream parser
 *****************************************************************************/
EbmlParser::EbmlParser( EbmlStream *es, EbmlElement *el_start, demux_t *p_demux,
                        bool b_with_dummy ) :
    p_demux( p_demux ),
    m_es( es ),
    mi_level( 1 ),
    m_got( NULL ),
    mi_user_level( 1 ),
    mb_keep( false ),
    mb_dummy( b_with_dummy )
{
    memset( m_el, 0, sizeof( *m_el ) * M_EL_MAXSIZE);
    m_el[0] = el_start;
}

EbmlParser::~EbmlParser( void )
{
    if( !mi_level )
    {
        assert( !mb_keep );
        delete m_el[1];
        return;
    }

    for( int i = 1; i <= mi_level; i++ )
    {
        if( !mb_keep )
        {
            delete m_el[i];
        }
        mb_keep = false;
    }
}

void EbmlParser::reconstruct( EbmlStream* es, EbmlElement* el_start, demux_t* p_demux )
{
    this->reconstruct( es, el_start, p_demux, var_InheritBool( p_demux, "mkv-use-dummy" ) );
}

void EbmlParser::reconstruct( EbmlStream* es, EbmlElement* el_start, demux_t* p_demux,
  bool b_with_dummy)
{
    this->~EbmlParser();

    new( static_cast<void*>( this ) ) EbmlParser(
      es, el_start, p_demux, b_with_dummy
    );
}

EbmlElement* EbmlParser::UnGet( uint64 i_block_pos, uint64 i_cluster_pos )
{
    if ( mi_user_level > mi_level )
    {
        while ( mi_user_level != mi_level )
        {
            delete m_el[mi_user_level];
            m_el[mi_user_level] = NULL;
            mi_user_level--;
        }
    }

    /* Avoid data skip in BlockGet */
    delete m_el[mi_level];
    m_el[mi_level] = NULL;

    m_got = NULL;
    mb_keep = false;
    if ( m_el[1] && m_el[1]->GetElementPosition() == i_cluster_pos )
    {
        m_es->I_O().setFilePointer( i_block_pos, seek_beginning );
        return m_el[1];
    }
    else
    {
        // seek to the previous Cluster
        m_es->I_O().setFilePointer( i_cluster_pos, seek_beginning );
        while(mi_level > 1)
        {
            mi_level--;
            mi_user_level--;
            delete m_el[mi_level];
            m_el[mi_level] = NULL;
        }
        return NULL;
    }
}

void EbmlParser::Up( void )
{
    if( mi_user_level == mi_level && m_el[mi_level] )
    {
        msg_Warn( p_demux, "MKV/Ebml Parser: Up cannot escape itself" );
    }

    mi_user_level--;
}

void EbmlParser::Down( void )
{
    mi_user_level++;
    mi_level++;
}

void EbmlParser::Keep( void )
{
    mb_keep = true;
}

void EbmlParser::Unkeep()
{
    mb_keep = false;
}

int EbmlParser::GetLevel( void ) const
{
    return mi_user_level;
}

void EbmlParser::Reset( demux_t *p_demux )
{
    while ( mi_level > 0)
    {
        delete m_el[mi_level];
        m_el[mi_level] = NULL;
        mi_level--;
    }
    this->p_demux = p_demux;
    mi_user_level = mi_level = 1;
    // a little faster and cleaner
    m_es->I_O().setFilePointer( static_cast<KaxSegment*>(m_el[0])->GetGlobalPosition(0) );
}


// Define a special Segment context that doesn't allow void/crc32 elements
static const EbmlSemanticContext & GetEbmlNoGlobal_Context();
static const EbmlSemanticContext Context_EbmlNoGlobal = EbmlSemanticContext(0, NULL, NULL, *GetEbmlNoGlobal_Context, NULL);
static const EbmlSemantic EbmlNoGlobal_ContextList[0] =
{
  //EbmlSemantic(false, false, EBML_INFO(EbmlCrc32)),   ///< EbmlCrc32
  //EbmlSemantic(false, false, EBML_INFO(EbmlVoid)),    ///< EbmlVoid
};
static const EbmlSemanticContext EbmlNoGlobal_Context = EbmlSemanticContext(countof(EbmlNoGlobal_ContextList), EbmlNoGlobal_ContextList, NULL, *GetEbmlNoGlobal_Context, NULL);
static const EbmlSemanticContext & GetEbmlNoGlobal_Context()
{
  return EbmlNoGlobal_Context;
}

// the Segment Context should not allow Void or CRC32 elements to avoid lookup false alarm
const EbmlSemanticContext Context_KaxSegmentVLC = EbmlSemanticContext(KaxSegment_Context.GetSize(),
                                                                      KaxSegment_Context.MyTable,
                                                                      KaxSegment_Context.Parent(),
                                                                      GetEbmlNoGlobal_Context,
                                                                      KaxSegment_Context.GetMaster());

EbmlElement *EbmlParser::Get( int n_call )
{
    int i_ulev = 0;
    EbmlElement *p_prev = NULL;

    if( mi_user_level != mi_level )
    {
        return NULL;
    }
    if( m_got )
    {
        EbmlElement *ret = m_got;
        m_got = NULL;

        return ret;
    }

    p_prev = m_el[mi_level];
    if( m_el[mi_level] )
    {
        m_el[mi_level]->SkipData( *m_es, EBML_CONTEXT(m_el[mi_level]) );

    }

    // ignore global Void/CRC32 elements when looking for level 1 elements in the Segment
    EbmlSemanticContext e_context =
            EBML_CTX_MASTER( EBML_CONTEXT(m_el[mi_level - 1]) ) == EBML_CTX_MASTER( Context_KaxSegmentVLC )
            ? Context_KaxSegmentVLC
            : EBML_CONTEXT(m_el[mi_level - 1]);
    /* Ignore unknown level 0 or 1 elements */
    m_el[mi_level] = m_es->FindNextElement( e_context,
                                            i_ulev, UINT64_MAX,
                                            (  mb_dummy | (mi_level > 1) ), 1 );
    if( i_ulev > 0 )
    {
        if( p_prev )
        {
            if( !mb_keep )
            {
                if( MKV_IS_ID( p_prev, KaxBlockVirtual ) )
                    static_cast<KaxBlockVirtualWorkaround*>(p_prev)->Fix(); // !! WARNING : TODO !! this is undefined-behavior
                delete p_prev;
            }
            mb_keep = false;
        }
        while( i_ulev > 0 )
        {
            if( mi_level == 1 )
            {
                mi_level = 0;
                return NULL;
            }

            delete m_el[mi_level - 1];
            m_got = m_el[mi_level -1] = m_el[mi_level];
            m_el[mi_level] = NULL;

            mi_level--;
            i_ulev--;
        }
        return NULL;
    }
    else if( m_el[mi_level] == NULL )
    {
        msg_Dbg( p_demux,"MKV/Ebml Parser: m_el[mi_level] == NULL" );
    }
    else if( m_el[mi_level]->IsDummy() && !mb_dummy )
    {
        bool b_bad_position = false;
        /* We got a dummy element but don't want those...
         * perform a sanity check */
        if( !mi_level )
        {
            msg_Err(p_demux, "Got invalid lvl 0 element... Aborting");
            return NULL;
        }

        if( p_prev && p_prev->IsFiniteSize() &&
            p_prev->GetEndPosition() != m_el[mi_level]->GetElementPosition() &&
            mi_level > 1 )
        {
            msg_Err( p_demux, "Dummy Element at unexpected position... corrupted file?" );
            b_bad_position = true;
        }

        if( n_call < M_EL_MAXSIZE && !b_bad_position && m_el[mi_level]->IsFiniteSize() &&
            ( !m_el[mi_level-1]->IsFiniteSize() ||
              m_el[mi_level]->GetEndPosition() <= m_el[mi_level-1]->GetEndPosition() ) )
        {
            /* The element fits inside its upper element */
            msg_Warn( p_demux, "Dummy element found %" PRIu64 "... skipping it",
                      m_el[mi_level]->GetElementPosition() );
            return Get( ++n_call );
        }
        else
        {
            /* Too large, misplaced or M_EL_MAXSIZE successive dummy elements */
            msg_Err( p_demux,
                     "Dummy element too large or misplaced at %" PRIu64 "... skipping to next upper element",
                     m_el[mi_level]->GetElementPosition() );

            if( mi_level >= 1 &&
                m_el[mi_level]->GetElementPosition() >= m_el[mi_level-1]->GetEndPosition() )
            {
                msg_Err(p_demux, "This element is outside its known parent... upping level");
                delete m_el[mi_level - 1];
                m_got = m_el[mi_level -1] = m_el[mi_level];
                m_el[mi_level] = NULL;

                mi_level--;
                return NULL;
            }

            delete m_el[mi_level];
            m_el[mi_level] = NULL;
            m_el[mi_level - 1]->SkipData( *m_es, EBML_CONTEXT(m_el[mi_level - 1]) );
            return Get();
        }
    }

    if( p_prev )
    {
        if( !mb_keep )
        {
            if( MKV_IS_ID( p_prev, KaxBlockVirtual ) )
                static_cast<KaxBlockVirtualWorkaround*>(p_prev)->Fix();
            delete p_prev;
        }
        mb_keep = false;
    }
    return m_el[mi_level];
}

bool EbmlParser::IsTopPresent( EbmlElement *el ) const
{
    for( int i = 0; i < mi_level; i++ )
    {
        if( m_el[i] && m_el[i] == el )
            return true;
    }
    return false;
}

