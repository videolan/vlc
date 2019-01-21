#ifndef NAVIGATION_HISTORY_HPP
#define NAVIGATION_HISTORY_HPP

#include <QObject>
#include <QList>
#include <QtQml/QQmlPropertyMap>

class NavigationHistory : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(QVariant current READ getCurrent NOTIFY currentChanged)
    Q_PROPERTY(bool empty READ isEmpty NOTIFY emptyChanged)

    enum class PostAction{
        Stay,
        Go
    };
    Q_ENUM(PostAction)

public:
    explicit NavigationHistory(QObject *parent = nullptr);

    QVariant getCurrent();
    bool isEmpty();

signals:
    void currentChanged(QVariant current);
    void emptyChanged(bool empty);

public slots:
    /**
     * Push a
     *
     * \code
     * push({
     *   view: "foo", //push the view foo
     *   viewProperties: {
     *      view : "bar",  //the sub view "bar"
     *      viewProperties: {
     *         baz: "plop" //the property baz will be set in the view "bar"
     *      }
     *   }
     * }, History.Go)
     * \endcode
     */
    Q_INVOKABLE void push( QVariantMap, PostAction = PostAction::Stay );

    /**
     * provide a short version of the history push({k:v}), wich implicitly create a dictonnary tree from the input list
     *
     * List items are interpreted as
     *   * strings will push a dict with "view" key to the value of the string and
     *     a "viewProperties" dict configured with the tail of the list
     *
     *   * dict: values will be added to the current viewProperty
     *
     * example:
     * \code
     * //push the view foo, then bar, set baz to plop in the view "bar"
     *  push(["foo", "bar", {baz: "plop"} ], History.Go)
     * \endcode
     */
    Q_INVOKABLE void push(QVariantList itemList, PostAction = PostAction::Stay );
    //pop the last page
    void pop( PostAction = PostAction::Stay );

private:
    QVariantList m_history;
};

#endif // NAVIGATION_HISTORY_HPP
