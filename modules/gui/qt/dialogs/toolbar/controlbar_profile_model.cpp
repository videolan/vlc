/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "controlbar_profile_model.hpp"

#include <QSettings>

#include "qt.hpp"
#include "controlbar_profile.hpp"
#include "player/control_list_model.hpp"

#define SETTINGS_KEY_SELECTEDPROFILE "SelectedProfile"
#define SETTINGS_ARRAYNAME_PROFILES "Profiles"
#define SETTINGS_KEY_NAME "Name"
#define SETTINGS_KEY_MODEL "Model"

#define SETTINGS_CONTROL_SEPARATOR ","
#define SETTINGS_CONFIGURATION_SEPARATOR "|"
#define SETTINGS_PROFILE_SEPARATOR "$"

decltype (ControlbarProfileModel::m_defaults)
    ControlbarProfileModel::m_defaults =
        {
            {
                {
                    "Minimalist Style"
                },
                {
                    {
                        {
                            "MainPlayer"
                        },
                        {
                            {
                                {
                                    ControlListModel::PLAY_BUTTON,
                                    ControlListModel::WIDGET_SPACER,
                                    ControlListModel::PREVIOUS_BUTTON,
                                    ControlListModel::STOP_BUTTON,
                                    ControlListModel::NEXT_BUTTON,
                                    ControlListModel::WIDGET_SPACER,
                                    ControlListModel::RECORD_BUTTON,
                                    ControlListModel::WIDGET_SPACER,
                                    ControlListModel::TELETEXT_BUTTONS,
                                    ControlListModel::WIDGET_SPACER,
                                    ControlListModel::PLAYLIST_BUTTON,
                                    ControlListModel::WIDGET_SPACER,
                                    ControlListModel::VOLUME
                                },
                                {

                                },
                                {

                                }
                            }
                        }
                    },
                    {
                        {
                            "MiniPlayer"
                        },
                        {
                            {
                                {
                                    ControlListModel::PREVIOUS_BUTTON,
                                    ControlListModel::PLAY_BUTTON,
                                    ControlListModel::STOP_BUTTON,
                                    ControlListModel::NEXT_BUTTON
                                },
                                {

                                },
                                {

                                }
                            }
                        }
                    }
                }
            },
            {
                {
                    "One-liner Style"
                },
                {
                    {
                        {
                            "MainPlayer"
                        },
                        {
                            {
                                {
                                    ControlListModel::PLAY_BUTTON,
                                    ControlListModel::WIDGET_SPACER,
                                    ControlListModel::PREVIOUS_BUTTON,
                                    ControlListModel::STOP_BUTTON,
                                    ControlListModel::NEXT_BUTTON,
                                    ControlListModel::WIDGET_SPACER,
                                    ControlListModel::FULLSCREEN_BUTTON,
                                    ControlListModel::PLAYLIST_BUTTON,
                                    ControlListModel::EXTENDED_BUTTON,
                                    ControlListModel::WIDGET_SPACER,
                                    ControlListModel::WIDGET_SPACER,
                                    ControlListModel::RECORD_BUTTON,
                                    ControlListModel::SNAPSHOT_BUTTON,
                                    ControlListModel::ATOB_BUTTON,
                                    ControlListModel::FRAME_BUTTON
                                },
                                {
                                    ControlListModel::VOLUME
                                },
                                {

                                }
                            }
                        }
                    },
                    {
                        {
                            "MiniPlayer"
                        },
                        {
                            {
                                {
                                    ControlListModel::RANDOM_BUTTON,
                                    ControlListModel::PREVIOUS_BUTTON,
                                    ControlListModel::PLAY_BUTTON,
                                    ControlListModel::STOP_BUTTON,
                                    ControlListModel::NEXT_BUTTON,
                                    ControlListModel::LOOP_BUTTON
                                },
                                {

                                },
                                {

                                }
                            }
                        }
                    }
                }
            },
            {
                {
                    "Simplest Style"
                },
                {
                    {
                        {
                            "MainPlayer"
                        },
                        {
                            {
                                {
                                    ControlListModel::VOLUME
                                },
                                {
                                    ControlListModel::PLAY_BUTTON,
                                    ControlListModel::NEXT_BUTTON,
                                    ControlListModel::STOP_BUTTON
                                },
                                {
                                    ControlListModel::FULLSCREEN_BUTTON
                                }
                            }
                        }
                    },
                    {
                        {
                            "MiniPlayer"
                        },
                        {
                            {
                                {
                                    ControlListModel::PREVIOUS_BUTTON,
                                    ControlListModel::PLAY_BUTTON,
                                    ControlListModel::NEXT_BUTTON
                                },
                                {

                                },
                                {

                                }
                            }
                        }
                    }
                }
            }
        };


