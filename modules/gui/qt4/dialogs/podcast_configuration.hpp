/*****************************************************************************
 * podcast_configuration.hpp: Podcast configuration dialog
 ****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
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

#ifndef _PODCAST_CONFIGURATION_DIALOG_H_
#define _PODCAST_CONFIGURATION_DIALOG_H_

#include "qt4.hpp"
#include "ui/podcast_configuration.h"

class PodcastConfigurationDialog : public QDialog
{
    Q_OBJECT;

public:
    PodcastConfigurationDialog( intf_thread_t *p_intf );

private:
    Ui::PodcastConfiguration ui;
    intf_thread_t *p_intf;

private slots:
    void accept();
    void add();
    void remove();
};

#endif
