// License GPLv2 or later
// COde by atmo

#include "registry.hpp"

QVLCRegistry::QVLCRegistry( HKEY rootKey )
{
    m_RootKey = rootKey;
}

QVLCRegistry::~QVLCRegistry( void )
{
}

bool QVLCRegistry::RegistryKeyExists( char *path )
{
    HKEY keyHandle;
    if(  RegOpenKeyEx( m_RootKey, path, 0, KEY_READ, &keyHandle ) == ERROR_SUCCESS )
    {
        RegCloseKey( keyHandle );
        return true;
    }
    return false;
}

bool QVLCRegistry::RegistryValueExists( char *path, char *valueName )
{
    HKEY keyHandle;
    bool temp = false;
    DWORD size1;
    DWORD valueType;

    if(  RegOpenKeyEx( m_RootKey, path, 0, KEY_READ, &keyHandle ) == ERROR_SUCCESS )
    {
        if( RegQueryValueEx( keyHandle, valueName, NULL,
                             &valueType, NULL, &size1 ) == ERROR_SUCCESS )
        {
           temp = true;
        }
        RegCloseKey( keyHandle );
    }
    return temp;
}

void QVLCRegistry::WriteRegistryInt( char *path, char *valueName, int value )
{
    HKEY keyHandle;

    if(  RegCreateKeyEx( m_RootKey, path, 0, NULL, REG_OPTION_NON_VOLATILE,
                         KEY_WRITE, NULL, &keyHandle, NULL )  == ERROR_SUCCESS )
    {
        RegSetValueEx( keyHandle, valueName, 0, REG_DWORD,
                (LPBYTE)&value, sizeof( int ) );
        RegCloseKey( keyHandle );
    }
}

void QVLCRegistry::WriteRegistryString( char *path, char *valueName, char *value )
{
    HKEY keyHandle;

    if(  RegCreateKeyEx( m_RootKey, path, 0, NULL, REG_OPTION_NON_VOLATILE,
                         KEY_WRITE, NULL, &keyHandle, NULL )  == ERROR_SUCCESS )
    {
        RegSetValueEx( keyHandle, valueName, 0, REG_SZ, (LPBYTE)value,
                (DWORD)( strlen( value ) + 1 ) );
        RegCloseKey( keyHandle );
    }
}

void QVLCRegistry::WriteRegistryDouble( char *path, char *valueName, double value )
{
    HKEY keyHandle;
    if( RegCreateKeyEx( m_RootKey, path, 0, NULL, REG_OPTION_NON_VOLATILE,
                       KEY_WRITE, NULL, &keyHandle, NULL ) == ERROR_SUCCESS )
    {
        RegSetValueEx( keyHandle, valueName, 0, REG_BINARY, (LPBYTE)&value, sizeof( double ) );
        RegCloseKey( keyHandle );
    }
}

int QVLCRegistry::ReadRegistryInt( char *path, char *valueName, int default_value ) {
    HKEY keyHandle;
    int tempValue;
    DWORD size1;
    DWORD valueType;

    if(  RegOpenKeyEx( m_RootKey, path, 0, KEY_READ, &keyHandle ) == ERROR_SUCCESS )
    {
        if( RegQueryValueEx(  keyHandle, valueName, NULL, &valueType, NULL, &size1 ) == ERROR_SUCCESS )
        {
           if( valueType == REG_DWORD )
           {
               if( RegQueryValueEx(  keyHandle, valueName, NULL, &valueType, (LPBYTE)&tempValue, &size1 ) == ERROR_SUCCESS )
               {
                  default_value = tempValue;
               };
           }
        }
        RegCloseKey( keyHandle );
    }
    return default_value;
}

char * QVLCRegistry::ReadRegistryString( char *path, char *valueName, char *default_value )
{
    HKEY keyHandle;
    char *tempValue = NULL;
    DWORD size1;
    DWORD valueType;

    if( RegOpenKeyEx( m_RootKey, path, 0, KEY_READ, &keyHandle ) == ERROR_SUCCESS )
    {
        if( RegQueryValueEx(  keyHandle, valueName, NULL, &valueType, NULL, &size1 ) == ERROR_SUCCESS )
        {
           if( valueType == REG_SZ )
           {
               // free
               tempValue = ( char * )malloc( size1+1 ); // +1 für NullByte`?
               if( RegQueryValueEx(  keyHandle, valueName, NULL, &valueType, (LPBYTE)tempValue, &size1 ) == ERROR_SUCCESS )
               {
                  default_value = tempValue;
               };
           }
        }
        RegCloseKey( keyHandle );
    }
    if( tempValue == NULL )
    {
        // wenn tempValue nicht aus registry gelesen wurde dafür sorgen das ein neuer String mit der Kopie von DefaultValue
        // geliefert wird - das macht das Handling des Rückgabewertes der Funktion einfacher - immer schön mit free freigeben!
        default_value = strdup( default_value );
    }

    return default_value;
}

double QVLCRegistry::ReadRegistryDouble( char *path, char *valueName, double default_value )
{
    HKEY keyHandle;
    double tempValue;
    DWORD size1;
    DWORD valueType;

    if( RegOpenKeyEx( m_RootKey, path, 0, KEY_READ, &keyHandle ) == ERROR_SUCCESS )
    {
        if( RegQueryValueEx( keyHandle, valueName, NULL, &valueType,
                             NULL, &size1 ) == ERROR_SUCCESS )
        {
           if( ( valueType == REG_BINARY ) && ( size1 == sizeof( double ) ) )
           {
               if( RegQueryValueEx(  keyHandle, valueName, NULL, &valueType,
                           (LPBYTE)&tempValue, &size1 ) == ERROR_SUCCESS )
               {
                  default_value = tempValue;
               };
           }
        }
        RegCloseKey( keyHandle );
    }
    return default_value;
}

