/*****************************************************************************
 * preferences_widgets.h : wxWindows plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2003 VideoLAN
 * $Id: preferences_widgets.h,v 1.6 2003/11/05 17:46:21 gbazin Exp $
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

protected:
    wxBoxSizer *sizer;
    wxStaticText *label;
    vlc_object_t *p_this;

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
    
class StringConfigControl: public ConfigControl
{
public:
    StringConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~StringConfigControl();
    virtual wxString GetPszValue();
private:
    wxTextCtrl *textctrl;
};

class StringListConfigControl: public ConfigControl
{
public:
    StringListConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~StringListConfigControl();
    virtual wxString GetPszValue();
private:
    wxComboBox *combo;

    void OnRefresh( wxCommandEvent& );
    char *psz_name;
    vlc_callback_t pf_list_update;

    void UpdateCombo( module_config_t *p_item );

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
    DECLARE_EVENT_TABLE()
    wxTextCtrl *textctrl;
    wxButton *browse;
    bool directory;
};

class IntegerConfigControl: public ConfigControl
{
public:
    IntegerConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~IntegerConfigControl();
    virtual int GetIntValue();
private:
    wxSpinCtrl *spin;
};

class IntegerListConfigControl: public ConfigControl
{
public:
    IntegerListConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~IntegerListConfigControl();
    virtual int GetIntValue();
private:
    wxComboBox *combo;

    void OnRefresh( wxCommandEvent& );
    char *psz_name;
    vlc_callback_t pf_list_update;

    void UpdateCombo( module_config_t *p_item );

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
};

class FloatConfigControl: public ConfigControl
{
public:
    FloatConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~FloatConfigControl();
    virtual float GetFloatValue();
private:
    wxTextCtrl *textctrl;
};

class BoolConfigControl: public ConfigControl
{
public:
    BoolConfigControl( vlc_object_t *, module_config_t *, wxWindow * );
    ~BoolConfigControl();
    virtual int GetIntValue();
private:
    wxCheckBox *checkbox;
};
