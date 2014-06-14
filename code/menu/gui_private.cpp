//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include "gui_private.h"
#include <assert.h>
#include <math.h>
#include <SDL/SDL.h>

#include <Rocket/Core.h>
#include <Rocket/Core/SystemInterface.h>
#include <Rocket/Core/FontDatabase.h>
#include <Rocket/Debugger.h>
#include <Rocket/Controls.h>

#include "ShellSystemInterface.h"
#include "ShellRenderInterfaceOpenGL.h"
#include "ShellFileInterface.h"
#include "ShellOpenGL.h"
#include "FrameAnimationDecoratorInstancer.h"
#include "BackgroundTextureDecoratorInstancer.h"
//#include "csteam.h"


extern "C" {
#include "types.h"
#include "gamedefs.h"
#include "function.h"
#include "config.h"
#include "build.h"
#include "game.h"
#include "menus.h"
#include "fx_man.h"
#include "file_lib.h"
#include "cache1d.h"
#include "text.h"
}

extern "C" {
extern BOOL ExitLevel, NewGame, GamePaused;
extern short Level, Skill;
extern BOOL MusicInitialized, FxInitialized;
extern BOOL ClassicLighting;
extern int32_t r_usenewshading;
extern int32_t r_usetileshades;
}

#ifndef _WIN32
#include <unistd.h>
#endif

struct ConfirmableAction {
    Rocket::Core::ElementDocument *doc;
    Rocket::Core::ElementDocument *back_page;
    virtual void Yes() {};
    virtual void No() {};
    virtual void OnClose() { };
    ConfirmableAction(Rocket::Core::ElementDocument* back_page):back_page(back_page){}
    virtual ~ConfirmableAction(){}
};

void encode(std::string& data) {
    std::string buffer;
    buffer.reserve(data.size());
    for(size_t pos = 0; pos != data.size(); ++pos) {
        switch(data[pos]) {
            case '&':  buffer.append("&amp;");       break;
            case '\"': buffer.append("&quot;");      break;
            case '\'': buffer.append("&apos;");      break;
            case '<':  buffer.append("&lt;");        break;
            case '>':  buffer.append("&gt;");        break;
            default:   buffer.append(&data[pos], 1); break;
        }
    }
    data.swap(buffer);
}

static
Rocket::Core::String KeyDisplayName(const Rocket::Core::String& key_id) {
    if (key_id.Length() == 0) {
        return Rocket::Core::String("None");
    }
    if (key_id[0] == '$') {
        int n;
        char name[20];
        if (sscanf(key_id.CString(), "$mouse%d", &n) == 1) {
            sprintf(name, "Mouse %d", n);
            return Rocket::Core::String(name);
        }
    }
    return key_id;
}

static const Rocket::Core::Vector2i menu_virtual_size(1920, 1080);

static
void LoadFonts(const char* directory)
{
	Rocket::Core::String font_names[2];
    
    font_names[0] = "Averia-Bold.ttf";
    font_names[1] = "go3v2.ttf";

	for (int i = 0; i < sizeof(font_names) / sizeof(Rocket::Core::String); i++)	{
		Rocket::Core::FontDatabase::LoadFontFace(Rocket::Core::String(directory) + font_names[i]);
	}
}

void workshop_refresh_callback (void *p) {
    ((GUI*)p)->InitUserMapsPage("menu-usermaps");
}

void GUI::ShowErrorMessage(const char *page_id, const char *message) {
    struct LobbyErrorMessage: public ConfirmableAction {
        Rocket::Core::Context *context;
        RocketMenuPlugin *menu;
        LobbyErrorMessage(Rocket::Core::ElementDocument *back_page, Rocket::Core::Context *context, RocketMenuPlugin *menu):ConfirmableAction(back_page),context(context), menu(menu) {}
        virtual void Yes() {
        }
    };
    Rocket::Core::ElementDocument *page = m_context->GetDocument(page_id);
    
    ShowConfirmation(new LobbyErrorMessage(page, m_context, m_menu), "ok", message);
}


void GUI::LoadDocuments() {
    Rocket::Core::ElementDocument *cursor = m_context->LoadMouseCursor("data/pointer.rml");
    if (cursor != NULL) {
        cursor->RemoveReference();
    }

	LoadDocument("data/menubg.rml");
	LoadDocument("data/menufg.rml");
	LoadDocument("data/mainmenu.rml");
    LoadDocument("data/ingamemenu.rml");
	LoadDocument("data/options.rml");
    LoadDocument("data/credits.rml");
    LoadDocument("data/loadgame.rml");
    LoadDocument("data/savegame.rml");
	LoadDocument("data/skill.rml");
	LoadDocument("data/episodes.rml");
	LoadDocument("data/credits.rml");
	LoadDocument("data/video.rml");
	LoadDocument("data/game.rml");
	LoadDocument("data/sound.rml");
    LoadDocument("data/mouse.rml");
	LoadDocument("data/keys.rml");
    LoadDocument("data/keyprompt.rml");
    LoadDocument("data/yesno.rml");
    LoadDocument("data/videoconfirm.rml");
    LoadDocument("data/usermaps.rml");
    LoadDocument("data/quitconfirm.rml");
    LoadDocument("data/panorama.rml");
    LoadDocument("data/ok.rml");
}

void GUI::GetAddonDocumentPath(int addon, const Rocket::Core::String& path, char * addonPath) {
    size_t dotPos = path.Find(".");
    const Rocket::Core::String& docName = path.Substring(0, dotPos);
    const Rocket::Core::String& extension = path.Substring(dotPos+1, path.Length() - docName.Length());
    sprintf(addonPath, "%s_%d.%s", docName.CString(), addon, extension.CString());
}


void GUI::LoadDocument(const Rocket::Core::String& path) {
#ifndef _WIN32
//	char data[1024];
//    getcwd(data, 1024);
//    printf("working dir: %s\n",data);
#endif
    Rocket::Core::ElementDocument *document = 0;
    
    char addonPath[1024];
    int addonId = swGetAddon();
    if (addonId != SHADOW_WARRIOR_CLASSIC) {
        GUI::GetAddonDocumentPath(addonId, path, addonPath);
        if (SafeFileExists(addonPath))
            document = m_context->LoadDocument(addonPath);
    }
    
    if (document == 0) {
        document = m_context->LoadDocument(path);
    }

	assert(document != 0);
	document->RemoveReference();
}

void GUI::Reload(){
	m_context->UnloadAllDocuments();
	LoadDocuments();
	m_menu->ShowDocument(m_context, "menu-keys-setup");
}

void GUI::SetActionToConfirm(ConfirmableAction *action) {
    if (m_action_to_confirm != NULL) {
        delete m_action_to_confirm;
    }
    m_action_to_confirm = action;
    m_draw_strips = m_action_to_confirm != NULL;
}

class OffsetAnimation: public BasicAnimationActuator {
private:
    Rocket::Core::Vector2f box;
public:
	OffsetAnimation() {
	}
	virtual void Init(Rocket::Core::Element *element) {
        box.x = element->GetOffsetWidth();
        box.y = element->GetOffsetHeight();
	};
	virtual void Apply(Rocket::Core::Element *e, float position) {
//        Rocket::Core::Property p(position*box.x, Rocket::Core::Property::NUMBER);
        char buf[40];
        sprintf(buf, "%f", position*box.x);
        e->SetProperty("offset-x", buf);
	}
	virtual void Stop(Rocket::Core::Element *e) {};
	virtual void Reset(Rocket::Core::Element *e) {
		e->RemoveProperty("offset-x");
	};
};

