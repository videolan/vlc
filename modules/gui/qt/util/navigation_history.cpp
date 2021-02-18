#include "navigation_history.hpp"
#include <cassert>
#include "network/networkmediamodel.hpp"
#include "medialibrary/mlqmltypes.hpp"

NavigationHistory::NavigationHistory(QObject *parent)
    : QObject(parent)
{
}

QVariant NavigationHistory::getCurrent()
{
    return m_history.back();
}

bool NavigationHistory::isPreviousEmpty()
{
    return m_history.count() <= 1;
}

void NavigationHistory::push(QVariantMap item, PostAction postAction)
{
    m_history.push_back(item);
    emit previousEmptyChanged(false);
    if (postAction == PostAction::Go)
        emit currentChanged(m_history.back());
}

static void pushListRec(QVariantMap& itemMap, QVariantList::const_iterator it, QVariantList::const_iterator end )
{
    if (it == end)
        return;
    if(it->canConvert<QString>())
    {
        itemMap["view"] = it->toString();
        QVariantMap subMap;
        pushListRec(subMap, ++it, end);
        itemMap["viewProperties"] = subMap;
    }
    else if ( it->canConvert<QVariantMap>() )
    {
        QVariantMap varMap = it->toMap();
        for (auto kv = varMap.constBegin(); kv != varMap.constEnd(); ++kv )
            itemMap[kv.key()] = kv.value();
        pushListRec(itemMap, ++it, end);
    }
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


void NavigationHistory::push(QVariantList itemList, NavigationHistory::PostAction postAction)
{
    QVariantMap itemMap;
    pushListRec(itemMap, itemList.cbegin(), itemList.cend());
    push(itemMap, postAction);
}


void NavigationHistory::update(QVariantMap item)
{
    int length = m_history.length();
    assert(length >= 1);
    m_history.back() = item;
}

void NavigationHistory::update(QVariantList itemList)
{
    QVariantMap itemMap;
    pushListRec(itemMap, itemList.cbegin(), itemList.cend());
    update(itemMap);
}

void NavigationHistory::previous(PostAction postAction)
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

    if (postAction == PostAction::Go)
        emit currentChanged( m_history.back() );
}
