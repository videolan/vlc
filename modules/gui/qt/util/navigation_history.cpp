#include "navigation_history.hpp"
#include <cassert>
#include "network/networkmediamodel.hpp"
#include "medialibrary/mlqmltypes.hpp"

NavigationHistory::NavigationHistory(QObject *parent)
    : QObject(parent)
{
}

bool NavigationHistory::isPreviousEmpty()
{
    return m_history.size() <= 1;
}

void NavigationHistory::push(QStringList path, const QVariantMap& properties, Qt::FocusReason focusReason)
{
    auto prop = std::make_unique<QQmlPropertyMap>();
    for (auto it =  properties.keyValueBegin(); it != properties.keyValueEnd(); ++it)
        prop->insert((*it).first, (*it).second);

    m_history.emplace_back(path, std::move(prop));

    updateCurrent();

    if (m_history.size() == 2)
        emit previousEmptyChanged(false);

    emit navigate(focusReason);
}

void NavigationHistory::push(QStringList path, Qt::FocusReason focusReason)
{
    m_history.emplace_back(path, std::make_unique<QQmlPropertyMap>());

    updateCurrent();

    if (m_history.size() == 2)
        emit previousEmptyChanged(false);

    emit navigate(focusReason);

}

static bool isNodeValid(const QVariant& value)
{
    if (value.canConvert<QStringList>()
        || value.canConvert<QString>()
        || value.canConvert<unsigned int>()
        || value.canConvert<int>()
        || value.canConvert<bool>()
        || value.canConvert<MLItemId>())
    {
        return true;
    }
    else if ( value.canConvert<QVariantList>() )
    {
        QVariantList valueList = value.toList();
        for (QVariant& v : valueList)
        {
            if (!isNodeValid(v))
                return false;
        }
        return true;
    }
    else if (value.canConvert<NetworkTreeItem>() )
    {
        NetworkTreeItem item = value.value<NetworkTreeItem>();
        if ( ! item.isValid() )
            return false;

        return true;
    }
    else if ( value.canConvert<QVariantMap>() )
    {
        QVariantMap valueList = value.toMap();
        for (QVariant& v : valueList.values())
        {
            if (!isNodeValid(v))
                return false;
        }
        return true;
    }
    else if ( value.isNull() )
        return true;

    assert(false);
    return false;
}

static bool isNodeValid(const QQmlPropertyMap& value)
{
    for (auto it: value.keys())
    {
        if (!isNodeValid(value[it]))
            return false;
    }
    return true;
}



void NavigationHistory::update(QStringList path)
{
    if (m_history.size() == 0)
    {
        m_history.emplace_back(path, std::make_unique<QQmlPropertyMap>());
    }
    else
    {
        auto& last = *m_history.rbegin();
        last.first = path;
    }
    updateCurrent();
}

void NavigationHistory::update(QStringList path, const QVariantMap& properties)
{
    if (m_history.size() == 0)
    {
        auto prop = std::make_unique<QQmlPropertyMap>();
        for (auto it =  properties.keyValueBegin(); it != properties.keyValueEnd(); ++it)
        {
            prop->insert((*it).first, (*it).second);
        }
        m_history.emplace_back(path, std::move(prop));
    }
    else
    {
        auto& last = *m_history.rbegin();
        last.first = path;
    }
    updateCurrent();
}


void NavigationHistory::previous(Qt::FocusReason reason)
{
    if (m_history.size() == 1)
        return;

    m_history.pop_back();

    while (!isNodeValid(*m_history.back().second)) {
        m_history.pop_back();
        if (m_history.size() == 1)
            break;
    }

    if (m_history.size() == 1)
        emit previousEmptyChanged(true);

    updateCurrent();
    emit navigate(reason);
}

void NavigationHistory::updateCurrent()
{
    assert(m_history.size() >= 1);
    const auto it = m_history.rbegin();

    m_viewPath = it->first;
    m_viewProperties = it->second.get();
    emit viewPathChanged(m_viewPath);
    emit viewPropChanged(m_viewProperties);
}

QStringList NavigationHistory::viewPath() const
{
    return m_viewPath;
}

QQmlPropertyMap* NavigationHistory::viewProp() const
{
    return m_viewProperties;
}

Q_INVOKABLE bool NavigationHistory::match(const QStringList& path,  const QStringList& pattern)
{
    if (pattern.length() > path.length())
        return false;
    for (qsizetype i = 0; i < pattern.length(); i++)
    {
        if (pattern[i] != path[i])
            return false;
    }
    return true;
}

Q_INVOKABLE bool NavigationHistory::exactMatch(const QStringList& path,  const QStringList& pattern)
{
    if (pattern.length() != path.length())
        return false;
    for (qsizetype i = 0; i < pattern.length(); i++)
    {
        if (pattern[i] != path[i])
            return false;
    }
    return true;
}
