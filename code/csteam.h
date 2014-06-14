//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#ifndef CSTEAM_H
#define CSTEAM_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CLOUD_FILES 31
#define MAX_CLOUD_FILE_LENGTH 15
#define MAX_CLOUD_FILE_SIZE 1024 * 1024
    
int CSTEAM_Init(void);
void CSTEAM_Shutdown(void);
void CSTEAM_ShowOverlay(const char *dialog);
int CSTEAM_Online();
void CSTEAM_AchievementsInit();
int CSTEAM_GetStat(const char * statID);
void CSTEAM_SetStat(const char* statID, int number);
void CSTEAM_UnlockAchievement(const char * achievementID);
void CSTEAM_IndicateProgress(const char * achievementID, int currentNumber, int maxNumber);
void CSTEAM_UploadFile(const char * filename);
void CSTEAM_DownloadFile(const char * filename);
void CSTEAM_OpenCummunityHub(void);
void CSTEAM_DeleteCloudFile(const char * filename);
    
typedef unsigned long long steam_id_t;

typedef struct {
    steam_id_t item_id;
    char title[128 + 1];		// title of item
    char description[8000];     // description of item
    char tags[1024 + 1];		// comma separated list of all tags associated with this file
    char filename[260];			// filename of the primary file (zip)
    char itemname[260];         // actual item name (map, demo)
} workshop_item_t;
    
void CSTEAM_UpdateWorkshopItems(void (*callback)(void *p), void *p);
void CSTEAM_GetWorkshopItemByIndex(int index, workshop_item_t * item);
void CSTEAM_GetWorkshopItemByID(steam_id_t item_id, workshop_item_t * item);
int CSTEAM_NumWorkshopItems();
void CSTEAM_SubscribeItem(steam_id_t item_id);
void CSTEAM_UnsubscribeItem(steam_id_t item_id);
    
void CSTEAM_RunFrame();
    

#ifdef __cplusplus
}
#endif

#endif /* CSTEAM_H */
