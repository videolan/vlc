/*
 * AtmoDynData.cpp: class for holding all variable data - which may be
 * passed between function calls, into threads instead of the use
 * of global variables
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "AtmoDynData.h"

#if defined(_ATMO_VLC_PLUGIN_)
CAtmoDynData::CAtmoDynData(vlc_object_t *p_atmo_filter, CAtmoConfig *pAtmoConfig) {
    this->p_atmo_filter     = p_atmo_filter;
    this->m_pAtmoConfig     = pAtmoConfig;
    this->m_pAtmoConnection = NULL;
    this->m_pCurrentEffectThread = NULL;

    this->m_pLivePacketQueue = NULL;
    this->m_pLiveInput = NULL;
    this->m_LivePictureSource = lpsExtern;
    vlc_mutex_init( &m_lock );
}
#else
CAtmoDynData::CAtmoDynData(HINSTANCE hInst, CAtmoConfig *pAtmoConfig, CAtmoDisplays *pAtmoDisplays) {
    this->m_pAtmoConfig     = pAtmoConfig;
    this->m_pAtmoDisplays   = pAtmoDisplays;
    this->m_pAtmoConnection = NULL;
    this->m_pCurrentEffectThread = NULL;
    this->m_hInst = hInst;

    this->m_pLivePacketQueue = NULL;
    this->m_pLiveInput = NULL;
    this->m_LivePictureSource = lpsScreenCapture;
    InitializeCriticalSection( &m_RemoteCallCriticalSection );
}
#endif

CAtmoDynData::~CAtmoDynData(void)
{
#if defined(_ATMO_VLC_PLUGIN_)
    vlc_mutex_destroy( &m_lock );
#else
    DeleteCriticalSection(&m_RemoteCallCriticalSection);
#endif
}

void CAtmoDynData::LockCriticalSection() {
#if defined(_ATMO_VLC_PLUGIN_)
    vlc_mutex_lock( &m_lock );
#else
    EnterCriticalSection(&m_RemoteCallCriticalSection);
#endif
}

void CAtmoDynData::UnLockCriticalSection() {
#if defined(_ATMO_VLC_PLUGIN_)
    vlc_mutex_unlock( &m_lock );
#else
    LeaveCriticalSection(&m_RemoteCallCriticalSection);
#endif
}

void CAtmoDynData::CalculateDefaultZones()
{
  int i;
  int num_cols_top;
  int num_cols_bottom;
  int num_rows;
  CAtmoZoneDefinition *zoneDef;

  if(!m_pAtmoConfig)
     return;

  m_pAtmoConfig->UpdateZoneDefinitionCount();


  num_cols_top    = m_pAtmoConfig->getZonesTopCount();
  num_cols_bottom = m_pAtmoConfig->getZonesBottomCount();
  num_rows        = m_pAtmoConfig->getZonesLRCount();

  for(int zone=0; zone < m_pAtmoConfig->getZoneCount(); zone++)
  {
     zoneDef = m_pAtmoConfig->getZoneDefinition(zone);
     if(zoneDef)
        zoneDef->Fill(0);
  }


  // the zones will be counted starting from top left - in clockwise order around the display
  // the summary channel will be the last one (in the center)
  i = 0;
  // top zones from left to right
  for(int c=0;c<num_cols_top;c++)
  {
       zoneDef = m_pAtmoConfig->getZoneDefinition(i); i++;
       if(zoneDef) {
          int l = (c * CAP_WIDTH)/num_cols_top;
          int r = ((c+1) * CAP_WIDTH)/num_cols_top;
          zoneDef->FillGradientFromTop( ATMO_MAX( l - CAP_ZONE_OVERLAP, 0) , ATMO_MIN( r + CAP_ZONE_OVERLAP, CAP_WIDTH ) );
       }
  }
  // right zones from top to bottom
  for(int r=0;r<num_rows;r++)
  {
       zoneDef = m_pAtmoConfig->getZoneDefinition(i); i++;
       if(zoneDef) {
          int t = (r * CAP_HEIGHT)/num_rows;
          int b = ((r+1) * CAP_HEIGHT)/num_rows;
          zoneDef->FillGradientFromRight( ATMO_MAX( t - CAP_ZONE_OVERLAP, 0) , ATMO_MIN( b + CAP_ZONE_OVERLAP, CAP_HEIGHT) );
       }
  }
  //  bottom zones from  RIGHT to LEFT!
  for(int c=(num_cols_bottom-1);c>=0;c--)
  {
       zoneDef = m_pAtmoConfig->getZoneDefinition(i); i++;
       if(zoneDef) {
          int l = (c * CAP_WIDTH)/num_cols_bottom;
          int r = ((c+1) * CAP_WIDTH)/num_cols_bottom;
          zoneDef->FillGradientFromBottom( ATMO_MAX( l - CAP_ZONE_OVERLAP, 0 ), ATMO_MIN( r + CAP_ZONE_OVERLAP, CAP_WIDTH ) );
       }
  }
  // left zones from bottom to top!
  for(int r=(num_rows-1);r>=0;r--)
  {
       zoneDef = m_pAtmoConfig->getZoneDefinition(i); i++;
       if(zoneDef)
       {
          int t = (r * CAP_HEIGHT)/num_rows;
          int b = ((r+1) * CAP_HEIGHT)/num_rows;
          zoneDef->FillGradientFromLeft( ATMO_MAX( t - CAP_ZONE_OVERLAP, 0 ), ATMO_MIN( b + CAP_ZONE_OVERLAP, CAP_HEIGHT ) );
       }
  }
  if(m_pAtmoConfig->getZoneSummary())
  {
     // and last the summary zone if requested!
     zoneDef = m_pAtmoConfig->getZoneDefinition(i++);
     if(zoneDef)
        zoneDef->Fill(255);
  }
}


#if defined(_ATMO_VLC_PLUGIN_)
void CAtmoDynData::ReloadZoneDefinitionBitmaps()
{
 // only as dummy for VLC Module - to avoid to if def out all calls to this function
}
#endif


#if !defined(_ATMO_VLC_PLUGIN_)

void CAtmoDynData::setWorkDir(const char *dir)
{
    strcpy( m_WorkDir, dir );
}

char *CAtmoDynData::getWorkDir()
{
    return m_WorkDir;
}

void CAtmoDynData::ReloadZoneDefinitionBitmaps()
{
  int i;
  // suchlogik für die Bitmaps ...
  // <WorkDir>\hardware\numchannels\zone..0..n.bmp
  // <WorkDir>\hardware\zone..0..n.bmp
  // <WorkDir>\zone..0..n.bmp
  // Automatik Berechnung...
  LockCriticalSection();
  if(!m_pAtmoConnection || !m_pAtmoConfig) {
      UnLockCriticalSection();
      return;
  }

  m_pAtmoConfig->UpdateZoneDefinitionCount();

  CalculateDefaultZones();


  char psz_filename[MAX_PATH];
  CAtmoZoneDefinition *zoneDef;

  sprintf(psz_filename,"%s%s",
                        m_WorkDir,
                        m_pAtmoConnection->getDevicePath()
                );
  CreateDirectory( psz_filename, NULL );

  sprintf(psz_filename,"%s%s\\%dx%dx%d",
                        m_WorkDir,
                        m_pAtmoConnection->getDevicePath(),
                        m_pAtmoConfig->getZonesTopCount(),
                        m_pAtmoConfig->getZonesLRCount(),
                        m_pAtmoConfig->getZonesBottomCount()

               );
  CreateDirectory(psz_filename, NULL );

  // try to load device depended zone definition bitmaps
  for(int zone=0; zone < m_pAtmoConfig->getZoneCount(); zone++)  {
      zoneDef = m_pAtmoConfig->getZoneDefinition(zone);
      if(!zoneDef) continue;

      sprintf(psz_filename,"%s%s\\%dx%dx%d\\zone_%d.bmp",
                        m_WorkDir,
                        m_pAtmoConnection->getDevicePath(),
                        m_pAtmoConfig->getZonesTopCount(),
                        m_pAtmoConfig->getZonesLRCount(),
                        m_pAtmoConfig->getZonesBottomCount(),
                        zone
                );
      i = zoneDef->LoadGradientFromBitmap( psz_filename );
      if(i == ATMO_LOAD_GRADIENT_OK) continue;
      if((i == ATMO_LOAD_GRADIENT_FAILED_SIZE) || (i == ATMO_LOAD_GRADIENT_FAILED_HEADER))
         MessageBox(0,psz_filename,"Failed to load, Check Format, Check Size.",MB_ICONERROR);

      sprintf(psz_filename,"%s%s\\zone_%d.bmp",
                        m_WorkDir,
                        m_pAtmoConnection->getDevicePath(),
                        zone
                );
      i = zoneDef->LoadGradientFromBitmap( psz_filename );
      if(i == ATMO_LOAD_GRADIENT_OK) continue;
      if((i == ATMO_LOAD_GRADIENT_FAILED_SIZE) || (i == ATMO_LOAD_GRADIENT_FAILED_HEADER))
         MessageBox(0,psz_filename,"Failed to load, Check Format, Check Size.",MB_ICONERROR);

      sprintf(psz_filename,"%szone_%d.bmp",
                        m_WorkDir,
                        zone
                );
      i = zoneDef->LoadGradientFromBitmap( psz_filename );
      if(i == ATMO_LOAD_GRADIENT_OK) continue;
      if((i == ATMO_LOAD_GRADIENT_FAILED_SIZE) || (i == ATMO_LOAD_GRADIENT_FAILED_HEADER))
         MessageBox(0,psz_filename,"Failed to load, Check Format, Check Size.",MB_ICONERROR);
  }

  UnLockCriticalSection();
}

#endif



