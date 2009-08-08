/*****************************************************************************
 * VLC backend for the Phonon library                                        *
 * Copyright (C) 2007-2008 Tanguy Krotoff <tkrotoff@gmail.com>               *
 * Copyright (C) 2008 Lukas Durfina <lukas.durfina@gmail.com>                *
 * Copyright (C) 2009 Fathi Boudra <fabo@kde.org>                            *
 *                                                                           *
 * This program is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU Lesser General Public                *
 * License as published by the Free Software Foundation; either              *
 * version 3 of the License, or (at your option) any later version.          *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Lesser General Public License for more details.                           *
 *                                                                           *
 * You should have received a copy of the GNU Lesser General Public          *
 * License along with this package; if not, write to the Free Software       *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA *
 *****************************************************************************/

#include "effectmanager.h"
#include "backend.h"

namespace Phonon
{
namespace VLC {

EffectInfo::EffectInfo(const QString &name, const QString &description, const QString &author, int filter, Type type)
        : m_name(name)
        , m_description(description)
        , m_author(author)
        , m_filter(filter)
        , m_type(type) {}

EffectManager::EffectManager(Backend *backend)
        : QObject(backend)
        , m_backend(backend)
{
//    m_equalizerEnabled = false;

    // Audio effects - equalizer
    // Only one of them can be used => last set
    // It is clever used with combobox

//    audioEffectList.append(new EffectInfo("(Audio) Equalizer", EQUALIZER, EffectInfo::AudioEffect));

//    audioEffectList.append(new EffectInfo("(Audio) Headphone spalization",
//                                          HEADPHONE_SPALIZATION, EffectInfo::AudioEffect,
//                                          "This effect gives you the feeling that you are standing in a room "
//                                          "with a complete 7.1 speaker set when using only a headphone, "
//                                          "providing a more realistic sound experience. It should also be "
//                                          "more comfortable and less tiring when listening to music for "
//                                          "long periods of time.\nIt works with any source format from mono "
//                                          "to 7.1."));

//    audioEffectList.append(new EffectInfo("(Audio) Parametric equalizer",
//                                          PARAMETRIC_EQUALIZER, EffectInfo::AudioEffect));

//    audioEffectList.append(new EffectInfo("(Audio) Scaletempo",
//                                          SCALETEMPO, EffectInfo::AudioEffect,
//                                          "Scale audio tempo in sync with playback rate"));

//    audioEffectList.append(new EffectInfo("(Audio) Spatializer",
//                                          SPATIALIZER, EffectInfo::AudioEffect));

//    audioEffectList.append(new EffectInfo("(Audio) Volume normalizer",
//                                          VOLUME_NORMALIZER, EffectInfo::AudioEffect));

//    audioEffectList.append(new EffectInfo("(Audio) Flat", FLAT, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Classical", CLASSICAL, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Club", CLUB, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Dance", DANCE, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Fullbass", FULLBASS, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Fullbasstreble", FULLBASSTREBLE, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Fulltreble", FULLTREBLE, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Headphones", HEADPHONES, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Large hall", LARGEHALL, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Live", LIVE, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Party", PARTY, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Pop", POP, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Reggae", REGGAE, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Rock", ROCK, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Ska", SKA, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Soft", SOFT, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Softrock", SOFTROCK, EffectInfo::AudioEffect));
//    audioEffectList.append(new EffectInfo("(Audio) Techno", TECHNO, EffectInfo::AudioEffect));

    // Video effects
    // More than one can be used simultaneously
    // It is clever used with checkbox

//    videoEffectList.append(new EffectInfo("(Video) Atmo light", ATMOLIGHT, EffectInfo::VideoEffect,
//                                          "AtmoLight Filter - "
//                                          "This module allows to control an so called AtmoLight device "
//                                          "connected to your computer.\n"
//                                          "AtmoLight is the homegrown version of what Philips calls AmbiLight.\n"
//                                          "If you need further information feel free to visit us at\n\n"
//                                          "http://www.vdr-wiki.de/wiki/index.php/Atmo-plugin\n "
//                                          "http://www.vdr-wiki.de/wiki/index.php/AtmoWin\n\n"
//                                          "You can find there detailed descriptions on how to build it for yourself "
//                                          "and where to get the required parts.\n"
//                                          "You can also have a look at pictures and some movies showing such a device "
//                                          "in live action.",
//                                          "André Weber (WeberAndre@gmx.de)"));

//    videoEffectList.append(new EffectInfo("(Video) Bluescreen", BLUESCREEN, EffectInfo::VideoEffect,
//                                          "Bluescreen video filter", "Antoine Cellerier <dionoea at videolan tod org>"));

//    videoEffectList.append(new EffectInfo("(Video) Color threshold", COLORTHRESHOLD, EffectInfo::VideoEffect,
//                                          "Color threshold filter", "Sigmund Augdal <dnumgis@videolan.org>"));

//    videoEffectList.append(new EffectInfo("(Video) Deinterlace", DEINTERLACE, EffectInfo::VideoEffect,
//                                          "Deinterlacing video filter", "Sam Hocevar <sam@zoy.org"));

//    videoEffectList.append(new EffectInfo("(Video) Erase", ERASE, EffectInfo::VideoEffect,
//                                          "Erase video filter", "Antoine Cellerier <dionoea at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Extract", EXTRACT, EffectInfo::VideoEffect,
//                                          "Extract RGB component video filter",
//                                          "Antoine Cellerier <dionoea at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Gaussian blur", GAUSSIAN_BLUR, EffectInfo::VideoEffect,
//                                          "Erase video filter", "Antoine Cellerier <dionoea at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Gradient", GRADIENT, EffectInfo::VideoEffect,
//                                          "Gradient video filter",
//                                          "Samuel Hocevar <sam@zoy.org>, "
//                                          "Antoine Cellerier <dionoea at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Grain", GRAIN, EffectInfo::VideoEffect,
//                                          "Grain video filter", "Antoine Cellerier <dionoea at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Invert", INVERT, EffectInfo::VideoEffect,
//                                          "Invert video filter - color inversion",
//                                          "Samuel Hocevar <sam@zoy.org>"));

//    videoEffectList.append(new EffectInfo("(Video) Motion blur", MOTIONBLUR, EffectInfo::VideoEffect,
//                                          "Motion blur filter",
//                                          "Sigmund Augdal Helberg <dnumgis@videolan.org>, "
//                                          "Antoine Cellerier <dionoea at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Motion detect", MOTIONDETECT, EffectInfo::VideoEffect,
//                                          "Motion detect video filter",
//                                          "Antoine Cellerier <dionoea at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Noise", NOISE, EffectInfo::VideoEffect,
//                                          "Noise video filter - add noise to image",
//                                          "Antoine Cellerier <dionoea at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Postprocess", POSTPROCESS, EffectInfo::VideoEffect,
//                                          "Video post processing filter",
//                                          "Laurent Aimar <fenrir@via.ecp.fr>, "
//                                          "Gildas Bazin <gbazin@netcourrier.com>, "
//                                          "Antoine Cellerier <dionoea at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Psychedelic", PSYCHEDELIC, EffectInfo::VideoEffect,
//                                          "Psychedelic video filter",
//                                          "Samuel Hocevar <sam@zoy.org>, "
//                                          "Antoine Cellerier <dionoea at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Ripple", RIPPLE, EffectInfo::VideoEffect,
//                                          "Ripple video filter",
//                                          "Samuel Hocevar <sam@zoy.org>, "
//                                          "Antoine Cellerier <dionoea at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Rotate", ROTATE, EffectInfo::VideoEffect,
//                                          "Rotate video filter",
//                                          "Antoine Cellerier <dionoea at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Seam carving", SEAM_CARVING, EffectInfo::VideoEffect,
//                                          "Seam Carving for Content-Aware Image Resizing",
//                                          "Antoine Cellerier <dionoea at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Sharpen", SHARPEN, EffectInfo::VideoEffect,
//                                          "Sharpen video filter - Augment contrast between contours.",
//                                          "Jérémy DEMEULE <dj_mulder at djduron dot no-ip dot org>, "
//                                          "Jean-Baptiste Kempf <jb at videolan dot org>"));

//    videoEffectList.append(new EffectInfo("(Video) Wave", WAVE, EffectInfo::VideoEffect,
//                                          "Wave video filter",
//                                          "Samuel Hocevar <sam@zoy.org>, "
//                                          "Antoine Cellerier <dionoea at videolan dot org>"));

    updateEffects();
}

EffectManager::~EffectManager()
{
    qDeleteAll(m_audioEffectList);
    m_audioEffectList.clear();
    qDeleteAll(m_videoEffectList);
    m_videoEffectList.clear();
    qDeleteAll(m_effectList);
    m_effectList.clear();
}

/**
 * Returns a list of available audio effects
 */
const QList<EffectInfo *> EffectManager::audioEffects() const
{
    return m_audioEffectList;
}

/**
 * Returns a list of available video effects
 */
const QList<EffectInfo *> EffectManager::videoEffects() const
{
    return m_videoEffectList;
}

/**
 * Returns a list of available effects
 */
const QList<EffectInfo *> EffectManager::effects() const
{
    return m_effectList;
}

void EffectManager::updateEffects()
{
    m_effectList.clear();
    m_effectList += m_audioEffectList;
    m_effectList += m_videoEffectList;
}

}
} // namespace Phonon::VLC
