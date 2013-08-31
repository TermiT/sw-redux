//
//  megawang.c
//  sw
//
//  Created by serge on 25/4/13.
//  Copyright (c) 2013 serge. All rights reserved.
//

#include <set>
#include <stdlib.h>
#ifdef  __APPLE__
#include <OpenGL/glu.h>
#endif
#ifdef _WIN32
#define APIENTRY __stdcall
#define WINGDIAPI
#define CALLBACK __stdcall
#include <gl/Gl.h>
#include <gl/Glu.h>
#endif
#ifdef __linux
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

#include "smpeg/smpeg.h"
#include "playvpx.h"
#include "megawang.h"

extern "C" {
#include "build.h"
#include "glbuild.h"
#include "hightile_priv.h"
#include "polymosttex_priv.h"
#include "baselayer.h"
#include "function.h"
#include "keyboard.h"
#include "mytypes.h"
#include "control.h"
#include "_control.h"
#include "game.h"
#include "net.h"
#include "menus.h"
#include "sounds.h"
#include "fx_man.h"
#include "cache1d.h"
#include "sounds.h"
#include "cd.h"
#include "text.h"
}

#ifdef _MSC_VER
#define isnan _isnan
#define NAN 0
#include <float.h>
#else
#include <math.h>
#endif

int swGetTile(int tile_no, int *width, int *height, void *data) {
    PTHead * pth = 0;
    int x, y;
    GLsizei w, h;
    unsigned int *buffer;
    pth = PT_GetHead(tile_no, 0, 3, 0);
    if (pth != NULL) {
        GLuint glpic = pth->pic[PTHPIC_BASE]->glpic;
        
        glBindTexture(GL_TEXTURE_2D, glpic);
        
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
        *width = h;
        *height = w;
        
        if (data != NULL) {
            buffer = (unsigned int*)malloc(4*w*h);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, buffer);
            /* transpose pixels     */
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                    ((unsigned int*)data)[y+x*h] = buffer[x+y*w];                }
            }
            free(buffer);
        }
        return 1;
    }
    return 0;
}

short selectedAddonId = 0;

typedef struct  {
    char *name;
    char *grpfile;
    char *script;
}addon_stuct;

addon_stuct official_addons[3] = {
    {"Shadow Warrior Classic", NULL, NULL},
    {"Wanton Destruction", "addons/WT.GRP", "addons/wtcustom.txt"},
    {"Twin Dragon", "addons/TD.grp", "addons/tdcustom.txt"},
};


void swSetAddon(short addonId) {
    if (addonId == SHADOW_WARRIOR_CLASSIC || addonId > 2) return;
    initgroupfile(official_addons[addonId].grpfile);
    LoadCustomInfoFromScript(official_addons[addonId].script);
    wm_setapptitle(official_addons[addonId].name);
    selectedAddonId = addonId;
}

short swGetAddon () {
    return selectedAddonId;
}

template<typename t>
t clamp(t v, t min, t max) {
	if (v > max) {
		return max;
	} else if (v < min) {
		return min;
	}
	return v;
}

template<typename t1, typename t2>
t2 lerp(t1 a, t1 b, t2 k) {
	return (t2)(a + (b-a)*k);
}

extern "C"
int clampi(int v, int min, int max) {
	return clamp(v, min, max);
}

extern "C"
unsigned int clampui(unsigned int v, unsigned int min, unsigned int max) {
	return clamp(v, min, max);
}

extern "C"
float clampf(float v, float min, float max) {
	return clamp(v, min, max);
}

extern "C"
double clampd(double v, double min, double max) {
	return clamp(v, min, max);
}

typedef struct {
    double h;       // angle in degrees
    double s;       // percent
    double v;       // percent
} hsv;

static hsv      rgb2hsv(rgb in);
static rgb      hsv2rgb(hsv in);

hsv rgb2hsv(rgb in)
{
    hsv         out;
    double      min, max, delta;
    
    min = in.r < in.g ? in.r : in.g;
    min = min  < in.b ? min  : in.b;
    
    max = in.r > in.g ? in.r : in.g;
    max = max  > in.b ? max  : in.b;
    
    out.v = max;                                // v
    delta = max - min;
    if( max > 0.0 ) {
        out.s = (delta / max);                  // s
    } else {
        // r = g = b = 0                        // s = 0, v is undefined
        out.s = 0.0;
        out.h = NAN;                            // its now undefined
        return out;
    }
    if( in.r >= max )                           // > is bogus, just keeps compilor happy
        out.h = ( in.g - in.b ) / delta;        // between yellow & magenta
    else
        if( in.g >= max )
            out.h = 2.0 + ( in.b - in.r ) / delta;  // between cyan & yellow
        else
            out.h = 4.0 + ( in.r - in.g ) / delta;  // between magenta & cyan
    
    out.h *= 60.0;                              // degrees
    
    if( out.h < 0.0 )
        out.h += 360.0;
    
    return out;
}

rgb hsv2rgb(hsv in)
{
    double      hh, p, q, t, ff;
    long        i;
    rgb         out;
    
    if(in.s <= 0.0) {       // < is bogus, just shuts up warnings
        if(isnan(in.h)) {   // in.h == NAN
            out.r = in.v;
            out.g = in.v;
            out.b = in.v;
            return out;
        }
        // error - should never happen
        out.r = 0.0;
        out.g = 0.0;
        out.b = 0.0;
        return out;
    }
    hh = in.h;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));
    
    switch(i) {
        case 0:
            out.r = in.v;
            out.g = t;
            out.b = p;
            break;
        case 1:
            out.r = q;
            out.g = in.v;
            out.b = p;
            break;
        case 2:
            out.r = p;
            out.g = in.v;
            out.b = t;
            break;
            
        case 3:
            out.r = p;
            out.g = q;
            out.b = in.v;
            break;
        case 4:
            out.r = t;
            out.g = p;
            out.b = in.v;
            break;
        case 5:
        default:
            out.r = in.v;
            out.g = p;
            out.b = q;
            break;
    }
    return out;
}