ControlbarProfileModel::ControlbarProfileModel(intf_thread_t *p_intf, QObject *parent)
    : QAbstractListModel(parent),
    m_intf(p_intf)
{
    assert(m_intf);

    connect(this, &QAbstractListModel::rowsInserted, this, &ControlbarProfileModel::countChanged);
    connect(this, &QAbstractListModel::rowsRemoved, this, &ControlbarProfileModel::countChanged);
    connect(this, &QAbstractListModel::modelReset, this, &ControlbarProfileModel::countChanged);

    // To make the QML player controlbars update when model is Reset
    connect(this, &QAbstractListModel::modelReset, this, &ControlbarProfileModel::selectedProfileChanged);

    // When all profiles are removed, insert defaults:
    // Maybe add a dedicate button for this purpose and don't allow removing all profiles ?
    connect(this, &ControlbarProfileModel::countChanged, this, [this] () {
        if (rowCount() == 0)
            insertDefaults();
    });

    if (reload() == false)
    {
        // If initial reload fails, load the default profiles:
        insertDefaults();
    }
}

void ControlbarProfileModel::insertDefaults()
{
    // First, add a blank new profile:
    // ControlbarProfile will inject the default configurations during its construction.
    newProfile(tr("Default Profile"));

    // Add default profiles:
    for (const auto& i : m_defaults)
    {
        const auto ptrNewProfile = newProfile(i.name);
        if (!ptrNewProfile)
            continue;

        ptrNewProfile->injectModel(i.modelData);
        ptrNewProfile->resetDirty(); // default profiles should not be dirty initially
    }

    setSelectedProfile(0);
}

QString ControlbarProfileModel::generateUniqueName(const QString &name)
{
    const auto sameNameCount = std::count_if(m_profiles.begin(),
                                             m_profiles.end(),
                                             [name](const ControlbarProfile* i) {
                                                 return i->name() == name;
                                             });

    if (sameNameCount > 0)
        return QString("%1 (%2)").arg(name).arg(sameNameCount + 1);
    else
        return name;
}

int ControlbarProfileModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_profiles.size();
}

QVariant ControlbarProfileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const auto ptrProfile = m_profiles.at(index.row());

    if (!ptrProfile)
        return QVariant();

    switch (role)
    {
    case Qt::DisplayRole:
        return ptrProfile->name();
    case MODEL_ROLE:
        return QVariant::fromValue(ptrProfile);
    }

    return QVariant();
}

QHash<int, QByteArray> ControlbarProfileModel::roleNames() const
{
    return {
        {
            Qt::DisplayRole, "name"
        },
        {
            MODEL_ROLE, "model"
        }
    };
}

bool ControlbarProfileModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (row < 0 || row > m_profiles.size())
        return false;

    beginInsertRows(parent, row, row + count - 1);

    for (int i = 0; i < count; ++i)
    {
        const auto profile = new ControlbarProfile(this);
        profile->setName(tr("Profile %1").arg(m_profiles.size()));

        m_profiles.insert(row, profile);
    }

    endInsertRows();

    return true;
}

bool ControlbarProfileModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (row < 0 || count < 1 || row + count > m_profiles.size())
        return false;

    beginRemoveRows(parent, row, row + count - 1);

    auto from = m_profiles.begin() + row;
    auto to = from + count - 1;
    std::for_each(from, to, [](auto* item) {
        assert(item);
        item->deleteLater();
    });
    m_profiles.erase(from, to);

    endRemoveRows();

    return true;
}

bool ControlbarProfileModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (data(index, role) != value)
    {
        auto ptrProfile = m_profiles.at(index.row());

        if (!ptrProfile)
            return false;

        switch (role)
        {
        case Qt::DisplayRole:
            if (value.canConvert(QVariant::String))
                ptrProfile->setName(value.toString());
            else
                return false;
            break;
        case MODEL_ROLE:
            if (value.canConvert<ControlbarProfile*>())
                ptrProfile = qvariant_cast<ControlbarProfile*>(value);
            else
                return false;
            break;
        default:
            return false;
        }

        m_profiles.replace(index.row(), ptrProfile);

        emit dataChanged(index, index, { role });
        return true;
    }
    return false;
}

