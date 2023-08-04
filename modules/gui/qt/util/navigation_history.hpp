#ifndef NAVIGATION_HISTORY_HPP
#define NAVIGATION_HISTORY_HPP

#include <QObject>
#include <QtQml/QQmlPropertyMap>

class NavigationHistory : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(QVariant current READ getCurrent NOTIFY currentChanged FINAL)
    Q_PROPERTY(bool previousEmpty READ isPreviousEmpty NOTIFY previousEmptyChanged FINAL)
    Q_PROPERTY(QStringList viewPath READ viewPath NOTIFY viewPathChanged FINAL)

    enum class PostAction{
        Stay,
        Go
    };
    Q_ENUM(PostAction)

public:
    explicit NavigationHistory(QObject *parent = nullptr);

    QVariant getCurrent();
    bool isPreviousEmpty();
    QStringList viewPath() const;

    // NOTE: The first item to call this takes ownership over the focus reason.
    Q_INVOKABLE Qt::FocusReason takeFocusReason();

signals:
    void currentChanged(QVariant current);
    void previousEmptyChanged(bool empty);
    void viewPathChanged(const QStringList& viewPath);

public slots:
    /**
     * Push a
     *
     * \code
     * push({
     *   name: "foo", //push the view foo
     *   properties: {
     *      view: { //the sub view "bar"
     *          name: "bar",
     *          properties: {
     *              baz: "plop" //the property baz will be set in the view "bar"
     *          }
     *      }
     *   }
     * }, History.Go)
     * \endcode
     */
    Q_INVOKABLE void push( QVariantMap, Qt::FocusReason = Qt::OtherFocusReason,
                           PostAction = PostAction::Go );

    /**
     * provide a short version of the history push({k:v}), which implicitly create a dictonnary tree from the input list
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
    Q_INVOKABLE void push(QVariantList itemList, Qt::FocusReason = Qt::OtherFocusReason,
                          PostAction = PostAction::Go );


    /**
     * @brief same as @a push(QVariantMap) but modify the last (current) item instead of insterting a new one
     *
     * @see push
     */
    Q_INVOKABLE void update(QVariantMap itemList);

    /**
     * @brief same as @a push(QVariantList) but modify the last (current) item instead of insterting a new one
     *
     * @see push
     */
    Q_INVOKABLE void update(QVariantList itemList);

    /**
     * @brief same as @a push(QVariantList) but modify the last (current) item's tail instead of insterting a new one
     *
     * @see push
     */
    Q_INVOKABLE void addLeaf(QVariantMap itemMap);


    // Go to previous page
    void previous( Qt::FocusReason = Qt::OtherFocusReason, PostAction = PostAction::Go );

private:
    void updateViewPath();

    QVariantList m_history;
    QStringList m_viewPath;

    Qt::FocusReason m_reason;
};

#endif // NAVIGATION_HISTORY_HPP
