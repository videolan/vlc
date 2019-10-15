/*****************************************************************************
 * chromaprint.hpp: Fingerprinter helper class
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
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
#ifndef CHROMAPRINT_HPP
#define CHROMAPRINT_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QObject>
#include <QString>
#include <vlc_fingerprinter.h>
#include <vlc_interface.h>

class Chromaprint : public QObject
{
    Q_OBJECT

public:
    Chromaprint( intf_thread_t *p_intf = NULL );
    virtual ~Chromaprint();
    bool enqueue( input_item_t *p_item );
    static int results_available( vlc_object_t *p_this, const char *,
                                  vlc_value_t, vlc_value_t newval, void *param );
    fingerprint_request_t * fetchResults();
    void apply( fingerprint_request_t *, size_t i_id );
    static bool isSupported( QString uri );

signals:
    void finished();

private:
    void finish() { emit finished(); }
    intf_thread_t *p_intf;
    fingerprinter_thread_t *p_fingerprinter;
};

#endif // CHROMAPRINT_HPP
