#include "languagemenu.h"

KLanguageMenuAction::KLanguageMenuAction( intf_thread_t *p_intf, const QString &text, es_descriptor_t * p_es, QObject *parent) : KRadioAction( text,0,parent), p_es(p_es), p_intf(p_intf)
{
    ;
}

void KLanguageMenuAction::setChecked( bool on )
{
    if ( on != isChecked() )
    {
        emit toggled( on, p_es );
        KRadioAction::setChecked( on );
    }
}

KLanguageMenuAction::~KLanguageMenuAction()
{
}