GUI::GUI(int width, int height):m_enabled(false),m_width(width),m_height(height),m_enabled_for_current_frame(false),m_waiting_for_key(false),m_action_to_confirm(NULL),m_need_apply_video_mode(false),m_need_apply_vsync(false),m_show_press_enter(true),m_draw_strips(false) {
    
	m_systemInterface = new ShellSystemInterface();
	m_renderInterface = new ShellRenderInterfaceOpenGL();

	m_menu = new RocketMenuPlugin();
	m_menu->SetDelegate(this);

	m_animation = new RocketAnimationPlugin();
	m_menu->SetAnimationPlugin(m_animation);

	Rocket::Core::SetSystemInterface(m_systemInterface);
	Rocket::Core::SetRenderInterface(m_renderInterface);
	Rocket::Core::RegisterPlugin(m_menu);
	Rocket::Core::RegisterPlugin(m_animation);
	Rocket::Core::Initialise();

	Rocket::Core::DecoratorInstancer *decorator_instancer = new FrameAnimationDecoratorInstancer();
	Rocket::Core::Factory::RegisterDecoratorInstancer("frame-animation", decorator_instancer);
    decorator_instancer = new BackgroundTextureDecoratorInstancer();
	Rocket::Core::Factory::RegisterDecoratorInstancer("texture", decorator_instancer);
    
    
	Rocket::Controls::Initialise();

	//Rocket::Core::Factory::RegisterFontEffectInstancer();

	decorator_instancer->RemoveReference();

	LoadFonts("data/assets/");

	m_context = Rocket::Core::CreateContext("menu", menu_virtual_size);
	assert(m_context != 0);

#if USE_ROCKET_DEBUGGER
	Rocket::Debugger::Initialise(m_context);
#endif

	UpdateMenuTransform();

	LoadDocuments();
    
    /* start animation */
    Rocket::Core::ElementDocument *panorama = m_context->GetDocument("panorama");
    m_animation->AnimateElement(panorama->GetElementById("panorama"), AnimationTypeCycle, 40.0f, new OffsetAnimation());
    
	m_menu->ShowDocument(m_context, "menu-main");
	SetupAnimation();
	m_context->ShowMouseCursor(false);
    
    Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-credits");
    Rocket::Core::Element *e = doc->GetElementById("version-string");
    e->SetInnerRML(MEGAWANG_VERSION_STRING);
    
    menu_to_open = "menu-ingame";

    memset(m_cheatbuffer, 0, sizeof(m_cheatbuffer));
    m_cheat_to_do = NULL;
	m_cheat_param = NULL;

	SDL_WM_GrabInput(SDL_GRAB_ON);
    SDL_ShowCursor(SDL_DISABLE);
}

/*
static
void ColorAnimation(Rocket::Core::Element *e, float position, void *ctx) {
	rgb p(235, 156, 9};
	rgb q(255, 255, 255);
	rgb v = rgb_lerp(p, q, position);

	char color[50];
	int r = (int)(v.r*255);
	int g = (int)(v.g*255);
	int b = (int)(v.b*255);
	sprintf(color, "rgb(%d,%d,%d)", r, g, b);
	e->SetProperty("color", color);;
}
*/

void GUI::SetupAnimation() {
//	Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-main");
//	Rocket::Core::Element *e = doc->GetElementById("new-game");
//	m_animation->AnimateElement(e, AnimationTypeBounce, 0.3f, ColorAnimation, NULL);
}

GUI::~GUI() {
	delete m_context;
	Rocket::Core::Shutdown();
	delete m_animation;
	delete m_menu;
	delete m_systemInterface;
	delete m_renderInterface;
}

void GUI::PreModeChange() {
	m_renderInterface->GrabTextures();
}

void GUI::PostModeChange(int width, int height) {
	m_width = width;
	m_height = height;
	m_renderInterface->RestoreTextures();
	UpdateMenuTransform();
}

void GUI::UpdateMenuTransform() {
    m_menu_scale = (float)m_width/(float)menu_virtual_size.x;
    m_menu_offset_y = (m_height/m_menu_scale - 1080)/2;
    m_menu_offset_x = 0;
	m_renderInterface->SetTransform(m_menu_scale, m_menu_offset_x, m_menu_offset_y, m_height);
}

static const float P_Q = 1.0f; /* perspective coeff */

static void
GL_SetUIMode(int width, int height, float menu_scale, float menu_offset_x, float menu_offset_y) {
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glScalef(menu_scale, menu_scale, 1.0f);
	glTranslatef(menu_offset_x, menu_offset_y, 0);
}

static void DrawStrips() {
    glEnable(GL_COLOR);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
    glColor4ub(0, 0, 0, 220);
    
    glBegin(GL_TRIANGLE_STRIP);
    glVertex2d(0, 0);
    glVertex2d(1920, 0);
    glVertex2d(0, -60);
    glVertex2d(1920, -60);
    glEnd();

    glBegin(GL_TRIANGLE_STRIP);
    glVertex2d(0, 1140);
    glVertex2d(1920, 1140);
    glVertex2d(0, 1080);
    glVertex2d(1920, 1080);
    glEnd();
}

