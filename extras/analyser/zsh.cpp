/*****************************************************************************
 * zsh.cpp: create zsh completion rule for vlc
 *****************************************************************************
 * Copyright © 2005-2011 the VideoLAN team
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
            Rafaël Carré <funman@videolanorg>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <utility>
#include <iostream>
#include <algorithm>

typedef std::pair<std::string, std::string> mpair;
typedef std::multimap<std::string, std::string> mumap;
mumap mods;

typedef std::pair<int, std::string> mcpair;
typedef std::multimap<int, std::string> mcmap;
mcmap mods2;


#include <vlc_common.h>
#include <vlc/vlc.h>
#include <vlc_modules.h>

/* evil hack */
#undef __PLUGIN__
#include <../src/modules/modules.h>

static void ReplaceChars(char *str)
{
    if (str) {
        char *parser;
        while ((parser = strchr(str, ':'))) *parser=';' ;
        while ((parser = strchr(str, '"'))) *parser='\'' ;
        while ((parser = strchr(str, '`'))) *parser='\'' ;
    }
}

static void PrintOption(const module_config_t *item, const std::string &opt,
                        const std::string &excl, const std::string &args)
{
    char *longtext = item->psz_longtext;
    char *text = item->psz_text;
    char i_short = item->i_short;
    ReplaceChars(longtext);
    ReplaceChars(text);

    if (!longtext || strchr(longtext, '\n') || strchr(longtext, '('))
        longtext = text;

    printf("  \"");

    const char *args_c = args.empty() ? "" : "=";
    if (i_short) {
        printf("(-%c", i_short);

        if (!excl.empty())
            printf("%s", excl.c_str());

        printf(")--%s%s[%s]", opt.c_str(), args_c, text);

        if (!args.empty())
            printf(":%s:%s", longtext, args.c_str());

        printf("\"\\\n  \"(--%s%s)-%c", opt.c_str(), excl.c_str(), i_short);
    } else {
        if (!excl.empty())
            printf("(%s)", excl.c_str());
        printf("--%s", opt.c_str());
        if (!excl.empty())
            printf("%s", args_c);
    }

    printf("[%s]", text);
    if (!args.empty())
        printf( ":%s:%s", longtext, args.c_str());
    puts( "\"\\");
}

static void ParseOption(const module_config_t *item)
{
    std::string excl, args;
    std::string list;
    std::pair<mcmap::iterator, mcmap::iterator> range;
    std::pair<mumap::iterator, mumap::iterator> range_mod;

    if (item->b_removed)
        return;

    switch(item->i_type)
    {
    case CONFIG_ITEM_MODULE:
        range_mod = mods.equal_range(item->psz_type);
        list = (*range_mod.first).second;
        if (range_mod.first != range_mod.second) {
            while (range_mod.first++ != range_mod.second)
                list += " " + range_mod.first->second;
            args = std::string("(") + list + ")";
        }
    break;

    case CONFIG_ITEM_MODULE_CAT:
        range = mods2.equal_range(item->min.i);
        list = (*range.first).second;
        if (range.first != range.second) {
            while (range.first++ != range.second)
                list += " " + range.first->second;
            args = std::string("(") + list + ")";
        }
    break;

    case CONFIG_ITEM_MODULE_LIST_CAT:
        range = mods2.equal_range(item->min.i);
        args = std::string("_values -s , ") + item->psz_name;
        while (range.first != range.second)
            args += " '*" + range.first++->second + "'";
    break;

    case CONFIG_ITEM_LOADFILE:
    case CONFIG_ITEM_SAVEFILE:
        args = "_files";
        break;
    case CONFIG_ITEM_DIRECTORY:
        args = "_files -/";
        break;

    case CONFIG_ITEM_STRING:
    case CONFIG_ITEM_INTEGER:
        if (item->i_list == 0)
            break;

        for (int i = 0; i < item->i_list; i++) {
            std::string val;
            if (item->ppsz_list_text) {
                const char *text = item->ppsz_list_text[i];
                if (item->i_type == CONFIG_ITEM_INTEGER) {
                    std::stringstream s;
                    s << item->pi_list[i];
                    val = s.str() + "\\:\\\"" + text;
                } else {
                    if (!item->ppsz_list[i] || !text)
                        continue;
                    val = item->ppsz_list[i] + std::string("\\:\\\"") + text;
                }
            } else
                val = std::string("\\\"") + item->ppsz_list[i];

            list = val + "\\\" " + list;
        }

        if (item->ppsz_list_text)
            args = std::string("((") + list + "))";
        else
            args = std::string("(") + list + ")";

        break;

    case CONFIG_ITEM_BOOL:
        excl = std::string("--no") + item->psz_name + " --no-" + item->psz_name;
        PrintOption(item, item->psz_name,                       excl, args);

        excl = std::string("--no") + item->psz_name + " --"    + item->psz_name;
        PrintOption(item, std::string("no-") + item->psz_name,  excl, args);

        excl = std::string("--no-")+ item->psz_name + " --"  + item->psz_name;
        PrintOption(item, std::string("no") + item->psz_name,   excl, args);
        return;

    case CONFIG_ITEM_KEY:
    case CONFIG_SECTION:
    case CONFIG_ITEM_FLOAT:
    default:
        break;
    }

    PrintOption(item, item->psz_name, "", args);
}

