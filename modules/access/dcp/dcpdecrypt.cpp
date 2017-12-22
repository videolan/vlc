/*****************************************************************************
 * Copyright (C) 2013 VLC authors and VideoLAN
 *
 * Author:
 *          Simona-Marinela Prodea <simona dot marinela dot prodea at gmail dot com>
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

/**
 * The code used for reading a DER-encoded private key, that is,
 * RSAKey::parseTag function and RSAKey::readDER function,
 * is taken almost as is from libgcrypt tests/fipsdrv.c
 */

/**
 * @file dcpdecrypt.cpp
 * @brief Handle encrypted DCPs
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* VLC core API headers */
#include <vlc_common.h>
#include <vlc_xml.h>
#include <vlc_strings.h>

#include <fstream>
#include <algorithm>
#include <cctype>

#include "dcpparser.h"

/* creates a printable, RFC 4122-conform UUID, from a given array of bytes
 */
static string createUUID( unsigned char *ps_string )
{
    string s_uuid;
    char h[3];
    int i, ret;

    if( ! ps_string )
        return "";

    try
    {
        s_uuid.append( "urn:uuid:" );
        for( i = 0; i < 16; i++ )
        {
            ret = snprintf( h, 3, "%02hhx", ps_string[i] );  /* each byte can be written as 2 hex digits */
            if( ret != 2 )
                return "";
            s_uuid.append( h );
            if( i == 3 || i == 5 || i == 7 || i == 9 )
                s_uuid.append( "-" );
        }
    }
    catch( ... )
    {
        return "";
    }

    return s_uuid;
}

/*
 * KDM class
 */

int KDM::Parse()
{
    string s_node, s_value;
    const string s_root_node = "DCinemaSecurityMessage";
    int type;

    AESKeyList *_p_key_list = NULL;

    /* init XML parser */
    if( this->OpenXml() )
    {
        msg_Err( p_demux, "failed to initialize KDM XML parser" );
        return VLC_EGENERIC;
    }

    msg_Dbg( this->p_demux, "parsing KDM..." );

    /* read first node and check if it is a KDM */
    if( ! ( ( XML_READER_STARTELEM == XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, s_node ) ) && ( s_node == s_root_node ) ) )
    {
        msg_Err( this->p_demux, "not a valid XML KDM" );
        goto error;
    }

    while( ( type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, s_node ) ) > 0 )
        if( type == XML_READER_STARTELEM && s_node == "AuthenticatedPrivate" )
        {
            _p_key_list = new (nothrow) AESKeyList;
            if( unlikely( _p_key_list == NULL ) )
                goto error;
            p_dcp->p_key_list = _p_key_list;
            if( this->ParsePrivate( s_node, type ) )
                goto error;

            /* keys found, so break */
            break;
        }

    if ( (_p_key_list == NULL) ||  (_p_key_list->size() == 0) )
    {
        msg_Err( p_demux, "Key list empty" );
        goto error;
    }

    /* close KDM XML */
    this->CloseXml();
    return VLC_SUCCESS;
error:
    this->CloseXml();
    return VLC_EGENERIC;
}

int KDM::ParsePrivate( const string _s_node, int _i_type )
{
    string s_node;
    int i_type;
    AESKey *p_key;

    /* check that we are where we're supposed to be */
    if( _i_type != XML_READER_STARTELEM )
        goto error;
    if( _s_node != "AuthenticatedPrivate" )
        goto error;

    /* loop on EncryptedKey nodes */
    while( ( i_type = XmlFile::ReadNextNode( this->p_demux, this->p_xmlReader, s_node ) ) > 0 )
    {
        switch( i_type )
        {
            case XML_READER_STARTELEM:
                if( s_node != "EncryptedKey" )
                    goto error;
                p_key = new (nothrow) AESKey( this->p_demux );
                if( unlikely( p_key == NULL ) )
                    return VLC_EGENERIC;
                if( p_key->Parse( p_xmlReader, s_node, i_type ) )
                {
                    delete p_key;
                    return VLC_EGENERIC;
                }
                p_dcp->p_key_list->push_back( p_key );
                break;

            case XML_READER_ENDELEM:
                if( s_node == _s_node )
                    return VLC_SUCCESS;
                break;
            default:
            case XML_READER_TEXT:
                goto error;
        }
    }

    /* shouldn't get here */
error:
    msg_Err( p_demux, "error while parsing AuthenticatedPrivate portion of KDM" );
    return VLC_EGENERIC;
}

