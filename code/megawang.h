//
//  megawang.h
//  sw
//
//  Created by serge on 21/4/13.
//  Copyright (c) 2013 serge. All rights reserved.
//

#ifndef sw_megawang_h
#define sw_megawang_h

#include <stdio.h>
#include "sw_system.h"
#include "gui.h"

#pragma pack(push,1)
typedef struct _VideMode {
    int width;
    int height;
    int bpp;
    int fullscreen;
} VideoMode;
#pragma pack(pop)

#ifdef __cplusplus
#include <vector>
typedef std::vector<VideoMode> VideoModeList;

void dnGetVideoModeList(VideoModeList& modes);
bool operator == (const VideoMode& a, const VideoMode& b);
inline bool operator != (const VideoMode& a, const VideoMode& b) {	return !(a==b); }


template<typename t>
t clamp(t v, t min, t max);

template<typename t1, typename t2>
t2 lerp(t1 a, t1 b, t2 k);

struct rgb {
    double r;       // percent
    double g;       // percent
    double b;       // percent
	rgb():r(0),g(0),b(0){}
	rgb(double r, double g, double b):r(r),g(g),b(b){}
	rgb(int r, int g, int b):r(r/255.0),g(g/255.0),b(b/255.0){}
};

rgb rgb_interp(rgb a, rgb b, float k);
rgb rgb_lerp(rgb a, rgb b, float k);

extern "C" {
#endif

#include <SDL/SDL.h>
#include "mytypes.h"
#include "compat.h"
#include "keyboard.h"
#include "control.h"
#include "build.h"

#if !MW_NO_GAME_H
#include "game.h"
#endif
    
//#include "cache1d.h"
#include "csteam.h"

#include "megawang_keys.h"

extern BOOL InMenuLevel;
extern long workshopmap_group_handler;
    
typedef struct _GameDesc {
    unsigned char level;
    unsigned char volume;
    unsigned char player_skill;
    unsigned char monsters_off;
    unsigned char respawn_monsters;
    unsigned char respawn_items;
    unsigned char respawn_inventory;
    unsigned char coop;
    unsigned char marker;
    unsigned char ffire;
    unsigned char fraglimit;
    unsigned long timelimit;
    unsigned char SplitScreen;
} GameDesc;
    
#define MEGAWANG_TITLE_PIC 9400
#define MEGAWANG_STATUS_BAR 9401
#define MEGAWANG_VERSION_STRING "Version 1.0.9"

typedef enum {
    SHADOW_WARRIOR_CLASSIC=0,
    WANTON_DESTRUCTION,
    TWIN_DRAGON
} addon_t;

void swSetAddon(short addonId);
short swGetAddon();
    
int clampi(int v, int min, int max);
unsigned int clampui(unsigned int v, unsigned int min, unsigned int max);
float clampf(float v, float min, float max);
double clampd(double v, double min, double max);
        
long get_modified_time(const char * path) ;
const char* va(const char *format, ...);

int swGetTile(int tile_no, int *width, int *height, void *data);    

void dnGetCurrentVideoMode(VideoMode *vm);
void dnChangeVideoMode(VideoMode *videmode);
void dnSetBrightness(int brightness); /* 0..63 */
int dnGetBrightness();


void dnNewGame(short episode, short _Skill);
void dnQuitGame();
void dnHideMenu();
void dnEnableSound(int enable);
void dnEnableMusic(int enable);
void dnEnableVoice(int enable);
void dnSetSoundVolume(int volume);
void dnSetMusicVolume(int volume);
void dnSetStatusbarMode(int mode);
void swClearScreen();

const char* dnGetGRPName();
const char* dnGetVersion();
    
void dnSetLastSaveSlot(short i);
void dnSetUserMap(const char * mapname);
int dnIsUserMap();
const char* dnGetEpisodeName(int episode);
const char* dnGetLevelName(int level);
void dnSetWorkshopMap(const char * mapname, const char * zipname);
void dnUnsetWorkshopMap();
    
void swGetCloudFilesName(char *names[]);
void swPullCloudFiles();
void swPushCloudFiles();
//void dnOverrideInput(input *loc);
void dnSetMouseSensitivity(int sens);
int dnGetMouseSensitivity();
int dnGetTile(int tile_no, int *width, int *height, void *data);
int dnLoadGame(int slot);
int dnSlotIsEmpty(int save_num);
void dnCaptureScreen();
int dnSaveGame(int slot);
void dnQuitToTitle();
void dnResetMouse();
void dnMouseMove(int xrel, int yrel);
void dnResetMouseWheel();
int dnIsVsyncOn();

void play_mpeg_video(const char * filename);
void play_vpx_video(const char * filename, void (*frame_callback)(int));
void play_intro_sounds(int frame);
    
#if !MW_NO_GAME_H
void dnOverrideInput(SW_PACKET *loc);
void swFnButtons(PLAYERp pp);
#endif

#ifdef __cplusplus
}
#endif


#endif