void GUI::Render() {
    PLAYERp player = &Player[myconnectindex];
    GLboolean fogOn = glIsEnabled(GL_FOG);
    if (fogOn == GL_TRUE) {
        glDisable(GL_FOG);
    }
    CSTEAM_RunFrame();
    
	//Enable(InMenuLevel && m_enabled_for_current_frame);
	Enable(UsingMenus != 0);
	m_enabled_for_current_frame = false;
	if (m_enabled) {
        GL_SetUIMode(m_width, m_height, m_menu_scale, m_menu_offset_x, m_menu_offset_y);

		if (m_menu_offset_y != 0 && !InGame) { // TODO
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		m_animation->UpdateAnimation();
		m_context->Update();
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
		m_context->Render();
        
//        if (m_draw_strips && m_menu_offset_y > 0) {
//			DrawStrips();
//        }
	}
    
    if (fogOn == GL_TRUE) {
        glEnable(GL_FOG);
    }
}

void GUI::Enable(bool v) {
	if (v != m_enabled) {
		KB_FlushKeyboardQueue();
		KB_ClearKeysDown();
		dnResetMouse();
		m_enabled = v;
		if (m_enabled) {
            printf("Menu enabled\n");
            if (GamePaused) {
                m_menu->ShowDocument(m_context, menu_to_open, false);
            } else {
                m_menu->ShowDocument(m_context, /*m_show_press_enter ? "menu-start" : */"menu-main", false);
            }
            
            Rocket::Core::ElementDocument *menubg = m_context->GetDocument("menu-bg");
            Rocket::Core::ElementDocument *menufg = m_context->GetDocument("menu-fg");
            Rocket::Core::ElementDocument *panorama = m_context->GetDocument("panorama");
            
            menubg->Show();
            menufg->Show();
            panorama->Show();
            
            ResetMouse();
		} else {
			m_context->ShowMouseCursor(false);
		}
	}
}

void GUI::ResetMouse() {
    VideoMode vm = { 0 };
    dnGetCurrentVideoMode(&vm);
    m_mouse_x = vm.width/2;
    m_mouse_y = vm.height/2;
}

template <typename T>
bool in_range(T v, T min, T max, int *offset) {
	*offset = v - min;
	return (v >= min) && (v <= max);
}

static bool
TranslateRange(SDLKey min, SDLKey max, SDLKey key, Rocket::Core::Input::KeyIdentifier base, Rocket::Core::Input::KeyIdentifier *result) {
	int offset;
	if (in_range(min, max, key, &offset)) {
		*result = (Rocket::Core::Input::KeyIdentifier)(base + offset);
		return true;
	}
	return false;
}

static Rocket::Core::Input::KeyIdentifier
translateKey(SDLKey key) {
	using namespace Rocket::Core::Input;
	KeyIdentifier result;
	if (TranslateRange(SDLK_0, SDLK_9, key, KI_0, &result)) { return result; }
	if (TranslateRange(SDLK_F1, SDLK_F12, key, KI_F1, &result)) { return result; }
	if (TranslateRange(SDLK_a, SDLK_z, key, KI_A, &result)) { return result; }
	if (key == SDLK_CAPSLOCK) { return KI_CAPITAL; }
	if (key == SDLK_TAB) { return KI_TAB; }
	return KI_UNKNOWN;
}

#define KEYDOWN(ev, k) (ev->type == SDL_KEYDOWN && ev->key.keysym.sym == k)

struct CheatInfo {
    const char *code;
    void (*CheatInputFunc)(PLAYERp, char *);
};

extern "C" {
    void GodCheat(PLAYERp pp, char *cheat_string);
    void ItemCheat(PLAYERp pp, char *cheat_string);
    void WarpCheat(PLAYERp pp, char *cheat_string);
    void EveryCheatToggle(PLAYERp pp, char *cheat_string);
    void ClipCheat(PLAYERp pp, char *cheat_string);
    void RestartCheat(PLAYERp pp, char *cheat_string);
    void ResCheatOn(PLAYERp pp, char *cheat_string);
    void LocCheat(PLAYERp pp, char *cheat_string);
    void MapCheat(PLAYERp pp, char *cheat_string);
    int cheatcmp(char *str1, char *str2, long len);
};

static
CheatInfo cheatInfo[] =
{
    {"swchan",      GodCheat },
    {"swgimme",     ItemCheat },
    {"swtrek##",    WarpCheat },
    {"swgreed",     EveryCheatToggle },
    {"swghost",     ClipCheat },
    
    {"swstart",     RestartCheat },
    
    {"swres",       ResCheatOn },
    {"swloc",       LocCheat },
    {"swmap",       MapCheat },
};

void GUI::CheckCheat(SDL_Event *ev) {
    if (ev->type == SDL_KEYDOWN) {
        SDLKey key = ev->key.keysym.sym;
        char sym[2] = { 0 };
        if (key >= SDLK_a && key <= SDLK_z) {
            sym[0] = 'a' + (key-SDLK_a);
        }
        if (key >= SDLK_0 && key <= SDLK_9) {
            sym[0] = '0' + (key-SDLK_0);
        }
        
        if (sym[0]) {
            int maxcheat = sizeof(m_cheatbuffer)-2;
            int len = strlen(m_cheatbuffer);
            if (len == maxcheat) {
                memmove(m_cheatbuffer, m_cheatbuffer+1, len);
            }
            strcat(m_cheatbuffer, sym);
            len++;
            if (len > 4) {
                for (int i = 0; i < sizeof(cheatInfo)/sizeof(cheatInfo[0]); i++) {
                    const char *code = cheatInfo[i].code;
                    int codelen = strlen(code);
                    if (len >= codelen) {
                        for (int j = 0; j <= len-codelen; j++) {
                            const char *pattern = &m_cheatbuffer[j];
                            if (cheatcmp((char*)code, (char*)&m_cheatbuffer[j], codelen) == 0) {
                                m_cheat_to_do = &cheatInfo[i];
                                if (m_cheat_param != NULL) {
                                    free(m_cheat_param);
                                }
                                m_cheat_param = strdup(pattern);
                                memset(m_cheatbuffer, 0, sizeof(m_cheatbuffer));
                            }
                        }
                    }
                }
            }
        }
    }
}

void GUI::DoCheat() {
    if (Skill < 3 && !CommEnabled && m_cheat_to_do) {
        m_cheat_to_do->CheatInputFunc(Player, m_cheat_param);
        free(m_cheat_param);
        m_cheat_param = NULL;
    }
    m_cheat_to_do = NULL;
}

bool GUI::InjectEvent(SDL_Event *ev) {
	bool retval = false;

#if 0
	if (ev->type == SDL_KEYDOWN && ev->key.keysym.sym == SDLK_BACKQUOTE) {
		if (isEnabled()) {
			if (!m_menu->GoBack(m_context)) {
				enable(false);
			}
		} else {
			enable(true);
		}
		return true;
	}
#endif
#if !CLASSIC_MENU
	if (KEYDOWN(ev, SDLK_ESCAPE) || (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == 3 && !m_waiting_for_key)) {
        if (m_waiting_for_key) {
            m_context->GetDocument("key-prompt")->Hide();
            m_waiting_for_key = false;
		} else if (UsingMenus) 	{
			m_draw_strips = false;
			if (!m_menu->GoBack(m_context)) {
                menu_to_open = "menu-ingame";
				dnHideMenu();
			}
		}
	}
#endif
#ifdef _DEBUG
	if (KEYDOWN(ev, SDLK_F8)) {
		Reload();
	}
#endif
    
    if (!IsEnabled()) {
        CheckCheat(ev);
    }
	
	if (IsEnabled()) {
		retval = true;

        if (m_waiting_for_key) {
            switch (ev->type) {
                case SDL_MOUSEBUTTONDOWN:
                    m_pressed_key = (dnKey)(DNK_MOUSE0+ev->button.button-1);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (m_pressed_key == DNK_MOUSE0+ev->button.button-1) {
                        HideKeyPrompt();
                        const char *id = dnGetKeyName(m_pressed_key);
                        AssignFunctionKey(m_waiting_menu_item->GetId(), id, m_waiting_slot);
                        InitKeysSetupPage(m_context->GetDocument("menu-keys-setup"));
                    }
                    break;
                case SDL_KEYDOWN:
                   m_pressed_key = ev->key.keysym.sym;
                    break;
                case SDL_KEYUP:
                    if (m_pressed_key == ev->key.keysym.sym) {
                        HideKeyPrompt();
                        const char *id = dnGetKeyName(m_pressed_key);
                        AssignFunctionKey(m_waiting_menu_item->GetId(), id, m_waiting_slot);
                        InitKeysSetupPage(m_context->GetDocument("menu-keys-setup"));
                    }
                    break;
                default:
                    break;
            }
        } else {
            switch (ev->type) {
                case SDL_MOUSEMOTION: {
                    m_context->ShowMouseCursor(true);
                    int xrel = ev->motion.xrel;
                    int yrel = ev->motion.yrel;
                    if (abs(xrel) < 200 && abs(yrel) < 200) { // jerk filter
                        m_mouse_x = clamp(m_mouse_x+xrel, 0, m_width);
						m_mouse_y = clamp(m_mouse_y+yrel, 0, m_height == 1200 ? 1080 : m_height);
                        Rocket::Core::Vector2i mpos = TranslateMouse(m_mouse_x, m_mouse_y);
                        m_context->ProcessMouseMove(mpos.x, mpos.y, 0);
                    }
                    break;
                }
                case SDL_MOUSEBUTTONDOWN:
                    if (ev->button.button < 4) {
                        m_context->ProcessMouseButtonDown(ev->button.button-1, 0);
                    } else if (ev->button.button == 4) {
                        m_context->ProcessMouseWheel(-2, 0);
                    } else if (ev->button.button == 5) {
                        m_context->ProcessMouseWheel(2, 0);
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (ev->button.button < 4) {
                        m_context->ProcessMouseButtonUp(ev->button.button-1, 0);
                    }
                    break;
                case SDL_KEYDOWN:
                    if (ev->key.keysym.sym == SDLK_LEFT) {
                        m_menu->SetPreviousItemValue(m_context);
                    } else if (ev->key.keysym.sym == SDLK_RIGHT) {
                        m_menu->SetNextItemValue(m_context);
                    } else if (ev->key.keysym.sym == SDLK_DOWN) {
                        m_menu->HighlightNextItem(m_context);
                    } else if (ev->key.keysym.sym == SDLK_UP) {
                        m_menu->HighlightPreviousItem(m_context);
                    } else if (ev->key.keysym.sym == SDLK_RETURN) {
                        m_menu->DoItemAction(ItemActionEnter, m_context);
                    } else if (ev->key.keysym.sym == SDLK_DELETE || ev->key.keysym.sym == SDLK_BACKSPACE) {
                        m_menu->DoItemAction(ItemActionClear, m_context);
                    } else {
                        retval = m_context->ProcessKeyDown(translateKey(ev->key.keysym.sym), 0);
                    }
                    break;
                case SDL_KEYUP:
                    retval = m_context->ProcessKeyUp(translateKey(ev->key.keysym.sym), 0);
                    break;
                default:
                    retval = false;
                    break;
            }
        }
	}
	return retval;
}

void GUI::HideKeyPrompt() {
    m_waiting_for_key = false;
    m_context->GetDocument("key-prompt")->Hide();
}

void GUI::TimePulse() {
}

Rocket::Core::Vector2i GUI::TranslateMouse(int x, int y) {
	int nx = (int)(x/m_menu_scale - m_menu_offset_x);
	int ny = (int)(y/m_menu_scale);
	return Rocket::Core::Vector2i(nx, ny);
}

void GUI::EnableForCurrentFrame() {
	m_enabled_for_current_frame = true;
}


static
void SaveVideoMode(VideoMode *vm) {
	/* update config */
	ScreenWidth = vm->width;
	ScreenHeight = vm->height;
	ScreenBPP = vm->bpp;
	ScreenMode = vm->fullscreen;
#if 0 // TODO
    vscrn();
#endif
}

void GUI::ReadChosenSkillAndEpisode(int *pskill, int *pepisode) {
	Rocket::Core::Element *skill = m_menu->GetHighlightedItem(m_context, "menu-skill");
    assert(skill != NULL);
    sscanf(skill->GetId().CString(), "skill-%d", pskill);
    if (swGetAddon() != SHADOW_WARRIOR_CLASSIC) {
        *pepisode = 1; // it's always 1 for addons
        return;
    } else if (dnIsUserMap()) { // usermap
        *pepisode = 0;
        return;
    } else {
        Rocket::Core::Element *episode = m_menu->GetHighlightedItem(m_context, "menu-episodes");
        assert(episode != NULL);
        sscanf(episode->GetId().CString(), "episode-%d", pepisode);
        return;
    }
    assert(false);
}

void GUI::DoCommand(Rocket::Core::Element *element, const Rocket::Core::String& command) {
//	printf("[GUI ] Command: %s\n", command.CString());
	if (command == "game-start") {
		int skill, episode;
		ReadChosenSkillAndEpisode(&skill, &episode);
		Enable(false);
        //ps[myconnectindex].gm &= ~MODE_MENU;
        InMenuLevel = 0; // TODO
		m_menu->ShowDocument(m_context, "menu-ingame", false);
        // for usermap level should and  episode should be 0
		//NewGame(skill, episode, (dnIsUserMap() ? 7 : 0));
        dnNewGame(episode, skill);
	} else if (command == "game-quit") {
        QuitGameCommand(element);
    } else if (command == "game-quit-immediately") {
        dnQuitGame();
    } else if (command == "apply-video-mode") {
		VideoMode vm;
		ReadChosenMode(&vm);
		dnChangeVideoMode(&vm);
		UpdateApplyStatus();
	} else if (command == "load-game") {
        LoadGameCommand(element);
    } else if (command == "save-game") {
        SaveGameCommand(element);
    } else if (command == "game-stop") {
        QuitToTitleCommand(element);
    } else if (command == "confirm-yes") {
        if (m_action_to_confirm != NULL) {
            m_action_to_confirm->Yes();
            if (m_action_to_confirm->back_page != NULL) {
                m_action_to_confirm->back_page->Hide();
            }
            SetActionToConfirm(NULL);
            m_menu->GoBack(m_context);
        }
    } else if (command == "confirm-no") {
        if (m_action_to_confirm != NULL) {
            m_action_to_confirm->No();
            if (m_action_to_confirm->back_page != NULL) {
                m_action_to_confirm->back_page->Hide();
            }
            SetActionToConfirm(NULL);
            m_menu->GoBack(m_context);
        }
    } else if (command == "resume-game") {
        menu_to_open = "menu-ingame";
        dnHideMenu();
    } else if (command == "restore-video-mode") {
        dnChangeVideoMode(&m_backup_video_mode);
        SaveVideoMode(&m_backup_video_mode);
        m_menu->GoBack(m_context, false);
    } else if (command == "restore-video-mode-esc") {
        dnChangeVideoMode(&m_backup_video_mode);
        SaveVideoMode(&m_backup_video_mode);
    } else if (command == "save-video-mode") {
        SaveVideoMode(&m_new_video_mode);
        m_menu->GoBack(m_context, false);
    } else if (command == "open-hub") {
        CSTEAM_OpenCummunityHub();
    } else if (command == "start") {
        m_show_press_enter = false;
    } else if (command == "load-workshopitem") {
        Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-usermaps");
        Rocket::Core::Element *map = m_menu->GetHighlightedItem(doc);
        workshop_item_t item;
        steam_id_t item_id;
        sscanf(map->GetProperty("item-id")->ToString().CString(), "%llu", &item_id);
        CSTEAM_GetWorkshopItemByID(item_id, &item);
        dnSetWorkshopMap(item.itemname, va("/workshop/maps/%llu/%s", item.item_id, item.filename));
        if (workshopmap_group_handler == -1) {
            ShowErrorMessage(doc->GetId().CString(), "The map isn't downloaded yet");
        } else {
            m_menu->ShowDocument(m_context, "menu-skill");
        }
    } else if (command == "load-usermap") {
        Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-usermaps");
        Rocket::Core::Element *map = m_menu->GetHighlightedItem(doc);
        dnSetUserMap(map->GetProperty("map-name")->ToString().CString());
        m_menu->ShowDocument(m_context, "menu-skill");
    }
}

void GUI::QuitToTitleCommand(Rocket::Core::Element *element) {

    struct QuitToTitleAction: public ConfirmableAction {
        Rocket::Core::Context *context;
        RocketMenuPlugin *menu;
        QuitToTitleAction(Rocket::Core::ElementDocument *back_page, Rocket::Core::Context *context, RocketMenuPlugin *menu):ConfirmableAction(back_page),context(context), menu(menu) {}
        virtual void Yes() {
            dnQuitToTitle();
            menu->ShowDocument(context, "menu-main", false);
        }
    };
    Rocket::Core::ElementDocument *page = element->GetOwnerDocument();
    ShowConfirmation(new QuitToTitleAction(page, m_context, m_menu), "yesno", "Quit To Title?");
}

void GUI::QuitGameCommand(Rocket::Core::Element *element) {

    struct QuitGameAction: public ConfirmableAction {
        QuitGameAction(Rocket::Core::ElementDocument *page):ConfirmableAction(page){}
        virtual void Yes() {
            dnQuitGame();
        }
    };
    
    Rocket::Core::ElementDocument *page = element->GetOwnerDocument();
    ShowConfirmation(new QuitGameAction(page), "yesno", "Quit Game?");
}

void GUI::SaveGameCommand(Rocket::Core::Element *element) {
    int slot;
    
    struct SaveGameAction: public ConfirmableAction {
        int slot;
        SaveGameAction(Rocket::Core::ElementDocument *back_page, int slot):ConfirmableAction(back_page),slot(slot){}
        virtual void Yes() {
            dnSaveGame(slot);
        }
    };
        
    if (sscanf(element->GetId().CString(), "slot%d", &slot) == 1) {
        if (dnSlotIsEmpty(slot)) {
            dnSaveGame(slot);
        } else {
            ShowConfirmation(new SaveGameAction(element->GetOwnerDocument(), slot), "yesno", "Save Game?");
        }
        menu_to_open = "menu-ingame";
    }
}

void GUI::LoadGameCommand(Rocket::Core::Element *element) {
    struct LoadGameAction: public ConfirmableAction {
            Rocket::Core::Context *context;
            RocketMenuPlugin *menu;
            int slot;
            LoadGameAction(Rocket::Core::ElementDocument *back_page, Rocket::Core::Context *context, RocketMenuPlugin *menu, int slot):ConfirmableAction(back_page),context(context),menu(menu),slot(slot){}
            virtual void Yes() {
                if (dnLoadGame(slot) == 0) {
                    menu->ShowDocument(context, "menu-ingame", false);
                }
            }
        };
        menu_to_open = "menu-ingame";

#if 1 // TODO
    int slot;
    if (sscanf(element->GetId().CString(), "slot%d", &slot) == 1 && !element->IsClassSet("empty")) {
        if (GamePaused) {
            LoadGameAction *action = new LoadGameAction(element->GetOwnerDocument(), m_context, m_menu, slot);
            ShowConfirmation(action, "yesno", "Load Game?");
        } else {
            if (dnLoadGame(slot) == 0) {
                m_menu->ShowDocument(m_context, "menu-ingame", false);
            }
        }
    }
#endif
}

void GUI::ShowConfirmation(ConfirmableAction *action, const Rocket::Core::String& document, const Rocket::Core::String& text, const Rocket::Core::String& default_option) {
    Rocket::Core::ElementDocument *page = action->back_page;
    Rocket::Core::ElementDocument *doc = m_context->GetDocument(document);
    Rocket::Core::Element *text_element = doc->GetElementById("question");
    
    text_element->SetInnerRML(text);
    
    m_menu->ShowDocument(m_context, document);
    m_menu->HighlightItem(doc, default_option);
    doc->PullToFront();
    page->Show();
    page->PushToBack();
    action->doc = doc;
    SetActionToConfirm(action);
}

void GUI::PopulateOptions(Rocket::Core::Element *menu_item, Rocket::Core::Element *options_element) {
	char buffer[64];
    if (menu_item->GetId() == "video-mode") {
        VideoModeList video_modes;
        dnGetVideoModeList(video_modes);
        for (VideoModeList::iterator i = video_modes.begin(); i != video_modes.end(); i++) {
            Rocket::Core::Element *option = Rocket::Core::Factory::InstanceElement(options_element, "option", "option", Rocket::Core::XMLAttributes());
            sprintf(buffer, "%dx%d", i->width, i->height);
            option->SetInnerRML(buffer);
            sprintf(buffer, "%dx%d@%d", i->width, i->height, i->bpp);
            option->SetId(buffer);
            options_element->AppendChild(option);
            option->RemoveReference();
        }
    } else if (menu_item->GetId() == "max-fps") {
        Rocket::Core::Element *option = Rocket::Core::Factory::InstanceElement(options_element, "option", "option", Rocket::Core::XMLAttributes());
        option->SetInnerRML("OFF");
        option->SetId("fps-0");
        options_element->AppendChild(option);
        option->RemoveReference();
        for (int i = 30; i < 190; i+=10) {
            Rocket::Core::Element *option = Rocket::Core::Factory::InstanceElement(options_element, "option", "option", Rocket::Core::XMLAttributes());
            sprintf(buffer, "%d", i);
            option->SetInnerRML(buffer);
            sprintf(buffer, "fps-%d", i);
            option->SetId(buffer);
            options_element->AppendChild(option);
            option->RemoveReference();
        }
    }
}

void GUI::DidOpenMenuPage(Rocket::Core::ElementDocument *menu_page) {
	Rocket::Core::String page_id(menu_page->GetId());

#if 0 // TODO
    intomenusounds();
#endif
    
	if (page_id != "menu-main" && page_id != "video-confirm" && page_id != "yesno" && !menu_page->HasAttribute("default-item")) {
		m_menu->HighlightItem(m_menu->GetMenuItem(menu_page, 0));
	}
	if (page_id == "menu-video") {
        InitVideoOptionsPage(menu_page);
    } else if (page_id == "menu-sound") {
        InitSoundOptionsPage(menu_page);
    } else if (page_id == "menu-game-options") {
        InitGameOptionsPage(menu_page);
    } else if (page_id == "menu-options") {
        if (m_need_apply_video_mode) {
            ApplyVideoMode(menu_page);
            m_need_apply_video_mode = false;
			m_need_apply_vsync = false;
        } else if (m_need_apply_vsync) {
            Rocket::Core::Element *option_element = m_menu->GetActiveOption(m_menu->GetMenuItem(m_context->GetDocument("menu-video"), "vertical-sync"));
            Rocket::Core::String str;
            
            if (option_element != NULL) {
                str = option_element->GetId();
                
                vsync = str == "vsync-on" ? 1 : 0;

                VideoMode vm;
                dnGetCurrentVideoMode(&vm);
                dnChangeVideoMode(&vm);
                m_need_apply_vsync = false;
#if 0 // TODO
                vscrn();
#endif
            }
        }
    } else if (page_id == "menu-keys-setup") {
        InitKeysSetupPage(menu_page);
    } else if (page_id == "menu-mouse-setup") {
        InitMouseSetupPage(menu_page);
    } else if (page_id == "menu-load" || page_id == "menu-save") {
        InitLoadPage(menu_page);
    } else if (page_id == "menu-usermaps") {
        CSTEAM_UpdateWorkshopItems(workshop_refresh_callback, this);
        InitUserMapsPage(page_id.CString());
    } else if (page_id == "menu-episodes") {
        dnSetUserMap(NULL);
    }
}

void GUI::InitLoadPage(Rocket::Core::ElementDocument *menu_page) {
    for (int i = 9; i >= 0; i--) {
        Rocket::Core::Element *menu_item;
        char id[80];
        sprintf(id, "slot%d", i);
        menu_item = menu_page->GetElementById(id);
        if (menu_item != NULL) {
#if 1 // TODO
            if (!dnSlotIsEmpty(i)) {
                char desc[80];
                LoadGameDescr(i, desc);
                menu_item->SetInnerRML(desc);
                menu_item->SetClass("empty", false);
                menu_item->RemoveAttribute("noanim");
            } else {
                menu_item->SetInnerRML("EMPTY");
                menu_item->SetClass("empty", true);
                menu_item->SetAttribute("noanim", true);
            }
#endif
        }
    }
    m_menu->HighlightItem(menu_page, "slot1");
    m_menu->HighlightItem(menu_page, "slot0"); // load menu bug workaround
}

void GUI::InitMouseSetupPage(Rocket::Core::ElementDocument *menu_page) {
    float sens = (float)clamp(dnGetMouseSensitivity()/65536.0, 0.0, 1.0);
    m_menu->SetRangeValue(m_menu->GetMenuItem(menu_page, "mouse-sens"), sens);
    m_menu->ActivateOption(m_menu->GetMenuItem(menu_page, "invert-y-axis"),
            gs.MouseInvert ? "invert-y-on" : "invert-y-off",
            false);
    
    m_menu->ActivateOption(m_menu->GetMenuItem(menu_page, "lock-y-axis"),
                           gs.MouseAimingOn ? "lock-y-off" : "lock-y-on",
                           false);

}

void GUI::InitVideoOptionsPage(Rocket::Core::ElementDocument *page) {
    char buf[10];
    VideoMode vm;
    dnGetCurrentVideoMode(&vm);
    SetChosenMode(&vm);
    UpdateApplyStatus();

    m_menu->ActivateOption(m_menu->GetMenuItem(page, "texture-filter"), gltexfiltermode < 3 ? "retro" : "smooth", false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "use-voxels"), gs.Voxels ? "voxels-on" : "voxels-off", false);
    m_menu->SetRangeValue(m_menu->GetMenuItem(page, "gamma"), (float)dnGetBrightness(), false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "vertical-sync"), vsync ? "vsync-on" : "vsync-off", false);
    int fps_max = clamp((int)((max_fps+5)/10)*10, 30, 180);
    if (max_fps == 0) {
        fps_max = 0;
    }
    sprintf(buf, "fps-%d", fps_max);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "max-fps"), buf, false);
    m_menu->SetRangeValue(m_menu->GetMenuItem(page, "fov"), xfov*90.0f, false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "classic-lighting"), ClassicLighting ? "classic-lighting-on" : "classic-lighting-off", false);
}