/*
 * AESKey class
 */

int AESKey::Parse( xml_reader_t *p_xml_reader, string _s_node, int _i_type)
{
    string s_node;
    string s_value;
    int i_type;

    if( _i_type != XML_READER_STARTELEM)
        goto error;
    if( _s_node != "EncryptedKey" )
        goto error;

    while( ( i_type = XmlFile::ReadNextNode( this->p_demux, p_xml_reader, s_node ) ) > 0 )
    {
        switch( i_type )
        {
            case XML_READER_STARTELEM:
                if( s_node == "CipherValue" )
                {
                    if( XmlFile::ReadEndNode( this->p_demux, p_xml_reader, s_node, i_type, s_value ) )
                        goto error;
                    if( this->decryptRSA( s_value ) )
                        return VLC_EGENERIC;
                }
                break;
            case XML_READER_ENDELEM:
                if( s_node == _s_node )
                    return VLC_SUCCESS;
                break;
            default:
            case XML_READER_TEXT:
                goto error;
        }
    }

    /* shouldn't get here */
error:
    msg_Err( this->p_demux, "error while parsing EncryptedKey" );
    return VLC_EGENERIC;
}

/* decrypts the RSA encrypted text read from the XML file,
 * and saves the AES key and the other needed info
 * uses libgcrypt for decryption
 */