rgb rgb_interp(rgb a, rgb b, float k) {
	hsv ha = rgb2hsv(a);
	hsv hb = rgb2hsv(b);
	hsv hr = { lerp(ha.h, hb.h, (double)k), lerp(ha.s, hb.s, (double)k), lerp(ha.v, hb.v, (double)k) };
	return hsv2rgb(hr);
}

rgb rgb_lerp(rgb a, rgb b, float k) {
	rgb result(lerp(a.r, b.r, k), lerp(a.g, b.g, k), lerp(a.b, b.b, k));
	return result;
}

void dnGetVideoModeList(VideoModeList& modes) {
	typedef std::pair<int, int> ScreenSize;
	typedef std::pair<ScreenSize, int> ScreenMode;
	typedef std::set<ScreenMode> ModeSet;
	ModeSet mode_set;
    
	for (int i = 0; i < validmodecnt; i++) {
		int bpp_fs = SDL_VideoModeOK(validmode[i].xdim, validmode[i].ydim, 32, SDL_OPENGL|SDL_FULLSCREEN);
		int bpp_win = SDL_VideoModeOK(validmode[i].xdim, validmode[i].ydim, 32, SDL_OPENGL);
		if (bpp_fs == bpp_win && (bpp_fs == 24 || bpp_fs == 32)) {
			ScreenSize size(validmode[i].xdim, validmode[i].ydim);
			ScreenMode mode(size, bpp_fs);
			mode_set.insert(mode);
			if (mode.second == 32) {
				mode.second = 24;
				if (mode_set.find(mode) != mode_set.end()) {
					mode_set.erase(mode);
				}
			}
		}
	}
	modes.resize(0);
	modes.reserve(mode_set.size());
	for (ModeSet::iterator i = mode_set.begin(); i != mode_set.end(); i++) {
		VideoMode vm;
		ScreenSize size = i->first;
		int bpp = i->second;
		vm.width = size.first;
		vm.height = size.second;
		vm.bpp = i->second;
		vm.fullscreen = 0;
		modes.push_back(vm);
	}
}

extern "C" void polymost_glreset ();
extern "C" char videomodereset;

void dnChangeVideoMode(VideoMode *v) {
	polymost_glreset();
    videomodereset = 1;
	COVERsetgamemode(v->fullscreen?1:0, v->width, v->height, v->bpp);
    SetupAspectRatio();
}

bool operator == (const VideoMode& a, const VideoMode& b) {
	return (a.bpp==b.bpp) && (clamp(a.fullscreen,0,1)==clamp(b.fullscreen,0,1)) && (a.height==b.height) && (a.width==b.width);
}

void swPullCloudFiles() {
    extern char cloudFileNames[MAX_CLOUD_FILES][MAX_CLOUD_FILE_LENGTH];
	for (int i = 0; i < MAX_CLOUD_FILES; i++) {
		//CSTEAM_DownloadFile(cloudFileNames[i]);
	}
}

void swPushCloudFiles() {
    extern char cloudFileNames[MAX_CLOUD_FILES][MAX_CLOUD_FILE_LENGTH];
	for (int i = 0; i < MAX_CLOUD_FILES; i++) {
		CSTEAM_UploadFile(cloudFileNames[i]);
	}
}

void swClearScreen() {
    bglClearColor(0, 0, 0, 1);
    bglClear(GL_COLOR_BUFFER_BIT);
}

const char* dnGetLevelName(int level) {
    return LevelInfo[level].Description;
}

/* new control system */

static uint32_t DN_ButtonState1 = 0;
static uint32_t DN_ButtonState2 = 0;
static uint32_t DN_AutoRelease1 = 0;
static uint32_t DN_AutoRelease2 = 0;

#define DN_BUTTONSET(x,value) \
(\
((x)>31) ?\
(DN_ButtonState2 |= (value<<((x)-32)))  :\
(DN_ButtonState1 |= (value<<(x)))\
)

#define DN_BUTTONCLEAR(x) \
(\
((x)>31) ?\
(DN_ButtonState2 &= (~(1<<((x)-32)))) :\
(DN_ButtonState1 &= (~(1<<(x))))\
)

#define DN_AUTORELEASESET(x,value) \
(\
((x)>31) ?\
(DN_AutoRelease2 |= (value<<((x)-32)))  :\
(DN_AutoRelease1 |= (value<<(x)))\
)


static int dnKeyMapping[DN_MAX_KEYS] = { -1 };
static dnKey dnFuncBindings[64][2] = { };
static const char *dnKeyNames[DN_MAX_KEYS] = { 0 };