void GUI::InitSoundOptionsPage(Rocket::Core::ElementDocument *page) {
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "sound"), gs.FxOn ? "sound-on" : "sound-off", false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "music"), gs.MusicOn ? "music-on" : "music-off", false);
    m_menu->SetRangeValue(m_menu->GetMenuItem(page, "sound-volume"), (float)gs.SoundVolume, false);
    m_menu->SetRangeValue(m_menu->GetMenuItem(page, "music-volume"), (float)gs.MusicVolume, false);
}

void GUI::InitGameOptionsPage(Rocket::Core::ElementDocument *page) {
    char id[20];

    m_menu->ActivateOption(m_menu->GetMenuItem(page, "crosshair"), gs.Crosshair ? "crosshair-on" : "crosshair-off", false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "level-stats"), gs.Stats ? "stats-on" : "stats-off", false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "auto-aiming"), gs.AutoAim ? "autoaim-on" : "autoaim-off", false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "auto-weapon-switch"), gs.WeaponAutoSwitch ? "autoswitch-on" : "autoswitch-off", false);
    sprintf(id, "statusbar-%d", gs.BorderNum);
    gs.BorderNum = clampi(gs.BorderNum, 0, 2);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "statusbar"), id, false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "use-darts"), useDarts ? "usedarts-on" : "usedarts-off", false);
}

