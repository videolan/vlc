/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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
#ifndef MAINCTX_SUBMODELS_HPP
#define MAINCTX_SUBMODELS_HPP

#include <QObject>
#include <QString>
#include <QJSValue>

class SearchCtx: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString pattern MEMBER m_pattern NOTIFY patternChanged FINAL)
    Q_PROPERTY(bool available MEMBER m_available NOTIFY availableChanged FINAL)

signals:
    void askShow();

public:
    using  QObject::QObject;

signals:
    void patternChanged(const QString& pattern);
    void availableChanged(bool available);

private:
    QString m_pattern;
    bool m_available = false;
};

class SortCtx: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ getAvailable WRITE setAvailable NOTIFY availableChanged FINAL)
    Q_PROPERTY(QJSValue model READ getModel WRITE setModel NOTIFY modelChanged FINAL)
    Q_PROPERTY(QString criteria READ getCriteria WRITE setCriteria NOTIFY criteriaChanged FINAL)
    Q_PROPERTY(Qt::SortOrder order MEMBER m_order NOTIFY orderChanged FINAL)

signals:
    void askShow();

public:
    using QObject::QObject;

    inline QJSValue getModel() const  {
        return m_model;
    }
    inline  void setModel(const QJSValue& value) {
        if (m_model.strictlyEquals(value))
            return;
        m_model = value;
        emit modelChanged(m_model);

        setAvailable(value.isArray()
                     && value.property("length").toInt() > 0);
    }

    inline QString getCriteria() const  {
        return m_criteria;
    }
    inline void setCriteria(const QString& value) {
        if (m_criteria == value)
            return;

        m_criteria = value;
        emit criteriaChanged(m_criteria);
    }

    inline bool getAvailable() const  {
        return m_available;
    }
    inline void setAvailable(bool value) {
        if (m_available == value)
            return;
        m_available = value;
        emit availableChanged(value);
    }

signals:
    void availableChanged(bool available);
    void modelChanged(const QJSValue& model);
    void criteriaChanged(const QString& criteria);
    void orderChanged(const Qt::SortOrder& criteria);

private:
    QJSValue m_model;
    QString m_criteria;
    Qt::SortOrder m_order = Qt::DescendingOrder;
    bool m_available = false;
};

class PlayqueuePanelCtx: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(bool docked READ isDocked WRITE setDocked NOTIFY dockedChanged FINAL)
    Q_PROPERTY(double widthFactor READ widthFactor WRITE setWidthFactor NOTIFY widthFactorChanged FINAL)

public:
    inline PlayqueuePanelCtx(QObject* parent = nullptr): QObject(parent) {}

    inline bool isDocked() const { return m_docked; }
    inline bool isVisible() const { return m_visible; }
    inline double widthFactor() const { return m_widthFactor; }

    inline void setDocked(bool value) {
        if (value == m_docked)
            return;
        m_docked = value;
        emit dockedChanged(value);
    }

    inline void setVisible(bool value) {
        if (value == m_visible)
            return;
        m_visible = value;
        emit visibleChanged(value);
    }

    inline void setWidthFactor(double value) {
        if (value == m_widthFactor)
            return;
        m_widthFactor = value;
        emit widthFactorChanged(value);
    }

signals:
    void visibleChanged(bool visible);
    void dockedChanged(bool);
    void widthFactorChanged(double);

private:
    bool m_visible = false;
    bool m_docked = false;
    ///< playlist size: root.width / playlistScaleFactor
    double  m_widthFactor = 4.;

    friend class MainCtx;
};

#endif // MAINCTX_SUBMODELS_HPP
