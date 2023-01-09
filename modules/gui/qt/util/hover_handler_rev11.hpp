/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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
#ifndef HOVERHANDLERREV11_HPP
#define HOVERHANDLERREV11_HPP

#include <QQuickItem>
#include <QQmlParserStatus>

class HoverHandlerRev11 : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

public:
    Q_PROPERTY(QQuickItem * target MEMBER m_target WRITE setTarget FINAL)
    Q_PROPERTY(bool hovered READ isHovered NOTIFY hoveredChanged FINAL)

public:
    HoverHandlerRev11(QObject *parent = nullptr);
    ~HoverHandlerRev11();

    void setTarget(QQuickItem *target);

    inline bool isHovered() const { return m_hovered; }

signals:
    void hoveredChanged();

private:
    bool eventFilter(QObject *obj, QEvent *event) override;

protected:
    void classBegin() override;
    void componentComplete() override;

private:
    QQuickItem *m_target = nullptr;
    bool m_hovered = false;
};

#endif