extern "C"
int32 CONFIG_FunctionNameToNum( char * func );

void GUI::InitKeysSetupPage(Rocket::Core::ElementDocument *page) {
    for (int i = 0, n = m_menu->GetNumMenuItems(page); i != n; i++) {
            Rocket::Core::Element *menu_item = m_menu->GetMenuItem(page, i);
            const Rocket::Core::String& gamefunc = menu_item->GetId();
#if 1 // TODO
            int func_num = CONFIG_FunctionNameToNum((char*)gamefunc.CString());
            if (func_num == -1) {
                m_menu->SetKeyChooserValue(menu_item, 0, "???", "???");
                m_menu->SetKeyChooserValue(menu_item, 1, "???", "???");
            } else {
                dnKey key0 = dnGetFunctionBinding(func_num, 0);
                dnKey key1 = dnGetFunctionBinding(func_num, 1);
                const char *key0_name = dnGetKeyName(key0);
                const char *key1_name = dnGetKeyName(key1);
                m_menu->SetKeyChooserValue(menu_item, 0, key0_name, key0_name);
                m_menu->SetKeyChooserValue(menu_item, 1, key1_name, key1_name);
            }
#endif
        }
}


void GUI::InitUserMapsPage(const char * page_id) {
    /*Rocket::Core::ElementDocument *menu_page = m_context->GetDocument("menu-usermaps");
    CACHE1D_FIND_REC * dnGetMapsList();
    CACHE1D_FIND_REC * files = dnGetMapsList();
    Rocket::Core::Element *menu = menu_page->GetElementById("menu");
    char buffer[256];
    if (files) {
        menu->SetInnerRML("");
        while (files) {
            Rocket::Core::Element * record = new Rocket::Core::Element("div");
            sprintf(buffer, "<t>%s</t>", files->name);
            Bstrlwr(buffer);
            record->SetInnerRML(buffer);
	    record->SetProperty("map-name", files->name);
            record->SetAttribute("command", "load-usermap");
            menu->AppendChild(record);
            files = files->next;
            m_menu->SetupMenuItem(record);
        }
        m_menu->HighlightItem(menu_page, menu->GetFirstChild()->GetId());
    }*/
    
    
    Rocket::Core::ElementDocument *menu_page = m_context->GetDocument("menu-usermaps");
    CACHE1D_FIND_REC * dnGetMapsList();
    CACHE1D_FIND_REC * files = dnGetMapsList();
    dnUnsetWorkshopMap();
    Rocket::Core::Element *menu = menu_page->GetElementById("menu");
    bool haveFiles = false;
	Rocket::Core::String firstItem;
    
    int num = CSTEAM_NumWorkshopItems();
	bool workshop_header_added = false;
    menu->SetInnerRML("");
    if (num > 0) {
        for (int i=0; i < num; i++) {
            workshop_item_t item;
            CSTEAM_GetWorkshopItemByIndex(i, &item);
            if (strstr(item.tags, "Singleplayer") == NULL)
                continue;
			if (!workshop_header_added) {
				Rocket::Core::Element *title = new Rocket::Core::Element("div");
				title->SetInnerRML("WORKSHOP MAPS (SINGLEPLAYER)");
				title->SetClass("listhdr", true);
				title->SetAttribute("noanim", "");
				menu->AppendChild(title);
				m_menu->SetupMenuItem(title);
				workshop_header_added = true;
			}
            Rocket::Core::Element * record = new Rocket::Core::Element("div");
            record->SetProperty("item-id", va("%llu", item.item_id));
            std::string item_title(item.title);
            encode(item_title);
            record->SetInnerRML(va("<t>%s (%s)</t>", item_title.c_str(), item.itemname));
            record->SetId(item.itemname);
            record->SetAttribute("command", "load-workshopitem");
            menu->AppendChild(record);
            m_menu->SetupMenuItem(record);
			haveFiles = true;
			if (firstItem == "") {
				firstItem = record->GetId();
			}
        }
    }
    
    if (files) {
        if (num == 0)
            menu->SetInnerRML("");
        haveFiles = true;
        Rocket::Core::Element *title = new Rocket::Core::Element("div");
        title->SetInnerRML("USER MAPS");
        title->SetClass("listhdr", true);
        title->SetAttribute("noanim", "");
        menu->AppendChild(title);
        m_menu->SetupMenuItem(title);
        while (files) {
            Rocket::Core::Element * record = new Rocket::Core::Element("div");
            record->SetProperty("map-name", files->name);
            record->SetInnerRML(va("<t>%s</t>", Bstrlwr(files->name)));
            record->SetId(files->name);
			if (firstItem == "") {
				firstItem = record->GetId();
			}
            record->SetAttribute("command", "load-usermap");
            menu->AppendChild(record);
            files = files->next;
            m_menu->SetupMenuItem(record);
        }
    }
    if (haveFiles) {
        m_menu->HighlightItem(menu_page, firstItem);
	} else {
		Rocket::Core::Element *title = new Rocket::Core::Element("div");
		title->SetInnerRML("No maps found");
		title->SetClass("empty", true);
		title->SetAttribute("noanim", "");
		title->SetId("emptyhdr");
		menu->AppendChild(title);
		m_menu->SetupMenuItem(title);
		m_menu->HighlightItem(menu_page, "emptyhdr");
	}
    
    
}


