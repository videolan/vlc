#ifndef _KDE_PLUGINBOX_H_
#define _KDE_PLUGINBOX_H_
#include <qgroupbox.h>
#include <klistview.h>
#include <qpushbutton.h>
#include <klineedit.h>
#include "kde_preferences.h"
class KPluginsBox : public QGroupBox
{
    Q_OBJECT
 public:
    KPluginsBox(QString title, QString value, QWidget *parent, int spacing,
                KPreferences *pref);
    ~KPluginsBox();

    QListView *getListView(void);

 private slots:
    void selectClicked(void);
    void configureClicked(void);
    void selectionChanged( QListViewItem * );

 signals:
    void selectionChanged(const QString &text);
    
 private:
    KListView *listView;
    QPushButton *configure;
    QPushButton *selectButton;
    KLineEdit *line;
    KPreferences *owner;
};
#endif
