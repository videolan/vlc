#include "navigation_history.hpp"
#include <cassert>

NavigationHistory::NavigationHistory(QObject *parent)
    : QObject(parent), m_position(-1)
{

}

QVariant NavigationHistory::getCurrent()
{
    return m_history[m_position];
}

bool NavigationHistory::isPreviousEmpty()
{
    return m_position < 1;
}

bool NavigationHistory::isNextEmpty()
{
    return m_position == m_history.length() - 1;
}

void NavigationHistory::push(QVariantMap item, PostAction postAction)
{
    if (m_position < m_history.length() - 1) {
        /* We want to push a new view while we have other views
         * after the current one.
         * In the case we delete all the following views. */
        m_history.erase(m_history.begin() + m_position + 1, m_history.end());
        emit nextEmptyChanged(true);
    }

    //m_history.push_back(VariantToPropertyMap(item));
    m_history.push_back(item);
    // Set to last position
    m_position++;
    if (m_position == 1)
        emit previousEmptyChanged(true);
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
    m_history.replace(m_position, item);
}

void NavigationHistory::update(QVariantList itemList)
{
    QVariantMap itemMap;
    pushListRec(itemMap, itemList.cbegin(), itemList.cend());
    update(itemMap);
}

void NavigationHistory::previous(PostAction postAction)
{
    if (m_position == 0)
        return;

    //delete m_history.back();
    m_position--;

    if (m_position == 0)
        emit previousEmptyChanged(true);
    if (m_position == m_history.length() - 2)
        emit nextEmptyChanged(false);

    if (postAction == PostAction::Go)
        emit currentChanged(m_history[m_position]);
}

void NavigationHistory::next(PostAction postAction)
{
    if (m_position == m_history.length() - 1)
        return;

    m_position++;

    if (m_position == 1)
        emit previousEmptyChanged(false);
    if (m_position == m_history.length() - 1)
        emit nextEmptyChanged(true);

    if (postAction == PostAction::Go)
        emit currentChanged(m_history[m_position]);
}