void GUI::DidCloseMenuPage(Rocket::Core::ElementDocument *menu_page) {
    const Rocket::Core::String& page_id = menu_page->GetId();
	if (page_id == "menu-video") {
    } else if (page_id == "menu-keys-setup") {
        /*
        dnResetMouseKeyBindings();
         */
        ApplyNewKeysSetup(menu_page);
         
    } else if (page_id == "menu-mouse-setup") {
        float sens = m_menu->GetRangeValue(m_menu->GetMenuItem(menu_page, "mouse-sens"));
        int sens_int = clampi((int) (65536.0*sens), 0, 65535);
        dnSetMouseSensitivity(sens_int);
        bool invert_y = m_menu->GetActiveOption(m_menu->GetMenuItem(menu_page, "invert-y-axis"))->GetId() == "invert-y-on";
        gs.MouseInvert = invert_y ? 1 : 0;
        bool lock_y = m_menu->GetActiveOption(m_menu->GetMenuItem(menu_page, "lock-y-axis"))->GetId() == "lock-y-on";
        gs.MouseAimingOn = lock_y ? 0 : 1;
        if (!gs.MouseAimingOn) {
            PLAYERp pp = Player + myconnectindex;
            if (TEST(pp->Flags, PF_LOCK_HORIZ))
            {
                RESET(pp->Flags, PF_LOCK_HORIZ);
                SET(pp->Flags, PF_LOOKING);
            }
        }
    }
}

