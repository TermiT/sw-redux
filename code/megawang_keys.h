#ifndef MEGAWANG_KEYS_H
#define MEGAWANG_KEYS_H

/* new key binding system */
typedef SDLKey dnKey;
    
#define DN_MAX_KEYS 512
    
#define DNK_FIRST (SDLK_LAST+1)
    
#define DNK_MOUSE0 (DNK_FIRST+0)
#define DNK_MOUSE1 (DNK_FIRST+1)
#define DNK_MOUSE2 (DNK_FIRST+2)
#define DNK_MOUSE3 (DNK_FIRST+3)
#define DNK_MOUSE4 (DNK_FIRST+4)
#define DNK_MOUSE5 (DNK_FIRST+5)
#define DNK_MOUSE6 (DNK_FIRST+6)
#define DNK_MOUSE7 (DNK_FIRST+7)
#define DNK_MOUSE8 (DNK_FIRST+8)
#define DNK_MOUSE9 (DNK_FIRST+9)
#define DNK_MOUSE10 (DNK_FIRST+10)
#define DNK_MOUSE11 (DNK_FIRST+11)
#define DNK_MOUSE12 (DNK_FIRST+12)
#define DNK_MOUSE13 (DNK_FIRST+13)
#define DNK_MOUSE14 (DNK_FIRST+14)
#define DNK_MOUSE15 (DNK_FIRST+15)
#define DNK_MOUSE_LAST DNK_MOUSE15
    
void dnInitKeyNames();
void dnAssignKey(dnKey key, int action);
int  dnGetKeyAction(dnKey key);
const char* dnGetKeyName(dnKey key);
dnKey dnGetKeyByName(const char *keyName);
void dnPressKey(dnKey key);
void dnReleaseKey(dnKey key);
void dnClearKeys();
int  dnKeyState(dnKey key);
void dnGetInput();

void dnResetBindings();
void dnBindFunction(int function, int slot, dnKey key);
SDLKey dnGetFunctionBinding(int function, int slot);
void dnClearFunctionBindings();
void dnApplyBindings();


#endif /* MEGAWANG_KEYS_H */