void dnInitKeyNames() {
    dnKeyNames[SDLK_UNKNOWN] = "None";
    dnKeyNames[SDLK_BACKSPACE] = "Backspace";
    dnKeyNames[SDLK_TAB] = "Tab";
    dnKeyNames[SDLK_RETURN] = "Enter";
    dnKeyNames[SDLK_ESCAPE] = "Escape";
    dnKeyNames[SDLK_SPACE] = "Space";
    dnKeyNames[SDLK_EXCLAIM] = "!";
    dnKeyNames[SDLK_QUOTEDBL] = "\"";
    dnKeyNames[SDLK_HASH] = "#";
    dnKeyNames[SDLK_DOLLAR] = "$";
    dnKeyNames[SDLK_AMPERSAND] = "&";
    dnKeyNames[SDLK_QUOTE] = "'";
    dnKeyNames[SDLK_LEFTPAREN] = "(";
    dnKeyNames[SDLK_RIGHTPAREN] = ")";
    dnKeyNames[SDLK_ASTERISK] = "*";
    dnKeyNames[SDLK_PLUS] = "+";
    dnKeyNames[SDLK_COMMA] = ",";
    dnKeyNames[SDLK_MINUS] = "-";
    dnKeyNames[SDLK_PERIOD] = ".";
    dnKeyNames[SDLK_SLASH] = "/";
    dnKeyNames[SDLK_0] = "0";
    dnKeyNames[SDLK_1] = "1";
    dnKeyNames[SDLK_2] = "2";
    dnKeyNames[SDLK_3] = "3";
    dnKeyNames[SDLK_4] = "4";
    dnKeyNames[SDLK_5] = "5";
    dnKeyNames[SDLK_6] = "6";
    dnKeyNames[SDLK_7] = "7";
    dnKeyNames[SDLK_8] = "8";
    dnKeyNames[SDLK_9] = "9";
    dnKeyNames[SDLK_COLON] = ":";
    dnKeyNames[SDLK_SEMICOLON] = ";";
    dnKeyNames[SDLK_LESS] = "<";
    dnKeyNames[SDLK_EQUALS] = "=";
    dnKeyNames[SDLK_GREATER] = ">";
    dnKeyNames[SDLK_QUESTION] = "?";
    dnKeyNames[SDLK_AT] = "@";
    dnKeyNames[SDLK_LEFTBRACKET] = "[";
    dnKeyNames[SDLK_BACKSLASH] = "\\";
    dnKeyNames[SDLK_RIGHTBRACKET] = "]";
    //dnKeyNames[SDLK_CARET] = "SDLK_CARET";
    dnKeyNames[SDLK_UNDERSCORE] = "_";
    dnKeyNames[SDLK_BACKQUOTE] = "`";
    dnKeyNames[SDLK_a] = "A";
    dnKeyNames[SDLK_b] = "B";
    dnKeyNames[SDLK_c] = "C";
    dnKeyNames[SDLK_d] = "D";
    dnKeyNames[SDLK_e] = "E";
    dnKeyNames[SDLK_f] = "F";
    dnKeyNames[SDLK_g] = "G";
    dnKeyNames[SDLK_h] = "H";
    dnKeyNames[SDLK_i] = "I";
    dnKeyNames[SDLK_j] = "J";
    dnKeyNames[SDLK_k] = "K";
    dnKeyNames[SDLK_l] = "L";
    dnKeyNames[SDLK_m] = "M";
    dnKeyNames[SDLK_n] = "N";
    dnKeyNames[SDLK_o] = "O";
    dnKeyNames[SDLK_p] = "P";
    dnKeyNames[SDLK_q] = "Q";
    dnKeyNames[SDLK_r] = "R";
    dnKeyNames[SDLK_s] = "S";
    dnKeyNames[SDLK_t] = "T";
    dnKeyNames[SDLK_u] = "U";
    dnKeyNames[SDLK_v] = "V";
    dnKeyNames[SDLK_w] = "W";
    dnKeyNames[SDLK_x] = "X";
    dnKeyNames[SDLK_y] = "Y";
    dnKeyNames[SDLK_z] = "Z";
    dnKeyNames[SDLK_DELETE] = "DEL";
    dnKeyNames[SDLK_KP0] = "KPad 0";
    dnKeyNames[SDLK_KP1] = "KPad 1";
    dnKeyNames[SDLK_KP2] = "KPad 2";
    dnKeyNames[SDLK_KP3] = "KPad 3";
    dnKeyNames[SDLK_KP4] = "KPad 4";
    dnKeyNames[SDLK_KP5] = "KPad 5";
    dnKeyNames[SDLK_KP6] = "KPad 6";
    dnKeyNames[SDLK_KP7] = "KPad 7";
    dnKeyNames[SDLK_KP8] = "KPad 8";
    dnKeyNames[SDLK_KP9] = "KPad 9";
    dnKeyNames[SDLK_KP_PERIOD] = "KP .";
    dnKeyNames[SDLK_KP_DIVIDE] = "KP /";
    dnKeyNames[SDLK_KP_MULTIPLY] = "KP *";
    dnKeyNames[SDLK_KP_MINUS] = "KP -";
    dnKeyNames[SDLK_KP_PLUS] = "KP +";
    dnKeyNames[SDLK_KP_ENTER] = "KP Enter";
    dnKeyNames[SDLK_KP_EQUALS] = "KP =";
    dnKeyNames[SDLK_UP] = "Up";
    dnKeyNames[SDLK_DOWN] = "Down";
    dnKeyNames[SDLK_RIGHT] = "Right";
    dnKeyNames[SDLK_LEFT] = "Left";
    dnKeyNames[SDLK_INSERT] = "Inser";
    dnKeyNames[SDLK_HOME] = "Home";
    dnKeyNames[SDLK_END] = "End";
    dnKeyNames[SDLK_PAGEUP] = "PgUp";
    dnKeyNames[SDLK_PAGEDOWN] = "PgDown";
    dnKeyNames[SDLK_F1] = "F1";
    dnKeyNames[SDLK_F2] = "F2";
    dnKeyNames[SDLK_F3] = "F3";
    dnKeyNames[SDLK_F4] = "F4";
    dnKeyNames[SDLK_F5] = "F5";
    dnKeyNames[SDLK_F6] = "F6";
    dnKeyNames[SDLK_F7] = "F7";
    dnKeyNames[SDLK_F8] = "F8";
    dnKeyNames[SDLK_F9] = "F9";
    dnKeyNames[SDLK_F10] = "F10";
    dnKeyNames[SDLK_F11] = "F11";
    dnKeyNames[SDLK_F12] = "F12";
    dnKeyNames[SDLK_F13] = "F13";
    dnKeyNames[SDLK_F14] = "F14";
    dnKeyNames[SDLK_F15] = "F15";
    dnKeyNames[SDLK_NUMLOCK] = "NumLock";
    dnKeyNames[SDLK_CAPSLOCK] = "CapsLock";
    dnKeyNames[SDLK_SCROLLOCK] = "ScrlLock";
    dnKeyNames[SDLK_RSHIFT] = "RShift";
    dnKeyNames[SDLK_LSHIFT] = "LShift";
    dnKeyNames[SDLK_RCTRL] = "RCtrl";
    dnKeyNames[SDLK_LCTRL] = "LCtrl";
    dnKeyNames[SDLK_RALT] = "RAlt";
    dnKeyNames[SDLK_LALT] = "LAlt";
    dnKeyNames[SDLK_RMETA] = "LMeta";
    dnKeyNames[SDLK_LMETA] = "RMeta";
    dnKeyNames[SDLK_LSUPER] = "LSuper";
    dnKeyNames[SDLK_RSUPER] = "RSuper";
    dnKeyNames[SDLK_MODE] = "Mode";
    dnKeyNames[SDLK_COMPOSE] = "Compose";
    dnKeyNames[SDLK_HELP] = "Help";
    dnKeyNames[SDLK_PRINT] = "Print";
    dnKeyNames[SDLK_SYSREQ] = "SysRq";
    dnKeyNames[SDLK_BREAK] = "Break";
    dnKeyNames[SDLK_MENU] = "Menu";
    dnKeyNames[SDLK_POWER] = "Power";
    dnKeyNames[SDLK_EURO] = "Euro";
    dnKeyNames[SDLK_UNDO] = "Undo";
    dnKeyNames[DNK_MOUSE0] = "Mouse 1";
    dnKeyNames[DNK_MOUSE1] = "Mouse 2";
    dnKeyNames[DNK_MOUSE2] = "Mouse 3";
    dnKeyNames[DNK_MOUSE3] = "Wheel Up";
    dnKeyNames[DNK_MOUSE4] = "Wheel Dn";
    dnKeyNames[DNK_MOUSE5] = "Mouse 6";
    dnKeyNames[DNK_MOUSE6] = "Mouse 7";
    dnKeyNames[DNK_MOUSE7] = "Mouse 8";
    dnKeyNames[DNK_MOUSE8] = "Mouse 9";
    dnKeyNames[DNK_MOUSE9] = "Mouse 10";
    dnKeyNames[DNK_MOUSE10] = "Mouse 11";
    dnKeyNames[DNK_MOUSE11] = "Mouse 12";
    dnKeyNames[DNK_MOUSE12] = "Mouse 13";
    dnKeyNames[DNK_MOUSE13] = "Mouse 14";
    dnKeyNames[DNK_MOUSE14] = "Mouse 15";
    dnKeyNames[DNK_MOUSE15] = "Mouse 16";
}

