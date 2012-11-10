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

#include <vlc/vlc.h>
#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include "../src/modules/modules.h" /* evil hack */

typedef std::pair<std::string, std::string> mpair;
typedef std::multimap<std::string, std::string> mumap;
mumap capabilities;

typedef std::pair<int, std::string> mcpair;
typedef std::multimap<int, std::string> mcmap;
mcmap categories;

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
        range_mod = capabilities.equal_range(item->psz_type);
        args = "(" + (*range_mod.first).second;
        while (range_mod.first++ != range_mod.second)
            args += " " + range_mod.first->second;
        args += ")";
    break;

    case CONFIG_ITEM_MODULE_CAT:
        range = categories.equal_range(item->min.i);
        args = "(" + (*range.first).second;
        while (range.first++ != range.second)
            args += " " + range.first->second;
        args += ")";
    break;

    case CONFIG_ITEM_MODULE_LIST_CAT:
        range = categories.equal_range(item->min.i);
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
        if (item->list_count == 0)
            break;

        for (int i = 0; i < item->list_count; i++) {
            std::string val;
            if (item->list_text) {
                const char *text = item->list_text[i];
                if (item->i_type == CONFIG_ITEM_INTEGER) {
                    std::stringstream s;
                    s << item->list.i[i];
                    val = s.str() + "\\:\\\"" + text;
                } else {
                    if (!item->list.psz[i] || !text)
                        continue;
                    val = item->list.psz[i] + std::string("\\:\\\"") + text;
                }
            } else
                val = std::string("\\\"") + item->list.psz[i];

            list = val + "\\\" " + list;
        }

        if (item->list_text)
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

static void PrintModule(const module_t *mod)
{
    const char *name = mod->pp_shortcuts[0];
    if (!strcmp(name, "main"))
        return;

    if (mod->psz_capability)
        capabilities.insert(mpair(mod->psz_capability, name));

    module_config_t *max = &mod->p_config[mod->i_config_items];
    for (module_config_t *cfg = mod->p_config; cfg && cfg < max; cfg++)
        if (cfg->i_type == CONFIG_SUBCATEGORY)
            categories.insert(mcpair(cfg->value.i, name));

    if (!mod->parent)
        printf("%s ", name);
}

static void ParseModule(const module_t *mod)
{
    if (mod->parent)
        return;

    module_config_t *max = mod->p_config + mod->confsize;
    for (module_config_t *cfg = mod->p_config; cfg && cfg < max; cfg++)
        if (CONFIG_ITEM(cfg->i_type))
            ParseOption(cfg);
}

int main(int argc, const char **argv)
{
    libvlc_instance_t *libvlc = libvlc_new(argc, argv);
    if (!libvlc)
        return 1;

    size_t modules = 0;
    module_t **mod_list;

    mod_list = module_list_get(&modules);
    if (!mod_list || modules == 0)
        return 2;

    module_t **max = &mod_list[modules];

    puts("#compdef vlc cvlc rvlc svlc mvlc qvlc nvlc\n"
           "#This file is autogenerated by zsh.cpp"
           "typeset -A opt_args"
           "local context state line ret=1"
           "local modules\n");

    printf("vlc_modules=\"");
    for (module_t **mod = mod_list; mod < max; mod++)
        PrintModule(*mod);
    puts("\"\n");

    puts("_arguments -S -s \\");
    for (module_t **mod = mod_list; mod < max; mod++)
        ParseModule(*mod);
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

    module_list_free(mod_list);
    libvlc_release(libvlc);
    return 0;
}