void GUI::ApplyVideoMode(Rocket::Core::ElementDocument *menu_page) {
    
    struct ApplyVideoModeAction: public ConfirmableAction {
        VideoMode new_mode;
        VideoMode backup_mode;
        ApplyVideoModeAction(Rocket::Core::ElementDocument *back_page):ConfirmableAction(back_page){}
        virtual void Yes() {
            printf("Apply Video Mode\n");
            SaveVideoMode(&new_mode);
        }
        virtual void No() {
            printf("Restore Video Mode\n");
            dnChangeVideoMode(&backup_mode);
            SaveVideoMode(&backup_mode);
        }
    };
    
    Rocket::Core::ElementDocument *doc = menu_page;
    dnGetCurrentVideoMode(&m_backup_video_mode);
    ReadChosenMode(&m_new_video_mode);
    dnChangeVideoMode(&m_new_video_mode);
//    UpdateApplyStatus();
    /* restore mouse pointer */
//    SDL_WM_GrabInput(SDL_GRAB_OFF);
//    SDL_ShowCursor(1);

    /*
    if (1) {
        ApplyVideoModeAction *action = new ApplyVideoModeAction(m_context->GetDocument("menu-options"));
        action->new_mode = vm;
        action->backup_mode = backup_mode;
        ShowConfirmation(action, "video-confirm", "no");
    } else {
        dnGetCurrentVideoMode(&vm);
        SaveVideoMode(&vm);
    }
     */
#if 0 // TODO
    vscrn();
#endif
    m_menu->ShowDocument(m_context, "video-confirm");
    m_menu->HighlightItem(m_context, "video-confirm", "no");
}

void GUI::ApplyNewKeysSetup(Rocket::Core::ElementDocument *menu_page) {
    dnResetBindings();
    for (int i = 0, n = m_menu->GetNumMenuItems(menu_page); i != n; i++) {
        Rocket::Core::Element *menu_item = m_menu->GetMenuItem(menu_page, i);
        const Rocket::Core::String& function_name = menu_item->GetId();
        const Rocket::Core::String& key0_id = m_menu->GetKeyChooserValue(menu_item, 0);
        const Rocket::Core::String& key1_id = m_menu->GetKeyChooserValue(menu_item, 1);

        AssignFunctionKey(function_name, key0_id, 0);
        AssignFunctionKey(function_name, key1_id, 1);
    }
    dnApplyBindings();
}

void GUI::AssignFunctionKey(Rocket::Core::String const & function_name, Rocket::Core::String const & key_id, int slot) {
#if 1 // TODO
    int function_num = CONFIG_FunctionNameToNum((char*)function_name.CString());
    dnKey key = dnGetKeyByName(key_id.CString());
    dnBindFunction(function_num, slot, key);
#endif
}

static
bool ParseVideoMode(const char *str, VideoMode *mode) {
	return sscanf(str, "%dx%d@%d", &mode->width, &mode->height, &mode->bpp) == 3;
}

void GUI::ReadChosenMode(VideoMode *mode) {
	assert(mode != NULL);
	Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-video");
	assert(doc!=NULL);
	Rocket::Core::Element *mode_item = m_menu->GetMenuItem(doc, "video-mode");
	Rocket::Core::Element *fs_item = m_menu->GetMenuItem(doc, "fullscreen-mode");
	assert(mode_item!=NULL && fs_item!=NULL);
	Rocket::Core::Element *mode_option = m_menu->GetActiveOption(mode_item);
	Rocket::Core::Element *fs_option = m_menu->GetActiveOption(fs_item);
	assert(mode_option!=NULL && fs_option!=NULL);
	bool mode_ok = ParseVideoMode(mode_option->GetId().CString(), mode);
	assert(mode_ok);
	mode->fullscreen = (fs_option->GetId() == "fullscreen-on") ? 1 : 0;
}

void GUI::SetChosenMode(const VideoMode *mode) {
	assert(mode != NULL);
	Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-video");
	assert(doc!=NULL);
	Rocket::Core::Element *mode_item = m_menu->GetMenuItem(doc, "video-mode");
	Rocket::Core::Element *fs_item = m_menu->GetMenuItem(doc, "fullscreen-mode");
	assert(mode_item!=NULL && fs_item!=NULL);

	char mode_id[40];
	sprintf(mode_id, "%dx%d@%d", mode->width, mode->height, mode->bpp);
	const char *fs_id = mode->fullscreen ? "fullscreen-on" : "fullscreen-off";
	m_menu->ActivateOption(mode_item, mode_id);
	m_menu->ActivateOption(fs_item, fs_id);
}

void GUI::UpdateApplyStatus() {
	VideoMode chosen_mode, current_mode;
	ReadChosenMode(&chosen_mode);
	dnGetCurrentVideoMode(&current_mode);
	Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-video");
	Rocket::Core::Element *apply_item = m_menu->GetMenuItem(doc, "apply");
    m_need_apply_video_mode = !(chosen_mode == current_mode);
	if (apply_item != NULL) {
		bool apply_disabled = chosen_mode == current_mode;
		apply_item->SetClass("disabled", apply_disabled);
		if (apply_disabled && m_menu->GetHighlightedItem(doc) == apply_item) {
			m_menu->HighlightNextItem(doc);
		}
	}
}

void GUI::DidChangeOptionValue(Rocket::Core::Element *menu_item, Rocket::Core::Element *new_value) {
	Rocket::Core::String item_id(menu_item->GetId());
	Rocket::Core::String value_id(new_value->GetId());
	if (item_id == "video-mode" || item_id == "fullscreen-mode") {
		UpdateApplyStatus();
	}
	if (item_id == "texture-filter") {
		if (value_id == "retro") {
			gltexfiltermode = 2;
            glanisotropy = 1;
			gltexapplyprops();
		} else {
			gltexfiltermode = 5;
            glanisotropy = 0;
			gltexapplyprops();
		}
	} else if (item_id == "use-voxels") {
        gs.Voxels = ( value_id == "voxels-on" ? 1 : 0 );
    } else if (item_id == "classic-lighting") {
        ClassicLighting = (value_id == "classic-lighting-on");
        if (ClassicLighting) {
            r_usenewshading = 2;
            r_usetileshades = 1;
        } else {
            r_usenewshading = 0;
            r_usetileshades = 0;
        }
    } else if (item_id == "sound") {
		dnEnableSound( value_id == "sound-on" ? 1 : 0 );
	} else if (item_id == "music") {
		dnEnableMusic( value_id == "music-on" ? 1 : 0 );
	} else if (item_id == "crosshair") {
		gs.Crosshair = ( value_id == "crosshair-on" ? 1 : 0 );
	} else if (item_id == "level-stats") {
		gs.Stats = ( value_id == "stats-on" ? 1 : 0 );
	} else if (item_id == "auto-aiming") {
		gs.AutoAim = ( value_id == "autoaim-on" ? 1 : 0 );
        if (!CommEnabled && numplayers == 1) {
            if (gs.AutoAim) {
                SET(Player[myconnectindex].Flags, PF_AUTO_AIM);
            } else {
                RESET(Player[myconnectindex].Flags, PF_AUTO_AIM);
            }
        }
	} else if (item_id == "run-key-style") {
		gs.AutoRun = ( value_id == "runkey-classic" ? 1 : 0 );
	} else if (item_id == "auto-weapon-switch") {
        gs.WeaponAutoSwitch = ( value_id == "autoswitch-on" ? 1 : 0);
	} else if (item_id == "statusbar") {
		int v = 2;
		sscanf(value_id.CString(), "statusbar-%d", &v);
        gs.BorderNum = v;
	} else if (item_id == "max-fps") {
        int fps;
        if (sscanf(value_id.CString(), "fps-%d", &fps) == 1) {
            max_fps = fps;
        }
	} else if (item_id == "vertical-sync") {
		int a = vsync ? 1 : 0;
		int b = value_id == "vsync-on" ? 1 : 0;
		m_need_apply_vsync = a != b;
	} else if (item_id == "use-darts") {
        useDarts = (value_id == "usedarts-on") ? 1 : 0;
    }
}