Qt::ItemFlags ControlbarProfileModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return (Qt::ItemIsEditable | Qt::ItemNeverHasChildren);
}

int ControlbarProfileModel::selectedProfile() const
{
    return m_selectedProfile;
}

ControlbarProfile* ControlbarProfileModel::currentModel() const
{
    return getProfile(selectedProfile());
}

void ControlbarProfileModel::save(bool clearDirty) const
{
    assert(m_intf->p_sys);
    assert(m_intf->p_sys->mainSettings);

    if (!m_intf || !m_intf->p_sys || !m_intf->p_sys->mainSettings)
        return;

    const auto settings = m_intf->p_sys->mainSettings;
    const auto groupName = metaObject()->className();

    settings->beginGroup(groupName);
    settings->remove(""); // clear the group before save

    settings->setValue(SETTINGS_KEY_SELECTEDPROFILE, selectedProfile());

    settings->beginWriteArray(SETTINGS_ARRAYNAME_PROFILES);

    for (int i = 0; i < m_profiles.size(); ++i)
    {
        settings->setArrayIndex(i);

        const auto& ptrModelMap = m_profiles.at(i)->m_models;

        QString val;
        for (auto it = ptrModelMap.constBegin(); it != ptrModelMap.end(); ++it)
        {
            const QString identifier = it.key();

            const auto serializedModels = m_profiles.at(i)->getModelData(identifier);

            static const auto join = [](const QVector<int>& list) {
                QString ret;
                for (auto i : list)
                {
                    ret += QString::number(i) + SETTINGS_CONTROL_SEPARATOR;
                }
                if (!ret.isEmpty())
                    ret.chop(1);
                return ret;
            };

            val += QString(SETTINGS_PROFILE_SEPARATOR
                           "%1"
                           SETTINGS_CONFIGURATION_SEPARATOR
                           "%2"
                           SETTINGS_CONFIGURATION_SEPARATOR
                           "%3"
                           SETTINGS_CONFIGURATION_SEPARATOR
                           "%4").arg(identifier,
                                     join(serializedModels[0]),
                                     join(serializedModels[1]),
                                     join(serializedModels[2]));
        }

        if (clearDirty)
            m_profiles.at(i)->resetDirty();

        settings->setValue(SETTINGS_KEY_NAME, m_profiles.at(i)->name());
        settings->setValue(SETTINGS_KEY_MODEL, val);
    }

    settings->endArray();
    settings->endGroup();
}

bool ControlbarProfileModel::reload()
{
    assert(m_intf->p_sys);
    assert(m_intf->p_sys->mainSettings);

    if (!m_intf || !m_intf->p_sys || !m_intf->p_sys->mainSettings)
        return false;

    const auto settings = m_intf->p_sys->mainSettings;
    const auto groupName = metaObject()->className();

    settings->beginGroup(groupName);

    const int size = settings->beginReadArray(SETTINGS_ARRAYNAME_PROFILES);

    if (size <= 0)
    {
        settings->endArray();
        settings->endGroup();

        return false;
    }

    beginResetModel();

    decltype (m_profiles) profiles;
    for (int i = 0; i < size; ++i)
    {
        settings->setArrayIndex(i);

        const QString modelValue = settings->value(SETTINGS_KEY_MODEL).toString();
        if (modelValue.isEmpty())
            continue;

        const auto val = modelValue.splitRef(SETTINGS_PROFILE_SEPARATOR);
        if (val.isEmpty())
            continue;

        const auto ptrNewProfile = new ControlbarProfile(this);
        ptrNewProfile->setName(settings->value(SETTINGS_KEY_NAME).toString());

        for (auto j : val)
        {
            if (j.isEmpty())
                continue;

            const auto alignments = j.split(SETTINGS_CONFIGURATION_SEPARATOR);

            if (alignments.length() != 4)
                continue;

            if (alignments[0].toString().isEmpty())
                continue;

            static const auto split = [](auto ref) {
                QVector<int> list;

                if (ref.isEmpty())
                    return list;

                for (auto i : ref.split(SETTINGS_CONTROL_SEPARATOR))
                {
                    bool ok = false;
                    int k = i.toInt(&ok);

                    if (ok)
                        list.append(k);
                }
                return list;
            };

            const std::array<QVector<int>, 3> data { split(alignments[1]),
                                                     split(alignments[2]),
                                                     split(alignments[3]) };

            ptrNewProfile->setModelData(alignments[0].toString(), data);
            ptrNewProfile->resetDirty(); // Newly loaded model can not be dirty
        }

        profiles.append(ptrNewProfile);
    }

    settings->endArray();

    m_selectedProfile = -1;
    std::for_each(m_profiles.begin(), m_profiles.end(), [](auto i) { delete i; });

    m_profiles = std::move(profiles);

    endResetModel();

    bool ok = false;
    int index = settings->value(SETTINGS_KEY_SELECTEDPROFILE).toInt(&ok);

    if (ok)
        setSelectedProfile(index);
    else
        setSelectedProfile(0);

    settings->endGroup();

    return true;
}

