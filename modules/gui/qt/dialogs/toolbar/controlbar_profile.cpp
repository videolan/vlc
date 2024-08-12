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
#include "controlbar_profile.hpp"

#include <QMetaMethod>

#include "player/control_list_model.hpp"
#include "player/player_controlbar_model.hpp"


ControlbarProfile::ControlbarProfile(QObject *parent) : QObject(parent)
{

}

PlayerControlbarModel *ControlbarProfile::newModel(int identifier)
{
    if (m_models.contains(identifier))
        return nullptr; // can not allow the same identifier

    const auto model = new PlayerControlbarModel(this);

    connect(model, &PlayerControlbarModel::controlListChanged, this, &ControlbarProfile::generateLinearControlList);

    connect(model, &PlayerControlbarModel::dirtyChanged, this, [this](bool dirty) {
        if (dirty)
            ++m_dirty;
        else
            --m_dirty;

        emit dirtyChanged( this->dirty() );
    });

    m_models.insert(identifier, model);

    return model;
}

PlayerControlbarModel *ControlbarProfile::getModel(int identifier) const
{
    if (m_models.contains(identifier))
    {
        return m_models[identifier];
    }
    else
    {
        return nullptr;
    }
}

void ControlbarProfile::setModelData(int identifier, const std::array<QVector<int>, 3> &data)
{
    auto ptrModel = getModel(identifier);

    if (ptrModel)
    {
        ptrModel->loadModels(data);
    }
    else
    {
        ptrModel = newModel(identifier);

        if (!ptrModel)
            return;

        ptrModel->loadModels(data);
    }

    ptrModel->setDirty(true);
}

std::array<QVector<int>, 3> ControlbarProfile::getModelData(int identifier) const
{
    const auto ptrModel = getModel(identifier);

    if (!ptrModel)
        return {};

    return ptrModel->serializeModels();
}

void ControlbarProfile::deleteModel(int identifier)
{
    if (m_models.contains(identifier))
    {
        m_models[identifier]->deleteLater();
        m_models.remove(identifier);
    }
}

void ControlbarProfile::setName(const QString &name)
{
    if (name == m_name)
        return;

    m_name = name;

    emit nameChanged(m_name);
}

bool ControlbarProfile::dirty() const
{
    return (m_dirty > 0);
}

QString ControlbarProfile::name() const
{
    return m_name;
}

bool ControlbarProfile::operator==(const ControlbarProfile &model) const
{
    if (m_models.count() != model.m_models.count())
        return false;

    if (m_models != model.m_models)
    {
        // Deep comparison
        QMapIterator<int, PlayerControlbarModel *> i(m_models);
        while (i.hasNext())
        {
            i.next();

            if (!model.m_models.contains(i.key()))
                return false;

            assert(model.m_models[i.key()]);
            assert(i.value());

            if (*model.m_models[i.key()] != *i.value())
                return false;
        }
    }

    return true;
}

void ControlbarProfile::injectModel(const QVector<ControlbarProfile::Configuration> &modelData)
{
    m_pauseControlListGeneration = true;

    for (const auto& i : modelData)
    {
        setModelData(i.identifier, i.data);
    }

    m_pauseControlListGeneration = false;

    generateLinearControlList();
}

void ControlbarProfile::generateLinearControlList()
{
    if (m_pauseControlListGeneration)
        return;

    // Don't bother if there is no receiver (connection):
    if (! isSignalConnected(QMetaMethod::fromSignal(&ControlbarProfile::controlListChanged)) )
        return;

    QVector<int> linearControls;

    for (const auto& i : m_models)
    {
        linearControls.append(i->serializeModels()[0] + i->serializeModels()[1] + i->serializeModels()[2]);
    }

    emit controlListChanged(linearControls);
}

void ControlbarProfile::resetDirty()
{
    if (dirty() == false)
        return;

    for (auto it = m_models.constBegin(); it != m_models.constEnd(); ++it)
    {
        it.value()->setDirty(false);
    }

    emit dirtyChanged(dirty());
}
