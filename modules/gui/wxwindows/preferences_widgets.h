/*****************************************************************************
 * preferences_widgets.h : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2003 VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <vector>

class ConfigControl: public wxPanel
{
public:
    ConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~ConfigControl();
    wxSizer *Sizer();

    virtual int GetIntValue() {return 0;}
    virtual float GetFloatValue() {return 0;}
    virtual wxString GetPszValue() {return wxString();}

    wxString GetName();
    int GetType();
    vlc_bool_t IsAdvanced();

    void SetUpdateCallback( void (*)( void * ), void * );

protected:
    wxBoxSizer *sizer;
    wxStaticText *label;
    vlc_object_t *p_this;

    void (*pf_update_callback)( void * );
    void *p_update_data;

    void OnUpdate( wxEvent& );

private:
    wxString name;
    int i_type;
    vlc_bool_t b_advanced;
};

ConfigControl *CreateConfigControl( vlc_object_t *,
                                    module_config_t *, wxWindow * );

class KeyConfigControl: public ConfigControl
{
public:
    KeyConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~KeyConfigControl();
    virtual int GetIntValue();
private:
    wxCheckBox *alt;
    wxCheckBox *ctrl;
    wxCheckBox *shift;
    wxComboBox *combo;
    // Array of key descriptions, for the ComboBox
    static wxString *m_keysList;
};

class ModuleConfigControl: public ConfigControl
{
public:
    ModuleConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~ModuleConfigControl();
    virtual wxString GetPszValue();
private:
    wxComboBox *combo;
};

struct moduleCheckBox {
    wxCheckBox *checkbox;
    char *psz_module;
};

class ModuleCatConfigControl: public ConfigControl
{
public:
    ModuleCatConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~ModuleCatConfigControl();
    virtual wxString GetPszValue();
private:
    wxComboBox *combo;
};


class ModuleListCatConfigControl: public ConfigControl
{
public:
    ModuleListCatConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~ModuleListCatConfigControl();
    virtual wxString GetPszValue();
private:
    std::vector<moduleCheckBox *> pp_checkboxes;

    void OnUpdate( wxCommandEvent& );

    wxTextCtrl *text;
    DECLARE_EVENT_TABLE()
};

;

class StringConfigControl: public ConfigControl
{
public:
    StringConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~StringConfigControl();
    virtual wxString GetPszValue();
private:
    wxTextCtrl *textctrl;

    DECLARE_EVENT_TABLE()
};

class StringListConfigControl: public ConfigControl
{
public:
    StringListConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~StringListConfigControl();
    virtual wxString GetPszValue();
private:
    wxComboBox *combo;
    char *psz_default_value;
    void UpdateCombo( module_config_t *p_item );

    void OnAction( wxCommandEvent& );

    DECLARE_EVENT_TABLE()
};

class FileConfigControl: public ConfigControl
{
public:
    FileConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~FileConfigControl();
    void OnBrowse( wxCommandEvent& );
    virtual wxString GetPszValue();
private:
    wxTextCtrl *textctrl;
    wxButton *browse;
    bool directory;

    DECLARE_EVENT_TABLE()
};

class IntegerConfigControl: public ConfigControl
{
public:
    IntegerConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~IntegerConfigControl();
    virtual int GetIntValue();
private:
    wxSpinCtrl *spin;
    int i_value;

    void OnUpdate( wxScrollEvent& );

    DECLARE_EVENT_TABLE()
};

class IntegerListConfigControl: public ConfigControl
{
public:
    IntegerListConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~IntegerListConfigControl();
    virtual int GetIntValue();
private:
    wxComboBox *combo;
    void UpdateCombo( module_config_t *p_item );

    void OnAction( wxCommandEvent& );

    DECLARE_EVENT_TABLE()
};

class RangedIntConfigControl: public ConfigControl
{
public:
    RangedIntConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~RangedIntConfigControl();
    virtual int GetIntValue();
private:
    wxSlider *slider;

    DECLARE_EVENT_TABLE()
};

class FloatConfigControl: public ConfigControl
{
public:
    FloatConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~FloatConfigControl();
    virtual float GetFloatValue();
private:
    wxTextCtrl *textctrl;

    DECLARE_EVENT_TABLE()
};

class BoolConfigControl: public ConfigControl
{
public:
    BoolConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~BoolConfigControl();
    virtual int GetIntValue();
private:
    wxCheckBox *checkbox;

    DECLARE_EVENT_TABLE()
};

class SectionConfigControl: public ConfigControl
{
public:
    SectionConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~SectionConfigControl();
};
