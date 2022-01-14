/*****************************************************************************
 * vlc_modules.h : Module descriptor and load functions
 *****************************************************************************
 * Copyright (C) 2001-2011 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#ifndef VLC_MODULES_H
#define VLC_MODULES_H 1

/**
 * \file
 * This file defines functions for modules in vlc
 */

typedef int (*vlc_activate_t)(void *func, bool forced, va_list args);
struct vlc_logger;

/*****************************************************************************
 * Exported functions.
 *****************************************************************************/

/**
 * Finds the candidate modules for given criteria.
 *
 * All candidates modules having the specified capability and name will be
 * sorted in decreasing order of priority and returned in a heap-allocated
 * table.
 *
 * \param capability capability, i.e. class of module
 * \param names string of comma-separated requested module shortcut names,
 *              or NULL for defaults
 * \param strict whether to exclude modules with no unmatching shortcut names
 * \param modules storage location for the base address of a sorted table
 *                of candidate modules (NULL on error) [OUT]
 * \param strict_matches storage location for the count of strictly matched
 *                       modules [OUT]
 * \return number of modules found or a strictly negative value on error
 */
VLC_API
ssize_t vlc_module_match(const char *capability, const char *names,
                         bool strict, module_t ***restrict modules,
                         size_t *restrict strict_matches);

/**
 * Maps a module in memory.
 *
 * This function attempts to map a given module in memory, if it is not
 * already mapped. If it is already mapped, this function does nothing.
 *
 * \param log message logger
 * \param mod module to map
 *
 * \return the module activation function on success, NULL on failure
 */
VLC_API
void *vlc_module_map(struct vlc_logger *log, module_t *mod);

/**
 * Finds and instantiates the best module of a certain type.
 * All candidates modules having the specified capability and name will be
 * sorted in decreasing order of priority. Then the probe callback will be
 * invoked for each module, until it succeeds (returns 0), or all candidate
 * module failed to initialize.
 *
 * The probe callback first parameter is the address of the module entry point.
 * Further parameters are passed as an argument list; it corresponds to the
 * variable arguments passed to this function. This scheme is meant to
 * support arbitrary prototypes for the module entry point.
 *
 * \param log logger for debugging (or NULL to ignore)
 * \param capability capability, i.e. class of module
 * \param name name of the module asked, if any
 * \param strict if true, do not fallback to plugin with a different name
 *                 but the same capability
 * \param probe module probe callback
 * \return the module or NULL in case of a failure
 */
VLC_API module_t *vlc_module_load(struct vlc_logger *log, const char *cap,
                                  const char *name, bool strict,
                                  vlc_activate_t probe, ... ) VLC_USED;
#ifndef __cplusplus
#define vlc_module_load(ctx, cap, name, strict, ...) \
    _Generic ((ctx), \
        struct vlc_logger *: \
            vlc_module_load((void *)(ctx), cap, name, strict, __VA_ARGS__), \
        void *: \
            vlc_module_load((void *)(ctx), cap, name, strict, __VA_ARGS__), \
        default: \
            vlc_module_load(vlc_object_logger((vlc_object_t *)(ctx)), cap, \
                            name, strict, __VA_ARGS__))
#endif

VLC_API module_t * module_need( vlc_object_t *, const char *, const char *, bool ) VLC_USED;
#define module_need(a,b,c,d) module_need(VLC_OBJECT(a),b,c,d)

VLC_USED
static inline module_t *module_need_var(vlc_object_t *obj, const char *cap,
                                        const char *varname)
{
    char *list = var_InheritString(obj, varname);
    if (unlikely(list == NULL))
        return NULL;

    module_t *m = module_need(obj, cap, list, false);

    free(list);
    return m;
}
#define module_need_var(a,b,c) module_need_var(VLC_OBJECT(a),b,c)

VLC_API void module_unneed( vlc_object_t *, module_t * );
#define module_unneed(a,b) module_unneed(VLC_OBJECT(a),b)

/**
 * Get a pointer to a module_t given it's name.
 *
 * \param name the name of the module
 * \return a pointer to the module or NULL in case of a failure
 */
VLC_API module_t *module_find(const char *name) VLC_USED;

/**
 * Checks if a module exists.
 *
 * \param name name of the module
 * \retval true if the module exists
 * \retval false if the module does not exist (in the running installation)
 */
VLC_USED static inline bool module_exists(const char * name)
{
    return module_find(name) != NULL;
}

/**
 * Gets the table of module configuration items.
 *
 * \note Use module_config_free() to release the allocated memory.
 *
 * \param module the module
 * \param psize the size of the configuration returned
 * \return the configuration as an array
 */
VLC_API module_config_t *module_config_get(const module_t *module,
                                           unsigned *restrict psize) VLC_USED;

/**
 * Releases a configuration items table.
 *
 * \param tab base address of a table returned by module_config_get()
 */
VLC_API void module_config_free( module_config_t *tab);

/**
 * Frees a flat list of VLC modules.
 *
 * \param list list obtained by module_list_get()
 */
VLC_API void module_list_free(module_t **);

/**
 * Gets the flat list of VLC modules.
 *
 * \param n [OUT] pointer to the number of modules
 * \return table of module pointers (release with module_list_free()),
 *         or NULL in case of error (in that case, *n is zeroed).
 */
VLC_API module_t ** module_list_get(size_t *n) VLC_USED;

/**
 * Checks whether a module implements a capability.
 *
 * \param m the module
 * \param cap the capability to check
 * \retval true if the module has the capability
 * \retval false if the module has another capability
 */
VLC_API bool module_provides(const module_t *m, const char *cap);

/**
 * Gets the internal name of a module.
 *
 * \param m the module
 * \return the module name
 */
VLC_API const char * module_get_object(const module_t *m) VLC_USED;

/**
 * Gets the human-friendly name of a module.
 *
 * \param m the module
 * \param longname TRUE to have the long name of the module
 * \return the short or long name of the module
 */
VLC_API const char *module_get_name(const module_t *m, bool longname) VLC_USED;
#define module_GetShortName( m ) module_get_name( m, false )
#define module_GetLongName( m ) module_get_name( m, true )

/**
 * Gets the help text for a module.
 *
 * \param m the module
 * \return the help
 */
VLC_API const char *module_get_help(const module_t *m) VLC_USED;

/**
 * Gets the capability string of a module.
 *
 * \param m the module
 * \return the capability, or "none" if unspecified
 */
VLC_API const char *module_get_capability(const module_t *m) VLC_USED;

/**
 * Gets the precedence of a module.
 *
 * \param m the module
 * return the score for the capability
 */
VLC_API int module_get_score(const module_t *m) VLC_USED;

/**
 * Translates a string using the module's text domain
 *
 * \param m the module
 * \param s the American English ASCII string to localize
 * \return the gettext-translated string
 */
VLC_API const char *module_gettext(const module_t *m, const char *s) VLC_USED;

VLC_USED static inline module_t *module_get_main (void)
{
    return module_find ("core");
}

VLC_USED static inline bool module_is_main( const module_t * p_module )
{
    return !strcmp( module_get_object( p_module ), "core" );
}

#endif /* VLC_MODULES_H */
