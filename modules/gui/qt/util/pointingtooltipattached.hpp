/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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
#ifndef POINTINGTOOLTIPATTACHED_HPP
#define POINTINGTOOLTIPATTACHED_HPP

#include <QQmlProperty>
#include <QPointF>
#include <QQuickItem>
#include <QString>
#include <QtQml>

class PointingToolTipAttached : public QObject
{
    Q_OBJECT

    // Note that instance is static:
    Q_PROPERTY(QObject* instance MEMBER m_instance NOTIFY instanceChanged FINAL)

    // Note that these properties are not bidirectionally synchronized. If you
    // want to get the actual values that the tool tip uses, access it through
    // the `instance` property:
    Q_PROPERTY(QPointF pos MEMBER m_pos WRITE setPos NOTIFY posChanged FINAL)
    Q_PROPERTY(bool visible MEMBER m_visible WRITE setVisible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(QString text MEMBER m_text WRITE setText NOTIFY textChanged FINAL)

    // The name `PointingToolTip` is not used to not conflict with the already existing type.
    QML_NAMED_ELEMENT(PointingToolTipAttached)
    QML_UNCREATABLE("PointingToolTipAttached is meant to be used as attached.")
    QML_ATTACHED(PointingToolTipAttached)

public:
    explicit PointingToolTipAttached(QObject *parent = nullptr)
        : QObject(parent)
    {
        // This is an unlikely case, but we can still handle it:
        connect(this, &PointingToolTipAttached::instanceChanged, this, [this]() {
            m_posProperty.reset();
            m_visibleProperty.reset();
            m_textProperty.reset();
            m_parentProperty.reset();

            if (m_instance)
            {
                setPos(m_pos);
                setText(m_text);
                setVisible(m_visible);
            }
        });
    }

    static PointingToolTipAttached *qmlAttachedProperties(QObject* parent)
    {
        return new PointingToolTipAttached(parent);
    }

    void setPos(const QPointF& point)
    {
        if (m_pos != point)
        {
            m_pos = point;
            emit posChanged();
        }

        if (instanceBoundToParent())
        {
            if (!m_posProperty)
                m_posProperty = QQmlProperty(m_instance, QStringLiteral("pos"));

            assert(m_posProperty->isWritable());

            m_posProperty->write(point);
        }
    }

    void setText(const QString& text)
    {
        if (m_text != text)
        {
            m_text = text;
            emit textChanged();
        }

        if (instanceBoundToParent())
        {
            if (!m_textProperty)
                m_textProperty = QQmlProperty(m_instance, QStringLiteral("text"));

            assert(m_textProperty->isWritable());

            m_textProperty->write(text);
        }
    }

    // Setting visibility binds the attached tool tip to the parent of the
    // attached tool tip:
    void setVisible(bool visible)
    {
        if (m_visible != visible)
        {
            m_visible = visible;
            emit visibleChanged();
        }

        assert(m_instance);

        if (!m_visibleProperty)
            m_visibleProperty = QQmlProperty(m_instance, QStringLiteral("visible"));

        assert(m_visibleProperty->isWritable());

        // Akin to `QQuickToolTipAttached`:
        if (visible)
        {
            setPos(m_pos);
            setText(m_text);

            if (!m_parentProperty)
                m_parentProperty = QQmlProperty(m_instance, QStringLiteral("parent"));

            assert(m_parentProperty->isWritable());

            m_parentProperty->write(QVariant::fromValue(qobject_cast<QQuickItem*>(parent())));

            m_visibleProperty->write(true);
        }
        else
        {
            if (instanceBoundToParent())
                m_visibleProperty->write(false);
        }
    }

signals:
    void instanceChanged();

    void zChanged();
    void posChanged();
    void visibleChanged();
    void textChanged();

protected:
    bool instanceBoundToParent() const
    {
        assert(m_instance);

        if (!m_parentProperty)
            m_parentProperty = QQmlProperty(m_instance, QStringLiteral("parent"));

        assert(m_parentProperty->isValid());

        return (m_parentProperty->read().value<QQuickItem*>() == qobject_cast<QQuickItem*>(parent()));
    }

private:
    static inline QPointer<QObject> m_instance;

    QPointF m_pos;
    bool m_visible = false;
    QString m_text;

    std::optional<QQmlProperty> m_posProperty;
    std::optional<QQmlProperty> m_visibleProperty;
    std::optional<QQmlProperty> m_textProperty;
    mutable std::optional<QQmlProperty> m_parentProperty;
};

QML_DECLARE_TYPEINFO(PointingToolTipAttached, QML_HAS_ATTACHED_PROPERTIES)

#endif // POINTINGTOOLTIPATTACHED_HPP
