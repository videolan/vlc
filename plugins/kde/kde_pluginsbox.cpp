#include "kde_pluginsbox.h"
#include "kde_preferences.h"

#include <videolan/vlc.h>
#include <qgroupbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qvbox.h>
#include <klistview.h>
#include <kbuttonbox.h>

KPluginsBox::KPluginsBox(QString text, QString value, QWidget *parent,
                         int spacing, KPreferences *pref) :
    QGroupBox( 1, Vertical, text, parent )
{
    owner = pref;
    QVBox *item_vbox = new QVBox( this );
    item_vbox->setSpacing(spacing);
    
    listView = new KListView(item_vbox);
    listView->setAllColumnsShowFocus(true);
    listView->addColumn(_("Name"));
    listView->addColumn(_("Description"));
    KButtonBox *item_bbox = new KButtonBox(item_vbox);
    configure = item_bbox->addButton( _("Configure") );
    configure->setEnabled(false);
    selectButton = item_bbox->addButton( _("Select") );
    QHBox *item_hbox = new QHBox(item_vbox);
    item_hbox->setSpacing(spacing);
    new QLabel( _("Selected:"), item_hbox );
    line = new KLineEdit( value, item_hbox );
    connect(selectButton, SIGNAL(clicked()), this, SLOT(selectClicked()));
    connect(configure, SIGNAL(clicked()), this, SLOT(configureClicked()));
    connect(listView, SIGNAL(selectionChanged( QListViewItem *)),
            this, SLOT( selectionChanged( QListViewItem *)));
}

KPluginsBox::~KPluginsBox()
{
    ;
}

QListView* KPluginsBox::getListView()
{
    return listView;
}

void KPluginsBox::selectClicked()
{
    if (listView->selectedItem()) {
        line->setText(listView->selectedItem()->text(0));
        emit selectionChanged(listView->selectedItem()->text(0));
    }
}

void KPluginsBox::configureClicked()
{
    if (listView->selectedItem()) {
        new KPreferences(listView->selectedItem()->text(0), this);
    }
}
void KPluginsBox::selectionChanged( QListViewItem *item )
{
    selectButton->setEnabled(true);
    /* look for module 'psz_name' */
    configure->setEnabled(owner->isConfigureable(item->text(0)));
}
