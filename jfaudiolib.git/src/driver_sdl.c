/*
 Copyright (C) 2009 Jonathon Fowler <jf@jonof.id.au>
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 
 See the GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
 */
 
/**
 * libSDL output driver for MultiVoc
 */


#if defined(SDL_FRAMEWORK)
# include <SDL/SDL.h>
# if defined(_WIN32) || defined(GEKKO)
#  include <SDL/SDL_mixer.h>
# else
#  include <SDL_mixer/SDL_mixer.h>
# endif
# include <SDL/SDL_thread.h>
#else
# include "SDL.h"
# include "SDL_mixer.h"
# include "SDL_thread.h"
#endif
#include "driver_sdl.h"
#include "multivoc.h"


#ifndef UNREFERENCED_PARAMETER
# define UNREFERENCED_PARAMETER(x) x=x
#endif

enum {
   SDLErr_Warning = -2,
   SDLErr_Error   = -1,
   SDLErr_Ok      = 0,
   SDLErr_Uninitialised,
   SDLErr_InitSubSystem,
   SDLErr_OpenAudio,
   SDLErr_OpenOGG,
   SDLErr_PlayOGG,
};

static int32_t ErrorCode = SDLErr_Ok;
static int32_t Initialised = 0;
static int32_t Playing = 0;
// static int32_t StartedSDL = -1;

static char *MixBuffer = 0;
static int32_t MixBufferSize = 0;
static int32_t MixBufferCount = 0;
static int32_t MixBufferCurrent = 0;
static int32_t MixBufferUsed = 0;
static void ( *MixCallBack )( void ) = 0;

static Mix_Chunk *DummyChunk = NULL;
static uint8_t *DummyBuffer = NULL;
static int32_t InterruptsDisabled = 0;
static SDL_mutex *EffectFence;

static void fillData(int32_t chan, void *ptr, int32_t remaining, void *udata)
{
    int32_t len;
    char *sptr;

    UNREFERENCED_PARAMETER(chan);
    UNREFERENCED_PARAMETER(udata);
    
    if (!MixBuffer || !MixCallBack)
      return;

    SDL_LockMutex(EffectFence);

    while (remaining > 0) {
        if (MixBufferUsed == MixBufferSize) {
            MixCallBack();
            
            MixBufferUsed = 0;
            MixBufferCurrent++;
            if (MixBufferCurrent >= MixBufferCount) {
                MixBufferCurrent -= MixBufferCount;
            }
        }
        
        while (remaining > 0 && MixBufferUsed < MixBufferSize) {
            sptr = MixBuffer + (MixBufferCurrent * MixBufferSize) + MixBufferUsed;
            
            len = MixBufferSize - MixBufferUsed;
            if (remaining < len) {
                len = remaining;
            }
            
            memcpy(ptr, sptr, len);
            
            ptr = (void *)((uintptr_t)(ptr) + len);
            MixBufferUsed += len;
            remaining -= len;
        }
    }

    SDL_UnlockMutex(EffectFence);
}


int32_t SDLDrv_GetError(void)
{
    return ErrorCode;
}

const char *SDLDrv_ErrorString( int32_t ErrorNumber )
{
    const char *ErrorString;
    
    switch( ErrorNumber ) {
        case SDLErr_Warning :
        case SDLErr_Error :
            ErrorString = SDLDrv_ErrorString( ErrorCode );
            break;

        case SDLErr_Ok :
            ErrorString = "SDL Audio ok.";
            break;
            
        case SDLErr_Uninitialised:
            ErrorString = "SDL Audio uninitialised.";
            break;

        case SDLErr_InitSubSystem:
            ErrorString = "SDL Audio: error in Init or InitSubSystem.";
            break;

        case SDLErr_OpenAudio:
            ErrorString = "SDL Audio: error in OpenAudio.";
            break;

        case SDLErr_OpenOGG:
            ErrorString = "SDL Audio: error loading OGG file.";
            break;

        case SDLErr_PlayOGG:
            ErrorString = "SDL Audio: error playing OGG file.";
            break;


        default:
            ErrorString = "Unknown SDL Audio error code.";
            break;
    }

    return ErrorString;
}

