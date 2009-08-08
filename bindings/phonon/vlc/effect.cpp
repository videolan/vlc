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

#include "effect.h"

#include "effectmanager.h"

#include "mediaobject.h"

namespace Phonon
{
namespace VLC {

Effect::Effect(EffectManager *p_em, int i_effectId, QObject *p_parent)
        : SinkNode(p_parent)
{
    p_effectManager = p_em;
    QList<EffectInfo *> effects = p_effectManager->effects();

    if (i_effectId >= 0 && i_effectId < effects.size()) {
        i_effect_filter = effects[ i_effectId ]->filter();
        effect_type = effects[ i_effectId ]->type();
        setupEffectParams();
    } else {
        // effect ID out of range
        Q_ASSERT(0);
    }
}

Effect::~Effect()
{
    parameterList.clear();
}

void Effect::connectToMediaObject(PrivateMediaObject *p_media_object)
{
    SinkNode::connectToMediaObject(p_media_object);

    switch (effect_type) {
    case EffectInfo::AudioEffect:
//        libvlc_audio_filter_add(p_vlc_instance, (libvlc_audio_filter_names_t)i_effect_filter, vlc_exception);
//        vlcExceptionRaised();
        break;
    case EffectInfo::VideoEffect:
//        libvlc_video_filter_add(p_vlc_current_media_player, (libvlc_video_filter_names_t)i_effect_filter, vlc_exception);
//        vlcExceptionRaised();
        break;
    }
}

void Effect::disconnectFromMediaObject(PrivateMediaObject *p_media_object)
{
    SinkNode::disconnectFromMediaObject(p_media_object);

    switch (effect_type) {
    case EffectInfo::AudioEffect:
//        libvlc_audio_filter_remove(p_vlc_instance, (libvlc_audio_filter_names_t)i_effect_filter, vlc_exception);
//        vlcExceptionRaised();
        break;
    case EffectInfo::VideoEffect:
//        libvlc_video_filter_remove(p_vlc_current_media_player, (libvlc_video_filter_names_t)i_effect_filter, vlc_exception);
//        vlcExceptionRaised();
        break;
    }
}

void Effect::setupEffectParams()
{
//    libvlc_filter_parameter_list_t *p_list;
    switch (effect_type) {
    case EffectInfo::AudioEffect:
//        p_list = libvlc_audio_filter_get_parameters(p_vlc_instance, (libvlc_audio_filter_names_t)i_effect_filter, vlc_exception );
//        vlcExceptionRaised();
        break;
    case EffectInfo::VideoEffect:
//        p_list = libvlc_video_filter_get_parameters(p_vlc_instance, (libvlc_video_filter_names_t)i_effect_filter, vlc_exception );
//        vlcExceptionRaised();
        break;
    }
//    if( !p_list )
//        return;

    int i_index = 0;
//    libvlc_filter_parameter_list_t *p_parameter_list = p_list;
//    while (p_parameter_list) {
//        switch (p_parameter_list->var_type) {
//        case LIBVLC_BOOL: {
//            const QString description = p_parameter_list->psz_description;
//            parameterList.append(Phonon::EffectParameter(
//                                     i_index,
//                                     QString(p_parameter_list->psz_parameter_name),
//                                     Phonon::EffectParameter::ToggledHint,   // hints
//                                     QVariant((bool) p_parameter_list->default_value.b_bool),
//                                     QVariant((bool) false),
//                                     QVariant((bool) true),
//                                     QVariantList(),
//                                     description));
//            break;
//        }
//        case LIBVLC_INT: {
//            const QString description = p_parameter_list->psz_description;
//            parameterList.append(Phonon::EffectParameter(
//                                     i_index,
//                                     QString(p_parameter_list->psz_parameter_name),
//                                     EffectParameter::IntegerHint,   // hints
//                                     QVariant((int) p_parameter_list->default_value.i_int),
//                                     QVariant((int) p_parameter_list->min_value.i_int),
//                                     QVariant((int) p_parameter_list->max_value.i_int),
//                                     QVariantList(),
//                                     description));
//            break;
//        }
//        case LIBVLC_FLOAT: {
//            const QString description = p_parameter_list->psz_description;
//            parameterList.append(Phonon::EffectParameter(
//                                     i_index,
//                                     QString(p_parameter_list->psz_parameter_name),
//                                     0,   // hints
//                                     QVariant((double) p_parameter_list->default_value.f_float),
//                                     QVariant((double) p_parameter_list->min_value.f_float),
//                                     QVariant((double) p_parameter_list->max_value.f_float),
//                                     QVariantList(),
//                                     description));
//            break;
//        }
//        case LIBVLC_STRING: {
//            const QString description = p_parameter_list->psz_description;
//            parameterList.append(Phonon::EffectParameter(
//                                     i_index,
//                                     QString(p_parameter_list->psz_parameter_name),
//                                     0,   // hints
//                                     QVariant((const char *) p_parameter_list->default_value.psz_string),
//                                     NULL,
//                                     NULL,
//                                     QVariantList(),
//                                     description));
//            break;
//        }
//        }
//        i_index++;
//        p_parameter_list = p_parameter_list->p_next;
//    }
//    libvlc_filter_parameters_release(p_list);
}

QList<EffectParameter> Effect::parameters() const
{
    return parameterList;
}

QVariant Effect::parameterValue(const EffectParameter & param) const
{
    return QVariant();
}

void Effect::setParameterValue(const EffectParameter & param, const QVariant & newValue)
{
//    libvlc_value_t value;
//    libvlc_var_type_t type;
//    switch (param.type()) {
//    case QVariant::Bool:
//        value.b_bool = newValue.toBool();
//        type = LIBVLC_BOOL;
//        break;
//    case QVariant::Int:
//        value.i_int = newValue.toInt();
//        type = LIBVLC_INT;
//        break;
//    case QVariant::Double:
//        value.f_float = (float) newValue.toDouble();
//        type = LIBVLC_FLOAT;
//        break;
//    case QVariant::String:
//        value.psz_string = newValue.toString().toAscii().data();
//        type = LIBVLC_STRING;
//        break;
//    default:
//        break;
//    }
//    switch (effect_type) {
//    case EffectInfo::AudioEffect:
//        libvlc_audio_filter_set_parameter(
//            p_vlc_instance,
//            // (libvlc_audio_filter_names_t) i_effect_filter,
//            param.name().toAscii().data(),
//            type,
//            value,
//            vlc_exception);
//        vlcExceptionRaised();
//        break;
//    case EffectInfo::VideoEffect:
//        libvlc_video_filter_set_parameter(
//            p_vlc_current_media_player,
//            (libvlc_video_filter_names_t) i_effect_filter,
//            param.name().toAscii().data(),
//            type,
//            value,
//            vlc_exception);
//        vlcExceptionRaised();
//        break;
//    }
}

}
} // Namespace Phonon::VLC
