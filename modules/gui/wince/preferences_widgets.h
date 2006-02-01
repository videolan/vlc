/*****************************************************************************
 * preferences_widgets.h : WinCE gui plugin for VLC
 *****************************************************************************
 * Copyright (C) 2000-2003 the VideoLAN team
 * $Id$
 *
 * Authors: Marodon Cedric <cedric_marodon@yahoo.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

class ConfigControl
{
public:
    ConfigControl( vlc_object_t *, module_config_t *, HWND, HINSTANCE );
    virtual ~ConfigControl();

    virtual int GetIntValue() {return 0;}
    virtual float GetFloatValue() {return 0;}
    virtual char *GetPszValue() {return GetName();}
    // FIXME returns name corresponding to parent
    // put the panel name corresponding to HWND into the constructor and make it private

    char *GetName();
    int GetType();
    vlc_bool_t IsAdvanced();

    void SetUpdateCallback( void (*)( void * ), void * );

protected:
    /*wxBoxSizer *sizer;*/
    HWND label;
    vlc_object_t *p_this;

    void (*pf_update_callback)( void * );
    void *p_update_data;

    void OnUpdate( UINT );

private:
    HWND parent;

    char *name;
    int i_type;
    vlc_bool_t b_advanced;
};

ConfigControl *CreateConfigControl( vlc_object_t *,
                                    module_config_t *, HWND, HINSTANCE,
                                    int * );

class KeyConfigControl: public ConfigControl
{
public:
    KeyConfigControl( vlc_object_t *, module_config_t *, HWND,
                      HINSTANCE, int * );
    ~KeyConfigControl();
    virtual int GetIntValue();
private:
    HWND alt;
    HWND alt_label;
    HWND ctrl;
    HWND ctrl_label;
    HWND shift;
    HWND shift_label;
    HWND combo;

    // Array of key descriptions, for the ComboBox
    static string *m_keysList;
};

class ModuleConfigControl: public ConfigControl
{
public:
    ModuleConfigControl( vlc_object_t *, module_config_t *, HWND,
                         HINSTANCE, int * );
    ~ModuleConfigControl();
    virtual char *GetPszValue();
private:
    HWND combo;
};

class StringConfigControl: public ConfigControl
{
public:
    StringConfigControl( vlc_object_t *, module_config_t *, HWND,
                         HINSTANCE, int * );
    ~StringConfigControl();
    virtual char *GetPszValue();
private:
    HWND textctrl;
};

class StringListConfigControl: public ConfigControl
{
public:
    StringListConfigControl( vlc_object_t *, module_config_t *, HWND,
                             HINSTANCE, int * );
    ~StringListConfigControl();
    virtual char *GetPszValue();
private:
};

class FileConfigControl: public ConfigControl
{
public:
    FileConfigControl( vlc_object_t *, module_config_t *, HWND,
                       HINSTANCE, int * );
    ~FileConfigControl();
    void OnBrowse( UINT );
    virtual char *GetPszValue();
private:
};

class IntegerConfigControl: public ConfigControl
{
public:
    IntegerConfigControl( vlc_object_t *, module_config_t *, HWND,
                          HINSTANCE, int * );
    ~IntegerConfigControl();
    virtual int GetIntValue();
private:
};

class IntegerListConfigControl: public ConfigControl
{
public:
    IntegerListConfigControl( vlc_object_t *, module_config_t *, HWND,
                              HINSTANCE, int * );
    ~IntegerListConfigControl();
    virtual int GetIntValue();
private:
};

class RangedIntConfigControl: public ConfigControl
{
public:
    RangedIntConfigControl( vlc_object_t *, module_config_t *, HWND,
                            HINSTANCE, int * );
    ~RangedIntConfigControl();
    virtual int GetIntValue();
private:
};

class FloatConfigControl: public ConfigControl
{
public:
    FloatConfigControl( vlc_object_t *, module_config_t *, HWND,
                        HINSTANCE, int * );
    ~FloatConfigControl();
    virtual float GetFloatValue();
private:
    HWND textctrl;
};

class BoolConfigControl: public ConfigControl
{
public:
    BoolConfigControl( vlc_object_t *, module_config_t *, HWND,
                       HINSTANCE, int * );
    ~BoolConfigControl();
    virtual int GetIntValue();
private:
    HWND checkbox;
    HWND checkbox_label;
};