int AESKey::decryptRSA( string s_cipher_text_b64 )
{
    RSAKey rsa_key( this->p_demux );
    unsigned char *ps_cipher_text = NULL;
    unsigned char *ps_plain_text = NULL;
    gcry_mpi_t cipher_text_mpi = NULL;
    gcry_sexp_t cipher_text_sexp = NULL;
    gcry_sexp_t plain_text_sexp = NULL;
    gcry_mpi_t plain_text_mpi = NULL;
    gcry_sexp_t tmp_sexp = NULL;
    gcry_error_t err;
    size_t length;
    int i_ret = VLC_EGENERIC;

    /* get RSA private key file path */
    if( rsa_key.setPath() )
        goto end;

    /* read private key from file */
    if( rsa_key.readPEM() )
        goto end;

    /* remove spaces and newlines from encoded cipher text
     * (usually added for indentation in XML files)
     * */
    try
    {
        s_cipher_text_b64.erase( remove_if( s_cipher_text_b64.begin(), s_cipher_text_b64.end(), static_cast<int(*)(int)>(isspace) ),
                                 s_cipher_text_b64.end() );
    }
    catch( ... )
    {
        msg_Err( this->p_demux, "error while handling string" );
        goto end;
    }

    /* decode cipher from BASE64 to binary */
    if( ! ( length = vlc_b64_decode_binary( &ps_cipher_text, s_cipher_text_b64.c_str() ) ) )
    {
        msg_Err( this->p_demux, "could not decode cipher from Base64" );
        goto end;
    }

    /* initialize libgcrypt */
    vlc_gcrypt_init ();

    /* create S-expression for ciphertext */
    if( ( err = gcry_mpi_scan( &cipher_text_mpi, GCRYMPI_FMT_USG, ps_cipher_text, 256, NULL ) ) )
    {
        msg_Err( this->p_demux, "could not scan MPI from cipher text: %s", gcry_strerror( err ) );
        goto end;
    }
    if( ( err = gcry_sexp_build( &cipher_text_sexp, NULL, "(enc-val(flags oaep)(rsa(a %m)))", cipher_text_mpi ) ) )
    {
        msg_Err( this->p_demux, "could not build S-expression for cipher text: %s", gcry_strerror( err ) );
        goto end;
    }

    /* decrypt */
    if( ( err = gcry_pk_decrypt( &plain_text_sexp, cipher_text_sexp, rsa_key.priv_key ) ) )
    {
        msg_Err( this->p_demux, "error while decrypting RSA encrypted info: %s", gcry_strerror( err ) );
        goto end;
    }

    /* extract plain-text from S-expression */
    if( ! ( tmp_sexp = gcry_sexp_find_token( plain_text_sexp, "value", 0 ) ) )
        /* when using padding flags, the decrypted S-expression is of the form
         * "(value <plaintext>)", where <plaintext> is an MPI */
    {
        msg_Err( this->p_demux, "decrypted text is in an unexpected form; decryption may have failed" );
        goto end;
    }
    /* we could have used the gcry_sexp_nth_data to get the data directly,
     * but as that function is newly introduced (libgcrypt v1.6),
     * we prefer compatibility, even though that means passing the data through an MPI first */
    if( ! ( plain_text_mpi = gcry_sexp_nth_mpi( tmp_sexp, 1, GCRYMPI_FMT_USG ) ) )
    {
        msg_Err( this->p_demux, "could not extract MPI from decrypted S-expression" );
        goto end;
    }

    if( ( err = gcry_mpi_aprint( GCRYMPI_FMT_USG, &ps_plain_text, &length, plain_text_mpi ) ) )
    {
        msg_Err( this->p_demux, "error while extracting plain text from MPI: %s", gcry_strerror( err ) );
        goto end;
    }

    /* interpret the plaintext data */
    switch( length )
    {
        case 138:   /* SMPTE    DCP */
            if( this->extractInfo( ps_plain_text, true ) )
                goto end;
            break;
        case 134:   /* Interop  DCP */
            if( this->extractInfo( ps_plain_text, false ) )
                goto end;
            break;
        case static_cast<size_t>( -1 ):
            msg_Err( this->p_demux, "could not decrypt" );
            goto end;
        default:
            msg_Err( this->p_demux, "CipherValue field length does not match SMPTE nor Interop standards" );
            goto end;
    }

    i_ret = VLC_SUCCESS;

end:
    free( ps_cipher_text );
    gcry_mpi_release( cipher_text_mpi );
    gcry_sexp_release( cipher_text_sexp );
    gcry_sexp_release( plain_text_sexp );
    gcry_mpi_release( plain_text_mpi );
    gcry_sexp_release( tmp_sexp );
    gcry_free( ps_plain_text );
    return i_ret;
}

/* extracts and saves the AES key info from the plaintext;
 * parameter smpte is true for SMPTE DCP, false for Interop;
 * see SMPTE 430-1-2006, section 6.1.2 for the exact structure of the plaintext
 */
int AESKey::extractInfo( unsigned char * ps_plain_text, bool smpte )
{

    string s_rsa_structID( "f1dc124460169a0e85bc300642f866ab" ); /* unique Structure ID for all RSA-encrypted AES keys in a KDM */
    string s_carrier;
    char psz_hex[3];
    int i_ret, i_pos = 0;

    /* check for the structure ID */
    while( i_pos < 16 )
    {
        i_ret = snprintf( psz_hex, 3, "%02hhx", ps_plain_text[i_pos] );
        if( i_ret != 2 )
        {
            msg_Err( this->p_demux, "error while extracting structure ID from decrypted cipher" );
            return VLC_EGENERIC;
        }
        try
        {
            s_carrier.append( psz_hex );
        }
        catch( ... )
        {
            msg_Err( this->p_demux, "error while handling string" );
            return VLC_EGENERIC;
        }
        i_pos++;
    }
    if( s_carrier.compare( s_rsa_structID ) )
    {
        msg_Err( this->p_demux, "incorrect RSA structure ID: KDM may be broken" );
        return VLC_EGENERIC;
    }

    i_pos += 36;        /* TODO thumbprint, CPL ID */
    if( smpte )         /* only SMPTE DCPs have the 4-byte "KeyType" field */
        i_pos += 4;

    /* extract the AES key UUID */
    if( ( this->s_key_id = createUUID( ps_plain_text + i_pos ) ).empty() )
    {
        msg_Err( this->p_demux, "error while extracting AES Key UUID" );
        return VLC_EGENERIC;
    }
    i_pos += 16;

    i_pos += 50; /* TODO KeyEpoch */

    /* extract the AES key */
    memcpy( this->ps_key, ps_plain_text + i_pos, 16 );

    return VLC_SUCCESS;
}