void dnAssignKey(dnKey key, int action) {
    dnKeyMapping[key] = action;
}

int  dnGetKeyAction(dnKey key) {
    return dnKeyMapping[key];
}

const char* dnGetKeyName(dnKey key) {
    const char *name = dnKeyNames[key];
    if (name == NULL) {
        return "Undef";
    }
    return name;
}

dnKey dnGetKeyByName(const char *keyName) {
	int i;
    for (i = 0; i < DN_MAX_KEYS; i++) {
        const char *name = dnKeyNames[i];
        if (name != NULL && SDL_strcasecmp(keyName, name) == 0) {
            return (dnKey)i;
        }
    }
    return SDLK_UNKNOWN;
}

static int dnIsAutoReleaseKey(dnKey key, int function) {
    switch (key) {
        case DNK_MOUSE3:
        case DNK_MOUSE4:
            return 1;
        default:
            break;
    }
    switch (function) {
        case gamefunc_Map:
        case gamefunc_AutoRun:
        case gamefunc_Map_Follow_Mode:
//        case gamefunc_Next_Track:
        case gamefunc_View_Mode:
        case gamefunc_Quick_Save:
            return 1;
    }
    return 0;
}

void dnPressKey(dnKey key) {
    int function = dnKeyMapping[key];
    if (!CONTROL_CheckRange(function)) {
        DN_BUTTONSET(function, 1);
        if (dnIsAutoReleaseKey(key, function)) {
            DN_AUTORELEASESET(function, 1);
        }
    }
}

void dnReleaseKey(dnKey key) {
    int function = dnKeyMapping[key];
    if (!CONTROL_CheckRange(function)) {
        DN_BUTTONCLEAR(function);
    }
}

void dnClearKeys() {
    DN_ButtonState1 = DN_ButtonState2 = 0;
}

int  dnKeyState(dnKey key) {
    int function = dnKeyMapping[key];
    if (CONTROL_CheckRange(function)) {
        return BUTTON(function);
    }
    return 0;
}

void dnGetInput() {
    CONTROL_ButtonState1 = DN_ButtonState1;
    CONTROL_ButtonState2 = DN_ButtonState2;
    
    DN_ButtonState1 &= ~DN_AutoRelease1;
    DN_ButtonState2 &= ~DN_AutoRelease2;
    
    DN_AutoRelease1 = DN_AutoRelease2 = 0;
}

