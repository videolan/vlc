// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * LazyPreparser.h: class to create a preparser at the first request
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifndef LAZYPREPARSER_H
#define LAZYPREPARSER_H

#include <vlc_common.h>
#include <vlc_preparser.h>

class LazyPreparser
{
    public:
        LazyPreparser(vlc_object_t *obj, const struct vlc_preparser_cfg cfg)
            : m_obj(obj)
            , m_cfg(cfg)
            , m_preparser(nullptr, &vlc_preparser_Delete)
        {
            assert(obj != NULL);
        }

        vlc_preparser_t *instance()
        {
            vlc::threads::mutex_locker locker(m_mutex);

            if (m_preparser.get() == nullptr) {
                m_preparser.reset(vlc_preparser_New(m_obj, &m_cfg));
                if (m_preparser.get() == nullptr) {
                    msg_Warn(m_obj, "LazyPreparser: Failed to instantiate a vlc_preparser_t!");
                } else {
                    msg_Dbg(m_obj, "LazyPreparser: vlc_preparser_t created!");
                }
            }
            return m_preparser.get();
        }

        vlc_preparser_t *get()
        {
            vlc::threads::mutex_locker locker(m_mutex);
            return m_preparser.get();
        }

    private:
        vlc::threads::mutex m_mutex;
        vlc_object_t *m_obj;
        const struct vlc_preparser_cfg m_cfg;
        std::unique_ptr<vlc_preparser_t, void(*)(vlc_preparser_t*)> m_preparser;
};

#endif