void GUI::DidChangeRangeValue(Rocket::Core::Element *menu_item, float new_value) {
	Rocket::Core::String item_id(menu_item->GetId());
	if (item_id == "gamma") {
		int brightness = clamp((int)new_value, 0, 63);
		dnSetBrightness(brightness);
	} else if (item_id == "sound-volume") {
		dnSetSoundVolume(clamp((int)new_value, 0, 255));
	} else if (item_id == "music-volume") {
		dnSetMusicVolume(clamp((int)new_value, 0, 255));
	} else if (item_id == "fov") {
        xfov = clampf(new_value, 60.0f, 145.0f)/90.0f;
    }
}

void GUI::DidRequestKey(Rocket::Core::Element *menu_item, int slot) {
    m_waiting_for_key = true;
    m_pressed_key = SDLK_UNKNOWN;
    m_waiting_menu_item = menu_item;
    m_waiting_slot = slot;
    m_context->GetDocument("key-prompt")->Show();
}

static
Rocket::Core::String GetEpisodeName(Rocket::Core::Context *context, int episode) {
    Rocket::Core::String r;
    char buffer[20];
    sprintf(buffer, "episode-%d", episode);
    Rocket::Core::ElementDocument *doc = context->GetDocument("menu-episodes");
    Rocket::Core::Element *e = doc->GetElementById(buffer);
    if (e != NULL) {
        e->GetInnerRML(r);
    }
    return r;
}

static
Rocket::Core::String GetSkillName(Rocket::Core::Context *context, int skill) {
    Rocket::Core::String r;
    char buffer[20];
    sprintf(buffer, "skill-%d", skill);
    Rocket::Core::ElementDocument *doc = context->GetDocument("menu-skill");
    Rocket::Core::Element *e = doc->GetElementById(buffer);
    if (e != NULL) {
        e->GetInnerRML(r);
    }
    return r;
}

void GUI::DidActivateItem(Rocket::Core::Element *menu_item) {
    int slot;
        
    if (sscanf(menu_item->GetId().CString(), "slot%d", &slot) == 1) {
        Rocket::Core::ElementDocument *doc = menu_item->GetOwnerDocument();
        Rocket::Core::Element *info_box = doc->GetElementById("info");
        Rocket::Core::Element *thumbnail = doc->GetElementById("thumbnail");
        Rocket::Core::Element *skill = doc->GetElementById("skill");
        Rocket::Core::Element *level = doc->GetElementById("level");
        Rocket::Core::Element *levelno = doc->GetElementById("levelno");

        Rocket::Core::Element *slot9_element = doc->GetElementById("slot9");
        float top = doc->GetElementById("slot0")->GetAbsoluteTop();
        float bottom = slot9_element->GetAbsoluteTop() + slot9_element->GetOffsetHeight();

        if (slot >= 0  && slot < 10) {
            char thumb_rml[50];
            char level_rml[50];
            char descr[80];
            short n_level, n_skill;
            short tile = LoadGameFullHeader(slot, descr, &n_level, &n_skill);
            if (tile != -1 && !menu_item->IsClassSet("empty")) {
                float item_top = menu_item->GetAbsoluteTop();
                float item_height = menu_item->GetOffsetHeight();
                float info_height = info_box->GetOffsetHeight();
                
                float info_top = item_top - (info_height-item_height)/2;
                
                if (info_top < top) {
                    info_top = top;
                }
                
                if (info_top + info_height > bottom) {
                    info_top = bottom - info_height;
                }
                
                info_box->SetProperty("top", Rocket::Core::Property(info_top, Rocket::Core::Property::PX));
                
                info_box->SetProperty("display", "block");
                sprintf(thumb_rml, "<img src=\"tile:%d?%d\"></img>", tile, SDL_GetTicks());
                skill->SetInnerRML(GetSkillName(m_context, n_skill));
                sprintf(level_rml, "%s", /*(int)saveh.levnum+1,*/ dnGetLevelName((int)n_level));
            } else {
                info_box->SetProperty("display", "none");
//                sprintf(thumb_rml, "<img src=\"assets/placeholder.png\"></img>");
//                skill->SetInnerRML("");
//                strcpy(level_rml, "");
            }
            thumbnail->SetInnerRML(thumb_rml);
            level->SetInnerRML(level_rml);
            sprintf(level_rml, "Level %d", n_level);
            levelno->SetInnerRML(level_rml);
        }
    }
}

void GUI::DidClearKeyChooserValue(Rocket::Core::Element *menu_item, int slot) {
    AssignFunctionKey(menu_item->GetId(), "", slot);
}

void GUI::DidClearItem(Rocket::Core::Element *menu_item) {
	if (menu_item->IsClassSet("game-slot")) {
		int slot;
		struct RemoveGameAction: public ConfirmableAction {
			Rocket::Core::Element *element;
			int slot;
			RemoveGameAction(Rocket::Core::ElementDocument *page, Rocket::Core::Element *element, int slot):ConfirmableAction(page), element(element), slot(slot){}
			virtual void Yes() {
				Rocket::Core::ElementDocument *doc = element->GetOwnerDocument();
				Rocket::Core::Element *thumbnail = doc->GetElementById("thumbnail");
				Rocket::Core::Element *skill = doc->GetElementById("skill");
				Rocket::Core::Element *level = doc->GetElementById("level");
				Rocket::Core::String test;
				skill->SetInnerRML("");
				thumbnail->SetInnerRML("<img src=\"assets/placeholder.tga\"></img>");
				level->SetInnerRML("");
				element->SetInnerRML("EMPTY");
				element->SetClass("empty", true);
				char filename[15];
				sprintf(filename, "game%d_%d.sav", slot, swGetAddon());
				CSTEAM_DeleteCloudFile(filename);
                unlink(filename);
			}
		};
		if (sscanf(menu_item->GetId().CString(), "slot%d", &slot) == 1) {
			if (!menu_item->IsClassSet("empty")) {
				Rocket::Core::ElementDocument *page = menu_item->GetOwnerDocument();
				ShowConfirmation(new RemoveGameAction(page, menu_item, slot), "yesno", "Remove saved game?");
			}
		}
	}
}

void GUI::ShowMenuByID(const char * menu_id) {
    menu_to_open = menu_id;
    GamePaused = true;
    UsingMenus = true;
}

void GUI::ShowSaveMenu() {
    ResetKeys();
    KB_ClearKeysDown();
    pMenuClearTextLine(Player + myconnectindex);
    Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-save");
    InitLoadPage(doc);
    ShowMenuByID("menu-save");
}

void GUI::ShowLoadMenu() {
    Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-save");
    InitLoadPage(doc);
    ShowMenuByID("menu-load");
}

void GUI::ShowHelpMenu() {
}

void GUI::ShowSoundMenu() {
    ShowMenuByID("menu-sound");
}

void GUI::ShowGameOptionsMenu() {
     ShowMenuByID("menu-game-options");
}

void GUI::ShowVideoSettingsMenu() {
    ShowMenuByID("menu-video");
}

void GUI::ShowQuitConfirmation() {
    ShowMenuByID("quit-confirm");
}