void dnResetBindings() {
    dnClearFunctionBindings();
    dnBindFunction(gamefunc_Move_Forward, 0, SDLK_w);
    dnBindFunction(gamefunc_Move_Forward, 1, SDLK_UP);
    dnBindFunction(gamefunc_Move_Backward, 0, SDLK_s);
    dnBindFunction(gamefunc_Move_Backward, 1, SDLK_DOWN);
    dnBindFunction(gamefunc_Turn_Left, 1, SDLK_LEFT);
    dnBindFunction(gamefunc_Turn_Right, 1, SDLK_RIGHT);
    dnBindFunction(gamefunc_Strafe, 0, SDLK_LALT);
    dnBindFunction(gamefunc_Strafe, 1, SDLK_RALT);
    dnBindFunction(gamefunc_Fire, 0, (SDLKey)DNK_MOUSE0);
    dnBindFunction(gamefunc_Crouch, 0, SDLK_LCTRL);
    dnBindFunction(gamefunc_Jump, 0, SDLK_SPACE);
    dnBindFunction(gamefunc_Open, 0, SDLK_e);
    dnBindFunction(gamefunc_Open, 1, (SDLKey)DNK_MOUSE2);
    dnBindFunction(gamefunc_Run, 0, SDLK_LSHIFT);
    dnBindFunction(gamefunc_Run, 1, SDLK_RSHIFT);
    dnBindFunction(gamefunc_AutoRun, 0, SDLK_CAPSLOCK);
    dnBindFunction(gamefunc_Strafe_Left, 0, SDLK_a);
    dnBindFunction(gamefunc_Strafe_Right, 0, SDLK_d);
    dnBindFunction(gamefunc_Aim_Up, 0, SDLK_HOME);
    dnBindFunction(gamefunc_Aim_Up, 1, SDLK_KP7);
    dnBindFunction(gamefunc_Aim_Down, 0, SDLK_END);
    dnBindFunction(gamefunc_Aim_Down, 1, SDLK_KP1);
    dnBindFunction(gamefunc_Inventory, 0, SDLK_RETURN);
    dnBindFunction(gamefunc_Inventory, 1, SDLK_KP_ENTER);
    dnBindFunction(gamefunc_Inventory_Left, 0, SDLK_LEFTBRACKET);
    dnBindFunction(gamefunc_Inventory_Right, 0, SDLK_RIGHTBRACKET);
    dnBindFunction(gamefunc_Med_Kit, 0, SDLK_m);
    dnBindFunction(gamefunc_Smoke_Bomb, 0, SDLK_q);
    dnBindFunction(gamefunc_Night_Vision, 0, SDLK_n);
    dnBindFunction(gamefunc_Gas_Bomb, 0, SDLK_g);
    dnBindFunction(gamefunc_Flash_Bomb, 0, SDLK_f);
    dnBindFunction(gamefunc_Caltrops, 0, SDLK_c);
    dnBindFunction(gamefunc_Map, 0, SDLK_TAB);
    dnBindFunction(gamefunc_Center_View, 0, SDLK_KP5);
    dnBindFunction(gamefunc_Next_Weapon, 0, SDLK_QUOTE);
    dnBindFunction(gamefunc_Next_Weapon, 1, (SDLKey)DNK_MOUSE3);
    dnBindFunction(gamefunc_Previous_Weapon, 0, SDLK_SEMICOLON);
    dnBindFunction(gamefunc_Previous_Weapon, 1, (SDLKey)DNK_MOUSE4);
    
    dnBindFunction(gamefunc_Weapon_1, 0, SDLK_1);
    dnBindFunction(gamefunc_Weapon_2, 0, SDLK_2);
    dnBindFunction(gamefunc_Weapon_3, 0, SDLK_3);
    dnBindFunction(gamefunc_Weapon_4, 0, SDLK_4);
    dnBindFunction(gamefunc_Weapon_5, 0, SDLK_5);
    dnBindFunction(gamefunc_Weapon_6, 0, SDLK_6);
    dnBindFunction(gamefunc_Weapon_7, 0, SDLK_7);
    dnBindFunction(gamefunc_Weapon_8, 0, SDLK_8);
    dnBindFunction(gamefunc_Weapon_9, 0, SDLK_9);
    dnBindFunction(gamefunc_Weapon_10, 0, SDLK_0);
    
    dnBindFunction(gamefunc_Shrink_Screen, 0, SDLK_MINUS);
    dnBindFunction(gamefunc_Enlarge_Screen, 0, SDLK_EQUALS);
    dnBindFunction(gamefunc_Map_Follow_Mode, 0, SDLK_f);
    
//    dnBindFunction(gamefunc_Help_Menu, 0, SDLK_F1);
    dnBindFunction(gamefunc_Save_Menu, 0, SDLK_F2);
    dnBindFunction(gamefunc_Load_Menu, 0, SDLK_F3);
    dnBindFunction(gamefunc_Sound_Menu, 0, SDLK_F4);
    dnBindFunction(gamefunc_Next_Track, 0, SDLK_F5);
    dnBindFunction(gamefunc_Quick_Save, 0, SDLK_F6);
    dnBindFunction(gamefunc_View_Mode, 0, SDLK_F7);
    dnBindFunction(gamefunc_Game_Menu, 0, SDLK_F8);
    dnBindFunction(gamefunc_Quick_Load, 0, SDLK_F9);
    dnBindFunction(gamefunc_Quit_Game, 0, SDLK_F10);
    dnBindFunction(gamefunc_Video_Menu, 0, SDLK_F11);
    
    dnBindFunction(gamefunc_Alt_Fire, 0, SDLK_x);
}

void dnBindFunction(int function, int slot, dnKey key) {
	int i;
    if (function >= 0 && function < 64) {
        for (i = 0; i < 64; i++) {
            if (dnFuncBindings[i][0] == key) {
                dnFuncBindings[i][0] = SDLK_UNKNOWN;
            }
            if (dnFuncBindings[i][1] == key) {
                dnFuncBindings[i][1] = SDLK_UNKNOWN;
            }
        }
        dnFuncBindings[function][slot] = key;
    }
}

SDLKey dnGetFunctionBinding(int function, int slot) {
    return dnFuncBindings[function][slot];
}

void dnClearFunctionBindings() {
    memset(dnFuncBindings, 0, sizeof(dnFuncBindings));
}

static void dnPrintBindings() {
	int i;
    for (i = 0; i < 64; i++) {
        dnKey key0 = dnFuncBindings[i][0];
        dnKey key1 = dnFuncBindings[i][1];
        dnKeyMapping[key0] = i;
        dnKeyMapping[key1] = i;
    }
}

void dnApplyBindings() {
	int i;
    memset(dnKeyMapping, -1, sizeof(dnKeyMapping));
    for (i = 0; i < 64; i++) {
        dnKey key0 = dnFuncBindings[i][0];
        dnKey key1 = dnFuncBindings[i][1];
        dnKeyMapping[key0] = i;
        dnKeyMapping[key1] = i;
    }
}

const char* dnGetVersion() {
    return "SW 1.0";
}

void dnSetBrightness(int brightness) {
    gs.Brightness = brightness;
	setbrightness(gs.Brightness>>2, (char*)palette_data, 0);
}

int dnGetBrightness() {
    return gs.Brightness;
	return 0;
}

