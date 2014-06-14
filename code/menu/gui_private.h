//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#ifndef GUI_PRIVATE_H
#define GUI_PRIVATE_H

#include "megawang.h"

#include <SDL/SDL.h>
#undef min
#undef max
#include <Rocket/Core.h>
#include <Rocket/Core/SystemInterface.h>
#include <Rocket/Core/RenderInterface.h>
#include <Rocket/Debugger.h>
#include "RocketMenuPlugin.h"
#include "RocketAnimationPlugin.h"
#include "ShellRenderInterfaceOpenGL.h"

struct ConfirmableAction;
struct CheatInfo;

class GUI: public RocketMenuDelegate {
private:
	int m_width, m_height;
	Rocket::Core::SystemInterface *m_systemInterface;
	ShellRenderInterfaceOpenGL *m_renderInterface;
	Rocket::Core::Context *m_context;
	RocketMenuPlugin *m_menu;
	RocketAnimationPlugin *m_animation;

	bool m_enabled_for_current_frame;
	bool m_enabled;
	float m_menu_scale, m_menu_offset_x, m_menu_offset_y;
	int m_mouse_x, m_mouse_y;

    bool m_waiting_for_key;
    dnKey m_pressed_key;
    Rocket::Core::Element *m_waiting_menu_item;
    int m_waiting_slot;
    
    bool m_need_apply_video_mode, m_need_apply_vsync;
    VideoMode m_backup_video_mode, m_new_video_mode;
    
    bool m_show_press_enter;
    bool m_draw_strips;
    
    ConfirmableAction *m_action_to_confirm;
    
    Rocket::Core::String menu_to_open;
    char m_cheatbuffer[20];
    CheatInfo *m_cheat_to_do;
    char *m_cheat_param;

	void SetupAnimation();
	void LoadDocuments();
	void LoadDocument(const Rocket::Core::String& path);
    void GetAddonDocumentPath(int addon, const Rocket::Core::String& path, char * addonPath);

	void Reload();

	void ReadChosenMode(VideoMode *mode);
	void SetChosenMode(const VideoMode *mode);
	void UpdateApplyStatus();
	void ReadChosenSkillAndEpisode(int *skill, int *episode);
    
    void SetActionToConfirm(ConfirmableAction *action);
    void CheckCheat(SDL_Event *ev);
public:
	GUI(int width, int height);
	~GUI();
	void PreModeChange();
	void PostModeChange(int width, int height);
	void Render();
	void Enable(bool v);
	bool IsEnabled() { return m_enabled; }
	bool InjectEvent(SDL_Event *ev);
	void TimePulse();
	void UpdateMenuTransform();
	void EnableForCurrentFrame();
	Rocket::Core::Vector2i TranslateMouse(int x, int y);

	virtual void DoCommand(Rocket::Core::Element *element, const Rocket::Core::String& command);
	virtual void PopulateOptions(Rocket::Core::Element *menu_item, Rocket::Core::Element *options_element);
	void DidOpenMenuPage(Rocket::Core::ElementDocument *menu_page);
	void DidCloseMenuPage(Rocket::Core::ElementDocument *menu_page);
	void DidChangeOptionValue(Rocket::Core::Element *menu_item, Rocket::Core::Element *new_value);
	virtual void DidChangeRangeValue(Rocket::Core::Element *menu_item, float new_value);
    virtual void DidRequestKey(Rocket::Core::Element *menu_item, int slot);
    virtual void DidActivateItem(Rocket::Core::Element *menu_item);
    virtual void DidClearKeyChooserValue(Rocket::Core::Element *menu_item, int slot);
    virtual void DidClearItem(Rocket::Core::Element *menu_item);

    void ShowErrorMessage(const char *page_id, const char *message);

    void InitKeysSetupPage(Rocket::Core::ElementDocument *page);

    void InitGameOptionsPage(Rocket::Core::ElementDocument *page);

    void InitSoundOptionsPage(Rocket::Core::ElementDocument *page);

    void InitVideoOptionsPage(Rocket::Core::ElementDocument *page);

    void ApplyNewKeysSetup(Rocket::Core::ElementDocument *menu_page);

    void HideKeyPrompt();

    void AssignFunctionKey(Rocket::Core::String const & function_name, Rocket::Core::String const & key_id, int slot);

    void InitMouseSetupPage(Rocket::Core::ElementDocument *menu_page);

    void InitLoadPage(Rocket::Core::ElementDocument *menu_page);
    void InitUserMapsPage(const char * page_id);

    void ShowConfirmation(ConfirmableAction *action, const Rocket::Core::String& document, const Rocket::Core::String& text, const Rocket::Core::String& default_option="yes");

    void LoadGameCommand(Rocket::Core::Element *element);

    void SaveGameCommand(Rocket::Core::Element *element);

    void QuitGameCommand(Rocket::Core::Element *element);

    void QuitToTitleCommand(Rocket::Core::Element *element);

    void ApplyVideoMode(Rocket::Core::ElementDocument *menu_page);

    void ResetMouse();
    
    void ShowMenuByID(const char * menu_id);
    void ShowSaveMenu();
    void ShowLoadMenu();
    void ShowHelpMenu();
    void ShowSoundMenu();
    void ShowVideoSettingsMenu();
    void ShowGameOptionsMenu();
    void ShowQuitConfirmation();
    
    void DoCheat();

};

#endif /* GUI_PRIVATE_H */