/*
 * RSAKey class
 */

/*
 * gets the private key path (always stored in the VLC config dir and called "priv.key" )
 */
int RSAKey::setPath( )
{
    char *psz_config_dir = NULL;

    if( ! ( psz_config_dir = config_GetUserDir( VLC_CONFIG_DIR ) ) )
    {
        msg_Err( this->p_demux, "could not read user config dir" );
        goto error;
    }
    try
    {
        this->s_path.assign( psz_config_dir );
        this->s_path.append( "/priv.key" );
    }
    catch( ... )
    {
        msg_Err( this->p_demux, "error while handling string" );
        goto error;
    }

    free( psz_config_dir );
    return VLC_SUCCESS;

error:
    free( psz_config_dir );
    return VLC_EGENERIC;
}

/*
 * reads the RSA private key from file
 * the file must be conform to PCKS#1, PEM-encoded, unencrypted
 */
int RSAKey::readPEM( )
{
    string s_header_tag( "-----BEGIN RSA PRIVATE KEY-----" );
    string s_footer_tag( "-----END RSA PRIVATE KEY-----" );
    string s_line;
    string s_data_b64;
    unsigned char *ps_data_der = NULL;
    size_t length;

    /* open key file */
    ifstream file( this->s_path.c_str(), ios::in );
    if( ! file.is_open() )
    {
        msg_Err( this->p_demux, "could not open private key file" );
        goto error;
    }

    /* check for header tag */
    if( ! getline( file, s_line ) )
    {
        msg_Err( this->p_demux, "could not read private key file" );
        goto error;
    }
    if( s_line.compare( s_header_tag ) )
    {
        msg_Err( this->p_demux, "unexpected header tag found in private key file" );
        goto error;
    }

    /* read file until footer tag is found */
    while( getline( file, s_line ) )
    {
        if( ! s_line.compare( s_footer_tag ) )
            break;
        try
        {
            s_data_b64.append( s_line );
        }
        catch( ... )
        {
            msg_Err( this->p_demux, "error while handling string" );
            goto error;
        }
    }
    if( ! file )
    {
        msg_Err( this->p_demux, "error while reading private key file; footer tag may be missing" );
        goto error;
    }

    /* decode data from Base64 */
    if( ! ( length = vlc_b64_decode_binary( &ps_data_der, s_data_b64.c_str() ) ) )
    {
        msg_Err( this->p_demux, "could not decode from Base64" );
        goto error;
    }

    /* extract key S-expression from DER-encoded data */
    if( this->readDER( ps_data_der, length ) )
        goto error;

    /* clear data */
    free( ps_data_der );
    return VLC_SUCCESS;

error:
    free( ps_data_der );
    return VLC_EGENERIC;
}

/*
 * Parse the DER-encoded data at ps_data_der
 * saving the key in an S-expression
 */