void dnEnableSound(int enable) {
    if (FXDevice >= 0 && enable != gs.FxOn) {
        gs.FxOn = enable;
        if (gs.FxOn == 0) {
            FX_StopAllSounds();
		} else {
			FX_SetVolume( (short) gs.SoundVolume);
		}
    }
}

extern "C" {
	extern char LevelSong[16];
	extern BYTE RedBookSong[40];
}

void dnEnableMusic(int enable) {
	BOOL bak;
	bak = DemoMode;

	if (enable != gs.MusicOn) {
		gs.MusicOn = enable;
		if (!gs.MusicOn) {
			StopSong();
		} else {
			PlaySong(LevelSong, RedBookSong[Level], TRUE, TRUE);
		}
	}
	DemoMode = bak;
//	if (MusicDevice >= 0 && enable != MusicToggle) {
//		MusicToggle = enable;
//		if (MusicToggle == 0 ) {
//			MusicPause(1);
//		}
//		else {
//			MUSIC_SetVolume( (short)MusicVolume );
//			if (ud.recstat != 2 && ps[myconnectindex].gm&MODE_GAME) {
//				playmusic(&music_fn[0][music_select][0]);
//			}
//			else {
//				playmusic(&env_music_fn[0][0]);
//			}
//			MusicPause(0);
//		}
//	}
}

void dnEnableVoice(int enable) {
//	if (FXDevice >= 0 && enable != VoiceToggle) {
//		VoiceToggle = enable;
//	}
}

void dnSetSoundVolume(int volume) {
	if (volume != gs.SoundVolume) {
		gs.SoundVolume = volume;
        FX_SetVolume( (short) gs.SoundVolume );
	}
}

void dnSetMusicVolume(int volume) {
	if (volume != gs.MusicVolume) {
		gs.MusicVolume = volume;
		if (gs.MusicOn) {
			CD_SetVolume(gs.MusicVolume);
		}
	}
}

void dnSetStatusbarMode(int mode) {
    
}

extern "C" {

extern BOOL QuitFlag;
extern SDL_Surface *sdl_surface;
extern BOOL InMenuLevel, LoadGameOutsideMoveLoop, LoadGameFromDemo, QuitFlag;
extern char SaveGameDescr[10][80];
extern BOOL ExitLevel, NewGame;
extern short Level, Skill;
extern BOOL MusicInitialized, FxInitialized;
extern char UserMapName[80];
extern char LevelSong[16];
extern BYTE RedBookSong[40];
extern int32 ScreenWidth;
extern int32 ScreenHeight;
extern BOOL DebugPanel;

}

void dnQuitGame() {
    if (CommPlayers >= 2)
        MultiPlayQuitFlag = TRUE;
    else
        QuitFlag = TRUE;
    
}

void dnGetCurrentVideoMode(VideoMode *videomode) {
	videomode->width = sdl_surface->w;
	videomode->height = sdl_surface->h;
	videomode->fullscreen = sdl_surface->flags & SDL_FULLSCREEN ? 1 : 0;
	videomode->bpp = sdl_surface->format->BitsPerPixel;
}

/*
 *      MOUSE
 *
 */

typedef struct {
    double x, y;
} vector2;

vector2 pointer = { 0, 0 };
vector2 sensitivity = { 10, 13.0 };
vector2 max_velocity = { 1000.0, 1000.0 };

void dnSetMouseSensitivity(int sens) {
    gs.MouseSpeed = sens;
    sensitivity.x = 0.5 + sens/65536.0*16.0;
    sensitivity.y = sensitivity.x;//*1.2;
}

int dnGetMouseSensitivity() {
    return (int)((sensitivity.x-1.0)*65536.0/16.0);
}

void dnResetMouse() {
//	pointer.x = pointer.y = 0;
    SDL_ShowCursor(0);
}

void dnMouseMove(int xrel, int yrel) {
    pointer.x += xrel;
    pointer.y += yrel;
}

void dnOverrideInput(SW_PACKET *loc) {
    vector2 total_velocity = { 0.0, 0.0 };
    vector2 mouse_velocity;
    
    mouse_velocity.x = pointer.x*sensitivity.x;
    if(gs.MouseAimingOn)  {
        mouse_velocity.y = pointer.y*sensitivity.y;//*(((double)xdim)/((double)ydim));
    }
    
    total_velocity.x += mouse_velocity.x;
    total_velocity.y += mouse_velocity.y;
    
    total_velocity.x = clampd(total_velocity.x, -max_velocity.x, max_velocity.x);
    total_velocity.y = clampd(total_velocity.y, -max_velocity.y, max_velocity.y);
        
    loc->angvel = (signed char) clampi((int)(128*total_velocity.x/max_velocity.x) + loc->angvel, -127, 127);
    if (loc->angvel != 0) {
        pointer.x = 0.0;
    }
    
    loc->aimvel = (signed char) clampi((int)(128*total_velocity.y/max_velocity.y) + loc->aimvel, -127, 127);
    if (loc->aimvel != 0) {
        pointer.y = 0.0;
    }
    if (!gs.MouseInvert) {
        loc->aimvel *= -1;
    }
    
}

extern "C" {
    int ExitToMenu = 0;
}

void dnHideMenu() {
    UsingMenus = FALSE;
    ResumeGame();
}

int dnLoadGame(int load_num) {
    int r = 0;
    if (InMenuLevel || DemoMode || DemoPlaying) {
        if ((r = LoadGame(load_num)) == -1) {
            return r;
        }
        ExitMenus();
        ExitLevel = TRUE;
        LoadGameOutsideMoveLoop = TRUE;
        if (DemoMode || DemoPlaying) {
            LoadGameFromDemo = TRUE;
        }
        return -1;
    }
    PauseAction();
    if (LoadGame(load_num) == -1) {
        ResumeAction();
        return -1;
    }
    
    ready2send = 1;
    ExitMenus();

    if (DemoMode) {
        ExitLevel = TRUE;
        DemoPlaying = FALSE;
    }
    return r;
}


