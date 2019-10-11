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

#define MAIN_TB1_DEFAULT "20;21;65;17;3;0-2;1-2;4;18;65;33;7"
#define MINI_TB_DEFAULT "17;3;0;1;4;18"

PlayerControlBarModel::PlayerControlBarModel(QObject *_parent) : QAbstractListModel(_parent)
{
    configName = "MainPlayerToolbar";
    defaultConfig = MAIN_TB1_DEFAULT;
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
        if(it.size != WIDGET_NORMAL)
            config += "-" + QString::number(it.size);
        config += ";";
    }
    return config;
}

void PlayerControlBarModel::reloadConfig(QString config)
{
    beginResetModel();
    mButtons.clear();
    parseAndAdd(config);
    endResetModel();
}

void PlayerControlBarModel::reloadModel()
{
    beginResetModel();
    mButtons.clear();
    QString config = getSettings() ->value( configName, defaultConfig )
                                            .toString();
    parseAndAdd(config);
    endResetModel();
}

void PlayerControlBarModel::parseAndAdd(QString &config)
{
    beginInsertRows(QModelIndex(),rowCount(),rowCount()+config.split(";", QString::SkipEmptyParts).length() - 1);

    for (const QString& iconPropertyTxt : config.split( ";", QString::SkipEmptyParts ) )
    {
        QStringList list2 = iconPropertyTxt.split( "-" );

        if( list2.count() < 1 )
        {
            msg_Warn( p_intf, "Parsing error 1. Please, report this." );
            continue;
        }
        bool ok;
        int i_option = WIDGET_NORMAL;
        ButtonType_e i_type = static_cast<ButtonType_e>(list2.at( 0 ).toInt( &ok ));
        if( !ok )
        {
            msg_Warn( p_intf, "Parsing error 2. Please, report this." );
            continue;
        }

        if( list2.count() > 1 )
        {
            i_option = list2.at( 1 ).toInt( &ok );
            if( !ok )
            {
                msg_Warn( p_intf, "Parsing error 3. Please, report this." );
                continue;
            }
        }

        mButtons.append({ i_type , i_option});
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

    case SIZE_ROLE:
        return QVariant(button.size);
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
    case SIZE_ROLE:
        button.size = value.toInt();
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
    names[SIZE_ROLE] = "size";

    return names;
}
bool PlayerControlBarModel::setButtonAt(int index, const IconToolButton &button)
{
    if(index < 0 || index >= mButtons.size())
        return false;
    const IconToolButton &oldButton = mButtons.at(index);

    if (button.size == oldButton.size && button.id == oldButton.id)
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
    if(configName == "MainPlayerToolbar")
        defaultConfig = MAIN_TB1_DEFAULT;
    else
        defaultConfig = MINI_TB_DEFAULT;
    if  (m_mainCtx)
        reloadModel();
    emit configNameChanged(name);
}

void PlayerControlBarModel::insert(int index, QVariantMap bdata)
{
    beginInsertRows(QModelIndex(),index,index);
    mButtons.insert(index,{bdata.value("id").toInt(),bdata.value("size").toInt()});
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

