#include "navigation_history.hpp"
#include <QDebug>

NavigationHistory::NavigationHistory(QObject *parent) : QObject(parent)
{

}

QVariant NavigationHistory::getCurrent()
{

    return m_history.back();
}

bool NavigationHistory::isEmpty()
{
    return m_history.count() < 1;
}

void NavigationHistory::push(QVariantMap item, PostAction postAction)
{
    //m_history.push_back(VariantToPropertyMap(item));
    m_history.push_back(item);
    if (m_history.count() == 2)
        emit emptyChanged(true);
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
        itemMap.unite( it->toMap() );
        pushListRec(itemMap, ++it, end);
    }
}

void NavigationHistory::push(QVariantList itemList, NavigationHistory::PostAction postAction)
{
    QVariantMap itemMap;
    pushListRec(itemMap, itemList.cbegin(), itemList.cend());
    push(itemMap, postAction);
}

void NavigationHistory::pop(PostAction postAction)
{
    if (m_history.count() == 1)
        return;

    //delete m_history.back();
    m_history.pop_back();

    if (m_history.count() == 1) {
        emit emptyChanged(true);
    }
    if (postAction == PostAction::Go)
        emit currentChanged(m_history.back());
}