static void PrintModuleList()
{
    printf("vlc_modules=\"");

    size_t modules = 0;
    module_t **list = module_list_get(&modules);

    if (!list || modules == 0)
        return;

    for (module_t **pmod = list; pmod < &list[modules]; pmod++) {
        /* Exclude empty plugins (submodules don't have config options, they
         * are stored in the parent module) */

        module_t *mod = *pmod;
        if (!strcmp(mod->pp_shortcuts[0], "main"))
            continue;

        const char *capability = mod->psz_capability ? mod->psz_capability : "";
        mods.insert(mpair(capability, mod->pp_shortcuts[0]));

        module_config_t *max = &mod->p_config[mod->i_config_items];
        for (module_config_t *cfg = mod->p_config; cfg && cfg < max; cfg++)
            if (cfg->i_type == CONFIG_SUBCATEGORY)
                mods2.insert(mcpair(cfg->value.i, mod->pp_shortcuts[0]));

        if (!mod->parent)
            printf("%s ", mod->pp_shortcuts[0]);
    }
    puts("\"\n");
    module_list_free(list);
}

static void ParseModules()
{
    size_t modules = 0;
    module_t **list = module_list_get(&modules);

    if (!list || modules == 0)
        return;

    for (module_t **pmod = list; pmod < &list[modules]; pmod++) {
        /* Exclude empty plugins (submodules don't have config options, they
         * are stored in the parent module) */
        module_t *mod = *pmod;
        if (mod->parent)
            continue;

        module_config_t *max = mod->p_config + mod->confsize;
        for (module_config_t *cfg = mod->p_config; cfg && cfg < max; cfg++)
            if (CONFIG_ITEM(cfg->i_type))
                ParseOption(cfg);
    }
    module_list_free(list);
}

int main(int argc, const char **argv)
{
    libvlc_instance_t *libvlc = libvlc_new(argc, argv);
    if (!libvlc)
        return 1;

    puts("#compdef vlc cvlc rvlc svlc mvlc qvlc nvlc\n"
           "#This file is autogenerated by zsh.cpp"
           "typeset -A opt_args"
           "local context state line ret=1"
           "local modules\n");

    PrintModuleList();

    puts("_arguments -S -s \\");
    ParseModules();
    puts("  \"(--module)-p[print help on module]:print help on module:($vlc_modules)\"\\");
    puts("  \"(-p)--module[print help on module]:print help on module:($vlc_modules)\"\\");
    puts("  \"(--help)-h[print help]\"\\");
    puts("  \"(-h)--help[print help]\"\\");
    puts("  \"(--longhelp)-H[print detailed help]\"\\");
    puts("  \"(-H)--longhelp[print detailed help]\"\\");
    puts("  \"(--list)-l[print a list of available modules]\"\\");
    puts("  \"(-l)--list[print a list of available modules]\"\\");
    puts("  \"--reset-config[reset the current config to the default values]\"\\");
    puts("  \"--config[use alternate config file]\"\\");
    puts("  \"--reset-plugins-cache[resets the current plugins cache]\"\\");
    puts("  \"--version[print version information]\"\\");
    puts("  \"*:Playlist item:->mrl\" && ret=0\n");

    puts("case $state in");
    puts("  mrl)");
    puts("    _alternative 'files:file:_files' 'urls:URL:_urls' && ret=0");
    puts("  ;;");
    puts("esac\n");

    puts("return ret");

    libvlc_release(libvlc);

    return 0;
}