int32_t SDLDrv_PCM_Init(int32_t *mixrate, int32_t *numchannels, int32_t *samplebits, void * initdata)
{
    int32_t err = 0;
    int32_t chunksize;
    uint16_t fmt;

    UNREFERENCED_PARAMETER(numchannels);
    UNREFERENCED_PARAMETER(initdata);

    if (Initialised) {
        SDLDrv_PCM_Shutdown();
    }

    chunksize = 512;

    if (*mixrate >= 16000) chunksize *= 2;
    if (*mixrate >= 32000) chunksize *= 2;

    err = Mix_OpenAudio(*mixrate, (*samplebits == 8) ? AUDIO_U8 : AUDIO_S16SYS, *numchannels, chunksize);

    if (err < 0) {
        ErrorCode = SDLErr_OpenAudio;
        return SDLErr_Error;
    }

    if (Mix_QuerySpec(mixrate, &fmt, numchannels))
    {
        if (fmt == AUDIO_U8 || fmt == AUDIO_S8) *samplebits = 8;
        else *samplebits = 16;
    }

    //Mix_SetPostMix(fillData, NULL);

    EffectFence = SDL_CreateMutex();

    // channel 0 and 1 are actual sounds
    // dummy channel 2 runs our fillData() callback as an effect
    Mix_RegisterEffect(2, fillData, NULL, NULL);

    DummyBuffer = (uint8_t *) malloc(chunksize);
    memset(DummyBuffer, 0, chunksize);

    DummyChunk = Mix_QuickLoad_RAW(DummyBuffer, chunksize);

    Mix_PlayChannel(2, DummyChunk, -1);

    Initialised = 1;

    return SDLErr_Ok;
}

void SDLDrv_PCM_Shutdown(void)
{
    if (!Initialised)
        return;
    else Mix_HaltChannel(-1);

    if (DummyChunk != NULL)
    {
        Mix_FreeChunk(DummyChunk);
        DummyChunk = NULL;
    }

    if (DummyBuffer  != NULL)
    {
        free(DummyBuffer);
        DummyBuffer = NULL;
    }

    Mix_CloseAudio();

    SDL_DestroyMutex(EffectFence);

    Initialised = 0;
}

int32_t SDLDrv_PCM_BeginPlayback(char *BufferStart, int32_t BufferSize,
                        int32_t NumDivisions, void ( *CallBackFunc )( void ) )
{
    if (!Initialised) {
        ErrorCode = SDLErr_Uninitialised;
        return SDLErr_Error;
    }
    
    if (Playing) {
        SDLDrv_PCM_StopPlayback();
    }
    
    MixBuffer = BufferStart;
    MixBufferSize = BufferSize;
    MixBufferCount = NumDivisions;
    MixBufferCurrent = 0;
    MixBufferUsed = 0;
    MixCallBack = CallBackFunc;
    
    // prime the buffer
    MixCallBack();
    
    Mix_Resume(-1);
    
    Playing = 1;
    
    return SDLErr_Ok;
}

void SDLDrv_PCM_StopPlayback(void)
{
    if (!Initialised || !Playing) {
        return;
    }

    Mix_Pause(-1);
    
    Playing = 0;
}

void SDLDrv_PCM_Lock(void)
{
        if (InterruptsDisabled++)
            return;
    
        SDL_LockMutex(EffectFence);
}

void SDLDrv_PCM_Unlock(void)
{
        if (--InterruptsDisabled)
            return;
    
        SDL_UnlockMutex(EffectFence);
}

static Mix_Music *music = NULL;

int  SDLDrv_CD_Init(void) {
    music = NULL;
    return SDLErr_Ok;
}

void SDLDrv_CD_Shutdown(void) {
    SDLDrv_CD_Stop();
}

int  CD_PlayFile(const char * filename, int loop) {

    if (music != NULL) {
        SDLDrv_CD_Stop();
    }

    music = Mix_LoadMUS(filename);

    if(music == NULL) {
        printf("Unable to load OGG file: %s\n", Mix_GetError());
        ErrorCode = SDLErr_OpenOGG;
        return SDLErr_Error;

    }
    if(Mix_PlayMusic(music, loop ? -1 : 0) == -1) {
        printf("Unable to playback OGG file: %s\n", Mix_GetError());
        ErrorCode = SDLErr_PlayOGG;
        return SDLErr_Error;
    }
    return SDLErr_Ok;
}


int CD_PlayByName(const char *songname, const char *folder, int loop) {
    char filename[200];
    sprintf(filename, "%s/%s", folder, songname); /* MEGATON specific */
    return CD_PlayFile(filename, loop);
}

int  SDLDrv_CD_Play(int track, int loop) {
    char filename[200];
    sprintf(filename, "classic/MUSIC/Track%02d.ogg", track); /* SW REDUX specific */
    return CD_PlayFile(filename, loop);
}


void SDLDrv_CD_Stop(void) {
         Mix_HaltMusic();
         music = NULL;
}

void SDLDrv_CD_Pause(int pauseon) {

}

int  SDLDrv_CD_IsPlaying(void) {
    return music != NULL;
}

void SDLDrv_CD_SetVolume(int volume){
    Mix_VolumeMusic(volume/2);
}