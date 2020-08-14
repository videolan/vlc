/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
#include <QSettings>

#include "qt.hpp"
#include "playercontrolbarmodel.hpp"

enum default_align {
    LEFT = 0,
    CENTER,
    RIGHT,
    SIZE
};

static const QVector<PlayerControlBarModel::IconToolButton> MAIN_TB_DEFAULT[default_align::SIZE] =
{
    {
        // left
        {PlayerControlBarModel::LANG_BUTTON},
        {PlayerControlBarModel::MENU_BUTTON}
    },
    {
        // center
        {PlayerControlBarModel::RANDOM_BUTTON},
        {PlayerControlBarModel::PREVIOUS_BUTTON},
        {PlayerControlBarModel::PLAY_BUTTON},
        {PlayerControlBarModel::NEXT_BUTTON},
        {PlayerControlBarModel::LOOP_BUTTON}
    },
    {
        // right
        {PlayerControlBarModel::VOLUME},
        {PlayerControlBarModel::FULLSCREEN_BUTTON}
    }
};

static const QVector<PlayerControlBarModel::IconToolButton> MINI_TB_DEFAULT[default_align::SIZE] =
{
    {
        // left
        {PlayerControlBarModel::ARTWORK_INFO}
    },
    {
        // center
        {PlayerControlBarModel::RANDOM_BUTTON},
        {PlayerControlBarModel::PREVIOUS_BUTTON},
        {PlayerControlBarModel::PLAY_BUTTON},
        {PlayerControlBarModel::NEXT_BUTTON},
        {PlayerControlBarModel::LOOP_BUTTON}
    },
    {
        // right
        {PlayerControlBarModel::VOLUME},
        {PlayerControlBarModel::PLAYER_SWITCH_BUTTON}
    }
};


PlayerControlBarModel::PlayerControlBarModel(QObject *_parent) : QAbstractListModel(_parent)
{
    configName = "MainPlayerToolbar";

    connect(this, &QAbstractListModel::rowsInserted, this, &PlayerControlBarModel::countChanged);
    connect(this, &QAbstractListModel::rowsRemoved, this, &PlayerControlBarModel::countChanged);
    connect(this, &QAbstractListModel::modelReset, this, &PlayerControlBarModel::countChanged);
}

void PlayerControlBarModel::saveConfig()
{
    getSettings()->setValue(configName,getConfig());
}

QString PlayerControlBarModel::getConfig()
{
    QString config="";
    for (IconToolButton it: mButtons) {
        config += QString::number(it.id);
        config += ";";
    }
    return config;
}

void PlayerControlBarModel::reloadConfig(QString config)
{
    beginResetModel();
    mButtons.clear();
    if (!config.isEmpty())
        parseAndAdd(config);
    endResetModel();
}

void PlayerControlBarModel::reloadModel()
{
    beginResetModel();
    mButtons.clear();

    QVariant config = getSettings() ->value(configName);

    if (!config.isNull() && config.canConvert<QString>())
        parseAndAdd(config.toString());
    else
    {
        QString alignment = configName.split("-").at(1);
        if (configName.startsWith("MainPlayerToolbar"))
        {
            if (alignment == "left")
                parseDefault(MAIN_TB_DEFAULT[default_align::LEFT]);
            else if (alignment == "center")
                parseDefault(MAIN_TB_DEFAULT[default_align::CENTER]);
            else if (alignment == "right")
                parseDefault(MAIN_TB_DEFAULT[default_align::RIGHT]);
        }
        else
        {
            if (alignment == "left")
                parseDefault(MINI_TB_DEFAULT[default_align::LEFT]);
            else if (alignment == "center")
                parseDefault(MINI_TB_DEFAULT[default_align::CENTER]);
            else if (alignment == "right")
                parseDefault(MINI_TB_DEFAULT[default_align::RIGHT]);
        }
    }
    endResetModel();
}

void PlayerControlBarModel::parseDefault(const QVector<PlayerControlBarModel::IconToolButton>& config)
{
    beginInsertRows(QModelIndex(),rowCount(),rowCount() + config.size());
    for (const auto& i : config)
        mButtons.append(i);
    endInsertRows();
}

void PlayerControlBarModel::parseAndAdd(const QString &config)
{
    beginInsertRows(QModelIndex(),rowCount(),rowCount()+config.split(";", QString::SkipEmptyParts).length() - 1);

    for (const QString& iconPropertyTxt : config.split( ";", QString::SkipEmptyParts ) )
    {
        QStringList list2 = iconPropertyTxt.trimmed().split( "-" );

        if( list2.count() < 1 )
        {
            msg_Warn( p_intf, "Parsing error 1. Please, report this." );
            continue;
        }
        bool ok;
        ButtonType_e i_type = static_cast<ButtonType_e>(list2.at( 0 ).toInt( &ok ));
        if( !ok )
        {
            msg_Warn( p_intf, "Parsing error 2. Please, report this." );
            continue;
        }

        IconToolButton itButton = {i_type};
        mButtons.append(itButton);
    }

    endInsertRows();
}

int PlayerControlBarModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() )
        return 0;

    return mButtons.size();
}

QVariant PlayerControlBarModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() )
        return QVariant();

    const IconToolButton button = mButtons.at(index.row());

    switch (role) {
    case ID_ROLE:
        return QVariant(button.id);
    }
    return QVariant();
}

bool PlayerControlBarModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    IconToolButton button = mButtons.at(index.row());
    switch (role) {
    case ID_ROLE:
        button.id = value.toInt();
        break;
    }

    if (setButtonAt(index.row(),button)) {
        emit dataChanged(index, index, QVector<int>() << role);
        return true;
    }
    return false;
}

Qt::ItemFlags PlayerControlBarModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsEditable;
}

QHash<int, QByteArray> PlayerControlBarModel::roleNames() const
{
    QHash<int, QByteArray> names;

    names[ID_ROLE] = "id";

    return names;
}
bool PlayerControlBarModel::setButtonAt(int index, const IconToolButton &button)
{
    if(index < 0 || index >= mButtons.size())
        return false;
    const IconToolButton &oldButton = mButtons.at(index);

    if (button.id == oldButton.id)
        return false;

    mButtons[index] = button;
    return true;
}

void PlayerControlBarModel::setMainCtx(QmlMainContext* ctx)
{
    if(ctx == nullptr && m_mainCtx == ctx)
        return;
    m_mainCtx = ctx;
    p_intf = m_mainCtx->getIntf();
    assert(p_intf != nullptr);
    reloadModel();
    emit ctxChanged(ctx);
}

void PlayerControlBarModel::setConfigName(QString name)
{
    if(configName == name)
        return;
    configName = name;
    if  (m_mainCtx)
        reloadModel();
    emit configNameChanged(name);
}

void PlayerControlBarModel::insert(int index, QVariantMap bdata)
{
    beginInsertRows(QModelIndex(),index,index);
    mButtons.insert(index, { bdata.value("id").toInt() });
    endInsertRows();
}
void PlayerControlBarModel::move(int src, int dest)
{
    if(src == dest) return;
    beginMoveRows(QModelIndex(),src,src,QModelIndex(),dest + (src < dest ? 1:0));
    mButtons.move(src,dest);
    endMoveRows();
}

void PlayerControlBarModel::remove(int index)
{
    beginRemoveRows(QModelIndex(),index,index);
    mButtons.remove(index);
    endRemoveRows();
}

