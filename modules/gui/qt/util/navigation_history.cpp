#include "navigation_history.hpp"
#include <cassert>
#include "network/networkmediamodel.hpp"
#include "medialibrary/mlqmltypes.hpp"


NavigationHistory::NavigationHistory(QObject *parent)
    : QObject(parent),
      m_reason(Qt::OtherFocusReason)
{
}

QVariant NavigationHistory::getCurrent()
{
    assert(m_history.isEmpty() == false);

    return m_history.back();
}

bool NavigationHistory::isPreviousEmpty()
{
    return m_history.count() <= 1;
}

void NavigationHistory::push(QVariantMap item, Qt::FocusReason reason, PostAction postAction)
{
    m_history.push_back(item);
    emit previousEmptyChanged(false);
    if (postAction == PostAction::Go)
    {
        updateViewPath();

        m_reason = reason;

        emit currentChanged(m_history.back());
    }
}

static void pushListRec(QVariantMap& itemMap, QVariantList::const_iterator it, QVariantList::const_iterator end )
{
    if (it == end)
        return;
    if(it->canConvert<QString>())
    {
        QVariantMap subViewMap;
        subViewMap["name"] = it->toString();
        QVariantMap subViewProperties;
        pushListRec(subViewProperties, ++it, end);
        subViewMap["properties"] = subViewProperties;
        itemMap["view"] = subViewMap;
    }
    else if ( it->canConvert<QVariantMap>() )
    {
        QVariantMap varMap = it->toMap();
        for (auto kv = varMap.constBegin(); kv != varMap.constEnd(); ++kv )
            itemMap[kv.key()] = kv.value();
        pushListRec(itemMap, ++it, end);
    }
}

static void addLeafRec(QVariant &item, const QVariantMap &leaf)
{
    auto itemMap = item.toMap();
    if (itemMap.contains("view"))
    {
        QVariant viewProps = itemMap.value("view");
        addLeafRec(viewProps, leaf);
        itemMap["view"] = viewProps;
    }
    else if (itemMap.contains("properties"))
    {
        QVariant propsVar = itemMap.value("properties");
        const auto propsMap = propsVar.toMap();
        if (propsMap.empty())
        {
            itemMap["properties"] = leaf;
        }
        else
        {
            addLeafRec(propsVar, leaf);
            itemMap["properties"] = propsVar;
        }
    }
    else
    {
        // invalid node?
        return;
    }

    //overwrite item QVariant
    item = itemMap;
}


static bool isNodeValid(QVariant& value)
{
    if (value.canConvert(QVariant::StringList)
        || value.canConvert(QVariant::StringList)
        || value.canConvert(QVariant::String)
        || value.canConvert(QVariant::UInt)
        || value.canConvert(QVariant::Int)
        || value.canConvert(QVariant::Bool)
        || value.canConvert<MLItemId>())
    {
        return true;
    }
    else if ( value.canConvert(QVariant::List) )
    {
        QVariantList valueList = value.toList();
        for (QVariant& v : valueList) {
            if (!isNodeValid(v))
                return false;
        }
        return true;
    }
    else if (value.canConvert<NetworkTreeItem>() )
    {
        NetworkTreeItem item = value.value<NetworkTreeItem>();
        if ( ! item.isValid() )
        {
            return false;
        }
        return true;
    }
    else if ( value.canConvert(QVariant::Map) )
    {
        QVariantMap valueList = value.toMap();
        for (QVariant& v : valueList.values()) {
            if (!isNodeValid(v)) {
                return false;
            }
        }
        return true;
    }

    assert(false);
    return false;
}

static QStringList getViewPath(QVariantMap map)
{
    QStringList r;
    if (map.contains("view"))
        return getViewPath(map.value("view").toMap());
    else if (map.contains("name"))
    {
        r = QStringList( map.value("name").toString() );
        r.append(std::move(getViewPath(map.value("properties").toMap())));
    }
    return r;
}

void NavigationHistory::push(QVariantList itemList, Qt::FocusReason reason,
                             NavigationHistory::PostAction postAction)
{
    QVariantMap itemMap;
    pushListRec(itemMap, itemList.cbegin(), itemList.cend());
    if (!itemMap.contains("view"))
        return;
    QVariant rootView = itemMap["view"];
    if (!rootView.canConvert(QVariant::Map))
        return;
    push(rootView.toMap(), reason, postAction);
}


void NavigationHistory::update(QVariantMap item)
{
    assert(m_history.size() >= 1);
    m_history.back() = item;
    updateViewPath();
}

void NavigationHistory::update(QVariantList itemList)
{
    QVariantMap itemMap;
    pushListRec(itemMap, itemList.cbegin(), itemList.cend());
    if (!itemMap.contains("view"))
        return;
    QVariant rootView = itemMap["view"];
    if (!rootView.canConvert(QVariant::Map))
        return;
    update(rootView.toMap());
}

void NavigationHistory::addLeaf(QVariantMap itemMap)
{
    assert(m_history.size() >= 1);
    addLeafRec(m_history.back(), itemMap);
    updateViewPath();
}

void NavigationHistory::previous(Qt::FocusReason reason, PostAction postAction)
{
    if (m_history.count() == 1)
        return;

    m_history.pop_back();
    while (!isNodeValid(m_history.back())) {
        m_history.pop_back();
        if (m_history.count() == 1)
            break;
    }

    if (m_history.count() == 1)
        emit previousEmptyChanged(true);

    if (postAction == PostAction::Go) {
        updateViewPath();

        m_reason = reason;

        emit currentChanged( m_history.back() );
    }
}

void NavigationHistory::updateViewPath()
{
    const auto viewPath = getViewPath(getCurrent().toMap());
    if (viewPath == m_viewPath)
        return;

    m_viewPath = viewPath;
    emit viewPathChanged( m_viewPath );
}

QStringList NavigationHistory::viewPath() const
{
    return m_viewPath;
}

Qt::FocusReason NavigationHistory::takeFocusReason()
{
    Qt::FocusReason reason = m_reason;

    m_reason = Qt::OtherFocusReason;

    return reason;
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
