#ifndef PRESET_H
#define PRESET_H
#define PRESET_DEBUG 0 /* 0 for no debugging, 1 for normal, 2 for insane */

#define HARD_CUT 0
#define SOFT_CUT 1
#include "preset_types.h"

void evalInitConditions();
void evalPerFrameEquations();
void evalPerFrameInitEquations();

int switchPreset(switch_mode_t switch_mode, int cut_type);
void switchToIdlePreset();
int loadPresetDir(char * dir);
int closePresetDir();
int initPresetLoader();
int destroyPresetLoader();
int loadPresetByFile(char * filename);
void reloadPerFrame(char * s, preset_t * preset);
void reloadPerFrameInit(char *s, preset_t * preset);
void reloadPerPixel(char *s, preset_t * preset);
void savePreset(char * name);


#endif
