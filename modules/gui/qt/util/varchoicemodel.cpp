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

#include "varchoicemodel.hpp"

int VLCVarChoiceModel::on_variable_callback( vlc_object_t * object, char const * , vlc_value_t oldvalue, vlc_value_t newvalue, void * data)
{
    VLCVarChoiceModel* that = static_cast<VLCVarChoiceModel*>(data);
    return that->updateData(object, oldvalue, newvalue);
}

int VLCVarChoiceModel::on_variable_list_callback( vlc_object_t * object, char const * , int action, vlc_value_t* value, void * data)
{
    VLCVarChoiceModel* that = static_cast<VLCVarChoiceModel*>(data);
    return that->updateList(object, action, value);
}

VLCVarChoiceModel::~VLCVarChoiceModel()
{
    if (m_object->get())
    {
        var_DelCallback(m_object->get(), qtu(m_varname), VLCVarChoiceModel::on_variable_callback, this);
        var_DelListCallback(m_object->get(), qtu(m_varname), VLCVarChoiceModel::on_variable_list_callback, this);
        var_Destroy(m_object->get(), qtu(m_varname));
    }
}

Qt::ItemFlags VLCVarChoiceModel::flags(const QModelIndex &) const
{
    return Qt::ItemFlag::ItemIsUserCheckable;
}

QHash<int, QByteArray> VLCVarChoiceModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = this->QAbstractListModel::roleNames();
    roleNames[Qt::CheckStateRole] = "checked";
    return roleNames;
}

int VLCVarChoiceModel::rowCount(const QModelIndex &) const
{
    if (!m_object->get())
        return 0;
    return m_values.count();
}

QVariant VLCVarChoiceModel::data(const QModelIndex &index, int role) const
{
    if (!m_object->get())
        return {};

    int row = index.row();
    if (row < 0 || row >= m_values.count())
        return {};

    if (role == Qt::DisplayRole)
        return m_titles[row];
    else if (role == Qt::CheckStateRole)
        return row == m_current;
    return {};
}

bool VLCVarChoiceModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!m_object->get())
        return false;

    int row = index.row();
    if (role == Qt::CheckStateRole && row >= 0 && row < m_values.count())
    {
        //only update the variable when we select an entry
        if (!value.toBool())
            return false;

        int ret = VLC_EGENERIC;
        switch (m_type & VLC_VAR_CLASS) {
        case VLC_VAR_STRING:
            ret = var_SetString(m_object->get(), qtu(m_varname), qtu(m_values[row].toString()) );
            break;
        case VLC_VAR_INTEGER:
            ret = var_SetInteger(m_object->get(), qtu(m_varname), m_values[row].toInt() );
            break;
        case VLC_VAR_FLOAT:
            ret = var_SetFloat(m_object->get(), qtu(m_varname), m_values[row].toFloat() );
            break;
        case VLC_VAR_BOOL:
            ret = var_SetBool(m_object->get(), qtu(m_varname), m_values[row].toBool() );
            break;
        default:
            break;
        }
        return ret == VLC_SUCCESS;
    }
    return false;
}


//update the choices of the variable, called on variable thread from the var_AddCallback callback
//calling vlcValToVariant is safe here as m_type will only be modified when no callbacks is registred
int VLCVarChoiceModel::updateData(const vlc_object_t* object, const vlc_value_t& oldvalue, const vlc_value_t& newvalue)
{
    QVariant oldvalueVariant = vlcValToVariant(oldvalue);
    QVariant newvalueVariant = vlcValToVariant(newvalue);

    emit dataUpdated(object, oldvalueVariant, newvalueVariant, QPrivateSignal{});
    return VLC_SUCCESS;
}

//update the choices of the variable, called on variable thread from the var_AddListCallback callback
//calling vlcValToVariant is safe here as m_type will only be modified when no callbacks is registred
int VLCVarChoiceModel::updateList(const vlc_object_t* object, int action, const vlc_value_t* p_value)
{
    QVariant valueVariant = p_value ? vlcValToVariant(*p_value) : QVariant();
    emit listUpdated(object, action, valueVariant, QPrivateSignal{});
    return VLC_SUCCESS;
}

//update the current value of the variable, called on UI thread
void VLCVarChoiceModel::onDataUpdated(const vlc_object_t* object, QVariant , QVariant newvalue)
{
    if (object != m_object->get())
        return;

    int oldCurrent = m_current;
    m_current = -1;
    for (int i = 0; i < m_values.count(); i++)
        if( newvalue == m_values[i] )
        {
            m_current = i;
            break;
        }

    if (m_current != oldCurrent)
        emit hasCurrentChanged(m_current != -1);
    if (m_current != -1)
        emit dataChanged(index(m_current), index(m_current), { Qt::CheckStateRole } );
    if (oldCurrent != -1)
        emit dataChanged(index(oldCurrent), index(oldCurrent), { Qt::CheckStateRole } );
}

//update the choices of the variable, called on UI thread
void VLCVarChoiceModel::onListUpdated(const vlc_object_t* object, int action, QVariant newvalue)
{
    if (object != m_object->get())
        return;

    switch (action) {
    case VLC_VAR_ADDCHOICE:
        beginInsertRows( QModelIndex{}, m_values.count(), m_values.count() );
        m_values.append( newvalue );
        //we should probably rather get the whole choices list to get the associated text
        m_titles.append( newvalue.toString() );
        endInsertRows();
        break;
    case VLC_VAR_DELCHOICE:
    {
        int i = 0;
        for (i = 0; i < m_values.count(); i++)
            if (newvalue == m_values[i])
                break;
        if (i != m_values.count())
        {
            beginRemoveRows( QModelIndex{}, i, i);
            m_values.removeAt(i);
            m_titles.removeAt(i);
            endRemoveRows();
        }
        break;
    }
    case VLC_VAR_CLEARCHOICES:
        beginResetModel();
        m_values.clear();
        m_titles.clear();
        endResetModel();
        break;
    default:
        //N/A
        break;
    }
}

QString VLCVarChoiceModel::vlcValToString(const vlc_value_t &value)
{
    switch (m_type & VLC_VAR_CLASS) {
    case VLC_VAR_INTEGER:
        return QString::number(value.i_int);
    case VLC_VAR_STRING:
        return qfu(value.psz_string);
    case VLC_VAR_FLOAT:
        return QString::number(value.f_float);
    default:
        return {};
    }
}

QVariant VLCVarChoiceModel::vlcValToVariant(const vlc_value_t &value)
{
    switch (m_type & VLC_VAR_CLASS) {
    case VLC_VAR_INTEGER:
        return QVariant::fromValue<qlonglong>(value.i_int);
    case VLC_VAR_STRING:
        return qfu(value.psz_string);
    case VLC_VAR_FLOAT:
        return value.f_float;
    case VLC_VAR_BOOL:
        return value.b_bool;
    default:
        return {};
    }
}

bool VLCVarChoiceModel::hasCurrent() const
{
    return m_current != -1;
}

void VLCVarChoiceModel::toggleIndex(int index)
{
    setData(QAbstractItemModel::createIndex(index,0),true,Qt::CheckStateRole);
}