short lastsavedpos = -1;
int dnSaveGame(int slot) {

    int r;
    time_t now;
    struct tm *localtm;
    now = time(0);
    localtm = localtime(&now);
    
    
    strftime(&SaveGameDescr[slot][0], 18, "%H:%M, %d %b",  localtm);
    lastsavedpos = slot;
    PauseAction();
    DebugPanel = 1;
    r = SaveGame(slot);
    DebugPanel = 0;
    ResumeAction();
    dnHideMenu();
    PutStringInfoLine(Player + myconnectindex, "Game saved");
    return r;
}

int dnSlotIsEmpty(int save_num) {
    char game_name[80];
    FILE *fil;
    sprintf(game_name,"game%d_%d.sav",save_num, swGetAddon());
    if ((fil = fopen(game_name, "r")) == NULL) {
        return 1;
    }
    fclose(fil);
    return 0;
}

void dnNewGame(short episode, short _Skill) {
    PLAYERp pp = Player + screenpeek;
    int handle = 0;
    long zero = 0;
    
    // always assumed that a demo is playing
    
    ready2send = 0;
    Skill = _Skill;
    Level = episode ? 5:1;
    
    ExitMenus();
    DemoPlaying = FALSE;
    ExitLevel = TRUE;
    NewGame = TRUE;
    DemoMode = FALSE;
    CameraTestMode = FALSE;
    
    //InitNewGame();
    
    if (Skill == 0)
        handle = PlaySound(DIGI_TAUNTAI3,&zero,&zero,&zero,v3df_none);
    else
        if(Skill == 1)
            handle = PlaySound(DIGI_NOFEAR,&zero,&zero,&zero,v3df_none);
        else
            if(Skill == 2)
                handle = PlaySound(DIGI_WHOWANTSWANG,&zero,&zero,&zero,v3df_none);
            else
                if(Skill == 3)
                    handle = PlaySound(DIGI_NOPAIN,&zero,&zero,&zero,v3df_none);
    
    if (handle > FX_Ok)
        while (FX_SoundActive(handle))
			handleevents();
    
}
void dnQuitToTitle() {
    ready2send = 0;
    ExitMenus();
    DemoMode = TRUE;
    ExitLevel = TRUE;
    ExitToMenu = TRUE;
}

void swFnButtons(PLAYERp pp){
    extern BOOL GamePaused;
    if (numplayers <= 1) {
        if (BUTTON(gamefunc_Save_Menu)) {
            BUTTONCLEAR(gamefunc_Save_Menu);
            if (!TEST(pp->Flags, PF_DEAD))
                GUI_ShowSaveMenu();
        }
        
        if (BUTTON(gamefunc_Load_Menu)) {
            BUTTONCLEAR(gamefunc_Load_Menu);
            GUI_ShowLoadMenu();
        }
    }
    
    if (BUTTON(gamefunc_Sound_Menu)) {
        BUTTONCLEAR(gamefunc_Sound_Menu);
        GUI_ShowSoundMenu();
    }
	if (BUTTON(gamefunc_Next_Track)) {
        PlaySong(LevelSong, RedBookSong[4+RANDOM_RANGE(10)], TRUE, TRUE);
    }
    
    if (numplayers <= 1) {
        if (BUTTON(gamefunc_Quick_Save) && !BUTTONHELD(gamefunc_Quick_Save)) {
            BUTTONCLEAR(gamefunc_Quick_Save);
            if (!TEST(pp->Flags, PF_DEAD)) {
                if(lastsavedpos != -1) {
                    ResetKeys();
                    KB_ClearKeysDown();
                    pMenuClearTextLine(Player + myconnectindex);
                    dnSaveGame(lastsavedpos);
                } else {
                    GUI_ShowSaveMenu();
                }
            }
        }
    }
    
    if (BUTTON(gamefunc_View_Mode)) {
        if (TEST(pp->Flags, PF_VIEW_FROM_OUTSIDE))
        {
            RESET(pp->Flags, PF_VIEW_FROM_OUTSIDE);
        }
        else
        {
            SET(pp->Flags, PF_VIEW_FROM_OUTSIDE);
            pp->camera_dist = 0;
        }
        BUTTONCLEAR(gamefunc_View_Mode);
    }
    
    if (BUTTON(gamefunc_Game_Menu)) {
        BUTTONCLEAR(gamefunc_Game_Menu);
        GUI_ShowGameOptionsMenu();
    }

    if (numplayers <= 1) {
        if(BUTTON(gamefunc_Quick_Load)) {
            if (lastsavedpos != -1)
                dnLoadGame(lastsavedpos);
            BUTTONCLEAR(gamefunc_Quick_Load);
        }
    }

    if (BUTTON(gamefunc_Quit_Game)) {
        BUTTONCLEAR(gamefunc_Quit_Game);
        GUI_ShowQuitConfirmation();
    }
    
    if (BUTTON(gamefunc_Video_Menu)) {
        BUTTONCLEAR(gamefunc_Video_Menu);
        GUI_ShowVideoSettingsMenu();
    }
}

// from duke nukem menu.c
static CACHE1D_FIND_REC *finddirs=NULL, *findfiles=NULL, *finddirshigh=NULL, *findfileshigh=NULL;
static int numdirs=0, numfiles=0;
static int currentlist=0;


void clearfilenames(void)
{
	klistfree(finddirs);
	klistfree(findfiles);
	finddirs = findfiles = NULL;
	numfiles = numdirs = 0;
}

int getfilenames(char *path, char kind[])
{
	CACHE1D_FIND_REC *r;
	
	clearfilenames();
	finddirs = klistpath(path,"*",CACHE1D_FIND_DIR);
	findfiles = klistpath(path,kind,CACHE1D_FIND_FILE);
	for (r = finddirs; r; r=r->next) numdirs++;
	for (r = findfiles; r; r=r->next) numfiles++;
	
	finddirshigh = finddirs;
	findfileshigh = findfiles;
	currentlist = 0;
	if (findfileshigh) currentlist = 1;
	
	return(0);
}


