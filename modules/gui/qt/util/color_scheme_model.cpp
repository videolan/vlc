/*****************************************************************************
 * Copyright (C) 2021 the VideoLAN team
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

#include "color_scheme_model.hpp"

#include "qt.hpp"


#ifdef Q_OS_WIN

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QSettings>

#include <qt_windows.h>

namespace
{
    const char *WIN_THEME_SETTING_PATH = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    const char *WIN_THEME_SETTING_LIGHT_THEME_KEY = "AppsUseLightTheme";
}

class ColorSchemeModel::WinColorSchemeList
        : public ColorSchemeModel::SchemeList
        , public QAbstractNativeEventFilter
{
public:
    static bool canDetectTheme()
    {
        QSettings settings(QLatin1String {WIN_THEME_SETTING_PATH}, QSettings::NativeFormat);
        return settings.contains(WIN_THEME_SETTING_LIGHT_THEME_KEY);
    }

    WinColorSchemeList(ColorSchemeModel *colorSchemeModel)
        : m_colorSchemeModel {colorSchemeModel}
        , m_settings(QLatin1String {WIN_THEME_SETTING_PATH}, QSettings::NativeFormat)
        , m_items {{qtr("Auto"), ColorScheme::Auto}, {qtr("Day"), ColorScheme::Day}, {qtr("Night"), ColorScheme::Night}}
    {
        qApp->installNativeEventFilter(this);
    }

    ~WinColorSchemeList()
    {
        qApp->removeNativeEventFilter(this);
    }

    ColorScheme scheme(int i) const override
    {
        if (m_items.at(i).scheme == ColorScheme::Auto)
            return m_settings.value(WIN_THEME_SETTING_LIGHT_THEME_KEY).toBool()
                    ? ColorScheme::Day : ColorScheme::Night;

        return m_items.at(i).scheme;
    }

    QString text(int i) const override
    {
        return m_items.at(i).text;
    }

    int size() const override
    {
        return m_items.size();
    }

    bool nativeEventFilter(const QByteArray &, void *message, long *) override
    {
        MSG* msg = static_cast<MSG*>( message );
        if ( msg->message == WM_SETTINGCHANGE
             && !lstrcmp( LPCTSTR( msg->lParam ), L"ImmersiveColorSet" ) )
        {
            m_colorSchemeModel->indexChanged(0);
        }
        return false;
    }

private:
    ColorSchemeModel *m_colorSchemeModel;
    QSettings m_settings;
    const QVector<ColorSchemeModel::Item> m_items;
};

#endif

class ColorSchemeModel::DefaultSchemeList : public ColorSchemeModel::SchemeList
{
public:
    DefaultSchemeList()
        : m_items {{qtr("System"), ColorScheme::System}, {qtr("Day"), ColorScheme::Day}, {qtr("Night"), ColorScheme::Night}}
    {
    }

    ColorScheme scheme(int i) const override
    {
        return m_items.at(i).scheme;
    }

    QString text(int i) const override
    {
        return m_items.at(i).text;
    }

    int size() const override
    {
        return m_items.size();
    }

private:
    const QVector<ColorSchemeModel::Item> m_items;
};

std::unique_ptr<ColorSchemeModel::SchemeList> ColorSchemeModel::createList(ColorSchemeModel *parent)
{
#ifdef Q_OS_WIN
    if (WinColorSchemeList::canDetectTheme())
        return std::make_unique<WinColorSchemeList>(parent);
#endif
    Q_UNUSED(parent);
    return std::make_unique<DefaultSchemeList>();
}

ColorSchemeModel::ColorSchemeModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_list {createList(this)}
    , m_currentIndex {0}
{
}

int ColorSchemeModel::rowCount(const QModelIndex &) const
{
    return m_list->size();
}

Qt::ItemFlags ColorSchemeModel::flags (const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);
    if (index.isValid())
        return defaultFlags | Qt::ItemIsUserCheckable;
    return defaultFlags;
}

QVariant ColorSchemeModel::data(const QModelIndex &index, const int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return QVariant {};

    if (role == Qt::CheckStateRole)
        return m_currentIndex == index.row() ? Qt::Checked : Qt::Unchecked;

    if (role == Qt::DisplayRole)
        return m_list->text(index.row());

    return QVariant {};
}

bool ColorSchemeModel::setData(const QModelIndex &index,
                               const QVariant &value, const int role)
{
    if (role == Qt::CheckStateRole
            && checkIndex(index, CheckIndexOption::IndexIsValid)
            && index.row() != m_currentIndex
            && value.type() == QVariant::Bool
            && value.toBool())
    {
        setCurrentIndex(index.row());
        return true;
    }

    return false;
}

int ColorSchemeModel::currentIndex() const
{
    return m_currentIndex;
}

void ColorSchemeModel::setCurrentIndex(const int newIndex)
{
    if (m_currentIndex == newIndex)
        return;

    assert(newIndex >= 0 && newIndex < m_list->size());
    const auto oldIndex = this->index(m_currentIndex);
    m_currentIndex = newIndex;
    emit dataChanged(index(m_currentIndex), index(m_currentIndex), {Qt::CheckStateRole});
    emit dataChanged(oldIndex, oldIndex, {Qt::CheckStateRole});
    emit currentChanged();
}

QString ColorSchemeModel::currentText() const
{
    return m_list->text(m_currentIndex);
}

QVector<ColorSchemeModel::Item> ColorSchemeModel::getSchemes() const
{
    QVector<ColorSchemeModel::Item> vec;
    vec.reserve( m_list->size() );

    for(int i = 0; i < m_list->size(); ++i)
    {
        vec.push_back( { m_list->text(i), m_list->scheme(i) } );
    }
    return vec;
}

ColorSchemeModel::ColorScheme ColorSchemeModel::currentScheme() const
{
    return m_list->scheme(m_currentIndex);
}

void ColorSchemeModel::indexChanged(const int i)
{
    emit dataChanged(index(i), index(i));
    if (i == m_currentIndex)
        emit currentChanged();
}
