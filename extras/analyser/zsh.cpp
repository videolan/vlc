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

#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <sstream>

#include <vlc/vlc.h>
#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>

typedef std::pair<std::string, std::string> mpair;
typedef std::multimap<std::string, std::string> mumap;
mumap capabilities;

typedef std::pair<int, std::string> mcpair;
typedef std::multimap<int, std::string> mcmap;
mcmap categories;

std::set<std::string> mnames;

static void ReplaceChars(std::string& str)
{
    std::replace(str.begin(), str.end(), ':', ';');
    std::replace(str.begin(), str.end(), '"', '\'');
    std::replace(str.begin(), str.end(), '`', '\'');
}

static void PrintOption(const module_config_t *item, const std::string &opt,
                        const std::string &excl, const std::string &args)
{
    std::string longtext = item->psz_longtext ? item->psz_longtext : "";
    std::string text = item->psz_text ? item->psz_text : "";
    char i_short = item->i_short;
    ReplaceChars(longtext);
    ReplaceChars(text);

    if (!longtext.length() || longtext.find('\n') != std::string::npos || longtext.find('(') != std::string::npos)
        longtext = text;

    std::cout << "  \"";

    const char *args_c = args.empty() ? "" : "=";
    if (i_short) {
        std::cout << "(-" << i_short;

        if (!excl.empty())
            std::cout << excl;

        std::cout << ")--" << opt << args_c << "[" << text << "]";

        if (!args.empty())
            std::cout << ":" << longtext << ":" << args;

        std::cout << "\"\\\n  \"(--" << opt << excl << ")-" << i_short;
    } else {
        if (!excl.empty())
            std::cout << "(" << excl << ")";
        std::cout << "--" << opt;
        if (!excl.empty())
            std::cout << args_c;
    }

    std::cout << "[" << text << "]";
    if (!args.empty())
        std::cout << ":" << longtext << ":" << args;
    std::cout << "\"\\\n";
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
        range_mod = capabilities.equal_range(item->psz_type ? item->psz_type : "");
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
    if (module_is_main(mod))
        return;

    const char *name = module_get_object(mod);
    const char *cap = module_get_capability(mod);

    if (strcmp(cap, "none"))
        capabilities.insert(mpair(cap, name));

    unsigned int cfg_size = 0;
    module_config_t *cfg_list = module_config_get(mod, &cfg_size);

    for (unsigned int j = 0; j < cfg_size; ++j)
    {
        const module_config_t *cfg = cfg_list + j;
        if (cfg->i_type == CONFIG_SUBCATEGORY)
            categories.insert(mcpair(cfg->value.i, name));
    }

    module_config_free(cfg_list);

    if (mnames.find(name) == mnames.end())
    {
        std::cout << name << " ";
        mnames.insert(name);
    }
}

static void ParseModule(const module_t *mod)
{
    unsigned int cfg_size = 0;
    module_config_t *cfg_list = module_config_get(mod, &cfg_size);

    for (unsigned int j = 0; j < cfg_size; ++j)
    {
        const module_config_t *cfg = cfg_list + j;
        if (CONFIG_ITEM(cfg->i_type))
            ParseOption(cfg);
    }

    module_config_free(cfg_list);
}

int main(int argc, const char **argv)
{
    libvlc_instance_t *libvlc = libvlc_new(argc - 1, argv + 1);
    if (!libvlc)
        return 1;

    size_t modules = 0;
    module_t **mod_list;

    mod_list = module_list_get(&modules);
    if (!mod_list || modules == 0)
    {
        libvlc_release(libvlc);
        return 2;
    }

    module_t **max = &mod_list[modules];

    std::cout << "#compdef vlc cvlc rvlc svlc mvlc qvlc nvlc\n"
           "#This file is autogenerated by zsh.cpp\n"
           "typeset -A opt_args\n"
           "local context state line ret=1\n"
           "local modules\n\n";

    std::cout << "vlc_modules=\"";
    for (module_t **mod = mod_list; mod < max; mod++)
        PrintModule(*mod);
    std::cout << "\"\n\n";

    std::cout << "_arguments -S -s \\\n";
    for (module_t **mod = mod_list; mod < max; mod++)
        ParseModule(*mod);
    std::cout << "  \"(--module)-p[print help on module]:print help on module:($vlc_modules)\"\\\n";
    std::cout << "  \"(-p)--module[print help on module]:print help on module:($vlc_modules)\"\\\n";
    std::cout << "  \"(--help)-h[print help]\"\\\n";
    std::cout << "  \"(-h)--help[print help]\"\\\n";
    std::cout << "  \"(--longhelp)-H[print detailed help]\"\\\n";
    std::cout << "  \"(-H)--longhelp[print detailed help]\"\\\n";
    std::cout << "  \"(--list)-l[print a list of available modules]\"\\\n";
    std::cout << "  \"(-l)--list[print a list of available modules]\"\\\n";
    std::cout << "  \"--reset-config[reset the current config to the default values]\"\\\n";
    std::cout << "  \"--config[use alternate config file]\"\\\n";
    std::cout << "  \"--reset-plugins-cache[resets the current plugins cache]\"\\\n";
    std::cout << "  \"--version[print version information]\"\\\n";
    std::cout << "  \"*:Playlist item:->mrl\" && ret=0\n\n";

    std::cout << "case $state in\n";
    std::cout << "  mrl)\n";
    std::cout << "    _alternative 'files:file:_files' 'urls:URL:_urls' && ret=0\n";
    std::cout << "  ;;\n";
    std::cout << "esac\n\n";

    std::cout << "return ret\n";

    module_list_free(mod_list);
    libvlc_release(libvlc);
    return 0;
}
