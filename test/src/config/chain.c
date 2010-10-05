/*****************************************************************************
 * chain.c: test configuration chains
 *****************************************************************************
 * Copyright (C) 2010 VideoLAN and authors
 * $Id$
 *
 * Authors: Rémi Duraffort <ivoire@videolan.org>
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

#include <stdlib.h>
#include <string.h>

#include "../../libvlc/test.h"

#include <vlc_common.h>
#include <vlc_configuration.h>

typedef struct
{
    const char *psz_string;
    const char *psz_escaped;
}escape_sample_t;

static const escape_sample_t escape_samples[] =
{
    { "a",          "a"             },
    { "azertyuiop", "azertyuiop"    },
    { "  test    ", "  test    "    },
    { "it's",       "it\\'s"        },
    { "''''",       "\\'\\'\\'\\'"  },
    { "' a '",      "\\' a \\'"     },
    { "\"quote\"",  "\\\"quote\\\"" },
    { " az\" ",     " az\\\" "      },
    { "\\test",     "\\\\test"      },
    { NULL,         NULL            }
};

static void test_config_StringEscape()
{
    for( int i = 0; escape_samples[i].psz_string; i++ )
    {
        char *psz_tmp = config_StringEscape( escape_samples[i].psz_string );
        assert( !strcmp( psz_tmp, escape_samples[i].psz_escaped ) );
        free( psz_tmp );
    }
}

static void test_config_StringUnEscape()
{
    for( int i = 0; escape_samples[i].psz_string; i++ )
    {
        char *psz_tmp = strdup( escape_samples[i].psz_escaped );
        config_StringUnescape( psz_tmp );
        assert( !strcmp( psz_tmp, escape_samples[i].psz_string ) );
        free( psz_tmp );
    }
}


typedef struct
{
    const char *psz_name;
    const char *psz_value;
}pair_t;

typedef struct
{
    const char *psz_string;
    const char *psz_module;
    const char *psz_next;
    pair_t      config[4];
}chain_sample_t;

static const chain_sample_t chain_samples[] =
{
    { "module1",                         "module1", NULL, { { NULL, NULL } } },
    { "bla{}",                           "bla",     NULL, { { NULL, NULL } } },
    { "module{a=b}:module2{name=value}", "module",  "module2{name=value}",
        { { "a",  "b"  },
          { NULL, NULL } } },
    { "éç€{a=b}",                        "éç€",     NULL,
        { { "a",  "b"  },
          { NULL, NULL } } },
    { "mod{listen=127.0.0.1:80}",        "mod",     NULL,
        { { "listen", "127.0.0.1:80" },
          { NULL,     NULL           } } },
    { "module:module2",                  "module",  "module2", { { NULL, NULL } } },
    { "mod{çé=\"arg'\",bla='bip'}",      "mod",     NULL,
        { { "çé",  "arg'"  },
          { "bla", "bip"  },
          { NULL, NULL } } },
    { "mod{a=b,  c=d, a_i=f}:mod2{b=c}", "mod",     "mod2{b=c}",
        { { "a",   "b"  },
          { "c",   "d"  },
          { "a_i", "f"  },
          { NULL,  NULL } } },
    { NULL,                              NULL,      NULL, { { NULL, NULL } } }
};

static void test_config_ChainCreate()
{
    for( int i = 0; chain_samples[i].psz_string; i++ )
    {
        config_chain_t *p_cfg;
        char *psz_module;
        char *psz_next = config_ChainCreate( &psz_module, &p_cfg, chain_samples[i].psz_string );

        assert( !strcmp( chain_samples[i].psz_module, psz_module ) );
        assert( (!psz_next && !chain_samples[i].psz_next) || !strcmp( chain_samples[i].psz_next, psz_next ) );

        config_chain_t *p_tmp = p_cfg;
        for( int j = 0; chain_samples[i].config[j].psz_name; j++)
        {
            assert( !strcmp( chain_samples[i].config[j].psz_name,  p_tmp->psz_name ) &&
                    !strcmp( chain_samples[i].config[j].psz_value, p_tmp->psz_value ) );
            p_tmp = p_tmp->p_next;
        }
        assert( !p_tmp );

        config_ChainDestroy( p_cfg );
        free( psz_next );
        free( psz_module );
    }
}


static const char *ppsz_string[] =
{
    "bla",
    "module1{a=b, b=c}",
    "a{re=\"errtetyegzrf\", b=c, vetrjtrjt=erte234tth:12}",
    "module1{a=b, b=c}:std{lang=C}",
    NULL
};

static void check_config_equality( config_chain_t *p_cfg1, config_chain_t *p_cfg2 )
{
    while(p_cfg1 && p_cfg2)
    {
        assert( !strcmp( p_cfg1->psz_name,  p_cfg2->psz_name ) &&
                !strcmp( p_cfg1->psz_value, p_cfg2->psz_value ) );

        p_cfg1 = p_cfg1->p_next;
        p_cfg2 = p_cfg2->p_next;
    }
    assert(!p_cfg1 && !p_cfg2);
}

static void test_config_ChainDuplicate()
{
    for( int i = 0; ppsz_string[i]; i++ )
    {
        char *psz_module;
        config_chain_t *p_cfg;
        char *psz_next = config_ChainCreate( &psz_module, &p_cfg, ppsz_string[i] );

        config_chain_t *p_cfg_copy = config_ChainDuplicate( p_cfg );
        check_config_equality(p_cfg, p_cfg_copy);

        config_ChainDestroy( p_cfg_copy );
        config_ChainDestroy( p_cfg );
        free( psz_next );
        free( psz_module );
    }
}


int main( void )
{
    log( "Testing config chain escaping\n" );
    test_config_StringEscape();
    log( "Testing config chain un-escaping\n" );
    test_config_StringUnEscape();
    log( "Testing config_ChainCreate()\n" );
    test_config_ChainCreate();
    log( "Testing config_ChainDuplicate()\n" );
    test_config_ChainDuplicate();

    return 0;
}