int RSAKey::readDER( unsigned char const* ps_data_der, size_t length )
{
    struct tag_info tag_inf;
    gcry_mpi_t key_params[8] = { NULL };
    gcry_error_t err;

    /* parse the ASN1 structure */
    if( parseTag( &ps_data_der, &length, &tag_inf )
            || tag_inf.tag != TAG_SEQUENCE || tag_inf.class_ || !tag_inf.cons || tag_inf.ndef )
        goto bad_asn1;
    if( parseTag( &ps_data_der, &length, &tag_inf )
       || tag_inf.tag != TAG_INTEGER || tag_inf.class_ || tag_inf.cons || tag_inf.ndef )
        goto bad_asn1;
    if( tag_inf.length != 1 || *ps_data_der )
        goto bad_asn1;  /* The value of the first integer is no 0. */
    ps_data_der += tag_inf.length;
    length -= tag_inf.length;

    for( int i = 0; i < 8; i++ )
    {
        if( parseTag( &ps_data_der, &length, &tag_inf )
                || tag_inf.tag != TAG_INTEGER || tag_inf.class_ || tag_inf.cons || tag_inf.ndef )
            goto bad_asn1;
        err = gcry_mpi_scan( key_params + i, GCRYMPI_FMT_USG, ps_data_der, tag_inf.length, NULL );
        if( err )
        {
            msg_Err( this->p_demux, "error scanning RSA parameter %d: %s", i, gpg_strerror( err ) );
            goto error;
        }
        ps_data_der += tag_inf.length;
        length -= tag_inf.length;
    }

    /* Convert from OpenSSL parameter ordering to the OpenPGP order.
     * First check that p < q; if not swap p and q and recompute u.
     */
    if( gcry_mpi_cmp( key_params[3], key_params[4] ) > 0 )
    {
        gcry_mpi_swap( key_params[3], key_params[4] );
        gcry_mpi_invm( key_params[7], key_params[3], key_params[4] );
    }

    /* Build the S-expression.  */
    err = gcry_sexp_build( & this->priv_key, NULL,
                         "(private-key(rsa(n%m)(e%m)(d%m)(p%m)(q%m)(u%m)))",
                         key_params[0], key_params[1], key_params[2],
                         key_params[3], key_params[4], key_params[7] );
    if( err )
    {
        msg_Err( this->p_demux, "error building S-expression: %s", gpg_strerror( err ) );
        goto error;
    }

    /* clear data */
    for( int i = 0; i < 8; i++ )
        gcry_mpi_release( key_params[i] );
    return VLC_SUCCESS;

bad_asn1:
    msg_Err( this->p_demux, "could not parse ASN1 structure; key might be corrupted" );

error:
    for( int i = 0; i < 8; i++ )
        gcry_mpi_release( key_params[i] );
    return VLC_EGENERIC;
}

/*
 * Parse the buffer at the address BUFFER which consists of the number
 * of octets as stored at BUFLEN.  Return the tag and the length part
 * from the TLV triplet.  Update BUFFER and BUFLEN on success.  Checks
 * that the encoded length does not exhaust the length of the provided
 * buffer.
 */
int RSAKey::parseTag( unsigned char const **buffer, size_t *buflen, struct tag_info *ti)
{
  int c;
  unsigned long tag;
  const unsigned char *buf = *buffer;
  size_t length = *buflen;

  ti->length = 0;
  ti->ndef = 0;
  ti->nhdr = 0;

  /* Get the tag */
  if (!length)
    return -1; /* Premature EOF.  */
  c = *buf++; length--;
  ti->nhdr++;

  ti->class_ = (c & 0xc0) >> 6;
  ti->cons   = !!(c & 0x20);
  tag        = (c & 0x1f);

  if (tag == 0x1f)
    {
      tag = 0;
      do
        {
          tag <<= 7;
          if (!length)
            return -1; /* Premature EOF.  */
          c = *buf++; length--;
          ti->nhdr++;
          tag |= (c & 0x7f);
        }
      while ( (c & 0x80) );
    }
  ti->tag = tag;

  /* Get the length */
  if (!length)
    return -1; /* Premature EOF. */
  c = *buf++; length--;
  ti->nhdr++;

  if ( !(c & 0x80) )
    ti->length = c;
  else if (c == 0x80)
    ti->ndef = 1;
  else if (c == 0xff)
    return -1; /* Forbidden length value.  */
  else
    {
      unsigned long len = 0;
      int count = c & 0x7f;

      for (; count; count--)
        {
          len <<= 8;
          if (!length)
            return -1; /* Premature EOF.  */
          c = *buf++; length--;
          ti->nhdr++;
          len |= (c & 0xff);
        }
      ti->length = len;
    }

  if (ti->class_ == 0 && !ti->tag)
    ti->length = 0;

  if (ti->length > length)
    return -1; /* Data larger than buffer.  */

  *buffer = buf;
  *buflen = length;
  return 0;
}