CACHE1D_FIND_REC *dnGetMapsList() {
    pathsearchmode = 1;
    getfilenames("maps", "*.map");
    return findfileshigh;
}

int dnIsUserMap() {
    return UserMapName[0] != '\0';
}

void dnSetUserMap(const char * mapname) {
    if (mapname == NULL) {
        UserMapName[0] = 0;
    } else {
        sprintf(UserMapName, "maps/%s", mapname);
    }
}

extern "C"
int dnFPS = 0;

extern "C"
void dnCalcFPS() {
	static Uint32 prev_time = 0;
	static Uint32 frame_counter = 0;
	int current_time;
	frame_counter++;
	if (frame_counter == 50) {
		frame_counter = 0;
		current_time = Sys_GetTicks();
		if (prev_time != 0) {
			dnFPS = (int)( 50000.0 / (current_time-prev_time) );
		}
		prev_time = current_time;
	}
}


// video playback


int APIENTRY gluBuild2DMipmaps (
                                GLenum      target,
                                GLint       components,
                                GLint       width,
                                GLint       height,
                                GLenum      format,
                                GLenum      type,
                                const void  *data);

void DrawIMG(SDL_Surface *img, int x, int y)
{
    GLuint texture;
    
    glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    
    glGenTextures(1,&texture);
    glBindTexture(GL_TEXTURE_2D,texture);
    
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,0);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,0);
    
    gluBuild2DMipmaps(GL_TEXTURE_2D, 4, img->w, img->h, GL_RGBA, GL_UNSIGNED_BYTE, img->pixels);
    
    
    glBindTexture(GL_TEXTURE_2D, texture);
    
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f( -1, -1,  0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(  1, -1,  0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(  1,  1,  0.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f( -1,  1,  0.0f);
    glEnd();
    
    glDeleteTextures(1, &texture);
}

static
void smpeg_callback(SDL_Surface* dst, int x, int y,
                    unsigned int w, unsigned int h) {
    
}


void play_mpeg_video(const char * filename) {
    
    SMPEG *movie = NULL;
    SDL_Surface *movieSurface = 0;
    SMPEGstatus mpgStatus;
    SMPEG_Info movieInfo;
	char *error;
	int done;
    
    movie = SMPEG_new(filename, &movieInfo, true);
    
    error = SMPEG_error(movie);
    
    if( error != NULL || movie == NULL ) {
        printf( "Error loading MPEG: %s\n", error );
        return;
    }
    
    movieSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, 512, 512, 32, 0xFF, 0xFF00, 0xFF0000, 0);
    
    SMPEG_setdisplay(movie, movieSurface, 0, &smpeg_callback);
    SDL_ShowCursor(SDL_DISABLE);
    
    SMPEG_play(movie);
    SMPEG_getinfo(movie, &movieInfo);
    
    glEnable(GL_TEXTURE_2D);
	glClearColor(1, 0, 1, 1);
    
    glDisable(GL_DEPTH_TEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    
	glViewport(0, 0, ScreenWidth, ScreenHeight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
    glOrtho(-1, 1, 1, -1, 0, 1);
    glScalef((16.0f/9.0f)/(ScreenWidth/(float)ScreenHeight), 1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
    
    done = 0;
    
    while(done == 0) {
        SDL_Event event;
        while (SDL_PollEvent(&event)){
            if (event.type == SDL_QUIT|| event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN)  {
                done = 1;
            }
        }
        glClear(GL_COLOR_BUFFER_BIT);
        
        DrawIMG(movieSurface, 0, 0);
        mpgStatus = SMPEG_status(movie);
        if(mpgStatus != SMPEG_PLAYING) {
            done = 1;
        }
        SDL_GL_SwapBuffers();
        
    }
    
    SMPEG_stop(movie);
    SMPEG_delete(movie);
    movie = NULL;
    SDL_FreeSurface(movieSurface);
}


void gfx_tex_blit(int tid) {
    static float coords[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
    static float verts[]  = {-1, 1, 1, 1,-1,-1, 1,-1 };
    
    glBindTexture(GL_TEXTURE_2D, tid);
    glVertexPointer(2, GL_FLOAT, 0, verts);
    glTexCoordPointer(2, GL_FLOAT, 0, coords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


void play_vpx_video(const char * filename, void (*frame_callback)(int)) {
    
    int ww = ScreenWidth;
    int hh = ScreenHeight;
    int frame = 0;
    
    SDL_ShowCursor(SDL_DISABLE);
    SDL_Surface *screen =  SDL_CreateRGBSurface(SDL_SWSURFACE, ww, hh, 32, 0xFF, 0xFF00, 0xFF0000, 0);
    
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glScalef(1, (9.0f/16.0f)/(ScreenHeight/(float)ScreenWidth), 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    int ticks = SDL_GetTicks();
    
    Vpxdata data;
    playvpx_init(&data,filename);
    int done = 0;
    while(playvpx_loop(&data) && !done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)){
            if (event.type == SDL_QUIT|| event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN)  {
                done = 1;
            }
        }
        GLuint tex = playvpx_get_texture(&data);
        if (!tex) { continue; }
        gfx_tex_blit(tex);
        SDL_GL_SwapBuffers();
        Sys_ThrottleFPS(40);
        glClear(GL_COLOR_BUFFER_BIT);
        frame++;
        if (frame_callback != NULL) {
            frame_callback(frame);
        }
        
    }
    
    playvpx_deinit(&data);
    printf("ticks: %d\n",SDL_GetTicks()-ticks);
    SDL_FreeSurface(screen);
}


void play_intro_sounds(int frame) {
    long zero=0;
    switch (frame) {
        case 1:
            PlaySound(DIGI_NOMESSWITHWANG,&zero,&zero,&zero,v3df_none);
            break;
        case 160:
            PlaySound(DIGI_INTRO_SLASH,&zero,&zero,&zero,v3df_none);
            break;
        case 190:
            PlaySound(DIGI_INTRO_WHIRL,&zero,&zero,&zero,v3df_none);
            break;
        default:
            break;
    }
}