bool ControlbarProfileModel::setSelectedProfile(int selectedProfile)
{
    if (m_selectedProfile == selectedProfile)
        return false;

    const auto ptrProfileNew = getProfile(selectedProfile);
    const auto ptrProfileOld = getProfile(m_selectedProfile);

    assert(ptrProfileNew);

    if (!ptrProfileNew)
        return false;

    connect(ptrProfileNew, &ControlbarProfile::controlListChanged, this, &ControlbarProfileModel::selectedProfileControlListChanged);
    connect(this, &QAbstractListModel::modelReset, ptrProfileNew, &ControlbarProfile::generateLinearControlList);
    connect(this, &ControlbarProfileModel::selectedProfileChanged, ptrProfileNew, &ControlbarProfile::generateLinearControlList);

    if (ptrProfileOld && (ptrProfileNew != ptrProfileOld))
    {
        disconnect(ptrProfileOld, &ControlbarProfile::controlListChanged, this, &ControlbarProfileModel::selectedProfileControlListChanged);
        disconnect(this, &QAbstractListModel::modelReset, ptrProfileOld, &ControlbarProfile::generateLinearControlList);
        disconnect(this, &ControlbarProfileModel::selectedProfileChanged, ptrProfileOld, &ControlbarProfile::generateLinearControlList);
    }

    m_selectedProfile = selectedProfile;

    emit selectedProfileChanged();

    return true;
}

ControlbarProfile *ControlbarProfileModel::getProfile(int index) const
{
    if (index < 0 || index >= m_profiles.size())
        return nullptr;

    return m_profiles.at(index);
}

ControlbarProfile *ControlbarProfileModel::newProfile(const QString &name)
{
    if (name.isEmpty())
        return nullptr;

    const auto ptrProfile = newProfile();

    ptrProfile->setName(generateUniqueName(name));

    return ptrProfile;
}

ControlbarProfile *ControlbarProfileModel::newProfile()
{
    const auto ptrNewProfile = new ControlbarProfile(this);

    beginInsertRows(QModelIndex(), m_profiles.size(), m_profiles.size());

    m_profiles.append(ptrNewProfile);

    endInsertRows();

    return ptrNewProfile;
}

ControlbarProfile *ControlbarProfileModel::cloneProfile(const ControlbarProfile *profile)
{
    const auto ptrNewProfile = newProfile(profile->name());

    if (!ptrNewProfile)
        return nullptr;

    for (auto it = profile->m_models.constBegin(); it != profile->m_models.constEnd(); ++it)
    {
        ptrNewProfile->setModelData(it.key(), profile->getModelData(it.key()));
        ptrNewProfile->resetDirty();
    }

    return ptrNewProfile;
}

void ControlbarProfileModel::cloneSelectedProfile(const QString &newProfileName)
{
    const auto ptrModel = currentModel();

    assert(ptrModel);
    if (!ptrModel)
        return;

    const auto ptrNewModel = cloneProfile(ptrModel);

    assert(ptrNewModel);
    if (!ptrNewModel)
        return;

    ptrNewModel->setName(generateUniqueName(newProfileName));
}

void ControlbarProfileModel::deleteSelectedProfile()
{
    const auto ptrSelectedProfile = getProfile(m_selectedProfile);

    if (!ptrSelectedProfile)
        return;

    const auto _selectedProfile = m_selectedProfile;

    beginRemoveRows(QModelIndex(), _selectedProfile, _selectedProfile);

    m_selectedProfile = -1;

    delete ptrSelectedProfile;
    m_profiles.removeAt(_selectedProfile);

    endRemoveRows();

    if (getProfile(_selectedProfile - 1))
        setSelectedProfile(_selectedProfile - 1);
    else
        setSelectedProfile(_selectedProfile);
}
