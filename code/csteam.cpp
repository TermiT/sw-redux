//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include "csteam.h"
#include <steam_api.h>
#include <stdio.h>
#include <stdlib.h>
#include "isteamfriends.h"


#ifndef _WIN32
#include <unistd.h>
#define Dmkdir(x) mkdir(x, 0777)
#define Dgetcwd(x, y) getcwd(x, y)
#else
#include <direct.h>
#define Dmkdir(x) _mkdir(x)
#define Dgetcwd(x, y) _getcwd(x, y)
#endif


#include <vector>
#include <stdlib.h>

#include "compat.h"

extern "C" {
long get_modified_time(const char * path);
const char* va(const char *format, ...);
char *str_replace ( const char *string, const char *substr, const char *replacement);
}


char cloudFileNames[MAX_CLOUD_FILES][MAX_CLOUD_FILE_LENGTH] = { 0 };

void dnAddCloudFileNames(void) {
    int count = 0;
    for (int i = 0; i < 10; i++) {
        for (int j=0; j < 3; j++) {
            sprintf(cloudFileNames[count], "game%d_%d.sav", i, j);
            count++;
        }
    }
//#ifndef DEBUG
    strcpy(cloudFileNames[count], "sw-redux.cfg");
//#endif
}

bool operator == (const workshop_item_t& a, const workshop_item_t& b) {
    return a.item_id == b.item_id &&
    !strcmp(a.description, b.description) &&
    !strcmp(a.filename, b.filename) &&
    !strcmp(a.itemname, b.itemname) &&
    !strcmp(a.tags, b.tags) &&
    !strcmp(a.title, b.title);
}

bool operator != (const workshop_item_t& a, const workshop_item_t& b) {
    return !(a == b);
}

const AppId_t app_id = 225160;
/* Workshop manager */
struct WorkshopManager {
    bool refreshing;
    std::vector<workshop_item_t> display_items, items;
    
    void *context;
    void (*refresh_calback)(void *context);
    
    
    WorkshopManager():refreshing(false),refresh_calback(NULL) {
    }
    
    CCallResult<WorkshopManager, SteamUGCQueryCompleted_t> ugc_query_completed_callback;
    CCallResult<WorkshopManager, SteamUGCRequestUGCDetailsResult_t> ugc_request_result;
    CCallResult<WorkshopManager, RemoteStorageDownloadUGCResult_t> ugc_download_result;
    CCallResult<WorkshopManager, RemoteStorageSubscribePublishedFileResult_t>ugc_subscribe_result;
    CCallResult<WorkshopManager, RemoteStorageUnsubscribePublishedFileResult_t>ugc_unsubscribe_result;
    
    void onSubscribe(RemoteStorageSubscribePublishedFileResult_t *pCallback, bool failure) {
        if (failure) {
            printf("onSubscribe failed\n");
            return;
        }
        this->RefreshSubscribedItems();
    }
    
    void onUnsubscribe(RemoteStorageUnsubscribePublishedFileResult_t *pCallback, bool failure) {
        if (failure) {
            printf("onUnsubscribe failed\n");
            return;
        }
        this->RefreshSubscribedItems();
    }
    
    void onQueryCompleted (SteamUGCQueryCompleted_t *pCallback, bool failure) {
        char item_folder[2048];
        char zipfile[2048];
        if (failure) {
            printf("onQueryCompleted failed\n");
            return;
        }
        SteamUGCDetails_t details;
        for (int i = 0; i < pCallback->m_unNumResultsReturned; i++) {
            SteamUGC()->GetQueryUGCResult(pCallback->m_handle, i, &details);
            if (details.m_bBanned || strstr(details.m_rgchTags, "Map") == NULL)
                continue;
            
            workshop_item_t item = { 0 };
            Dgetcwd(item_folder, sizeof(item_folder));
            strcat(item_folder, va("/workshop/maps/%llu",details.m_nPublishedFileId));
            bool need_download = false;
            sprintf(zipfile, "%s/%s", item_folder, details.m_pchFileName);
            
            if (Dmkdir(item_folder) == 0) { //folder created
                need_download = true;
            } else {
                if (!fopen(zipfile, "r")) {
                    need_download = true;
                } else if (get_modified_time(zipfile) < details.m_rtimeUpdated) {
                    remove(zipfile);
                    need_download = true;
                }
            }
            if (need_download) {
                // SteamAPICall_t callback =
                SteamRemoteStorage()->UGCDownloadToLocation(details.m_hFile, zipfile, i);
                //ugc_download_result.Set(callback, this, &WorkshopManager::onFileDownloadCompleted);
                //later add download bool field to workshop_item_t
            }
            strcpy(item.title, details.m_rgchTitle);
            strcpy(item.description, details.m_rgchDescription);
            strcpy(item.tags, details.m_rgchTags);
            strcpy(item.filename, details.m_pchFileName);
            strcpy(item.itemname, str_replace(details.m_pchFileName, ".zip", ".map"));
            item.item_id = details.m_nPublishedFileId;
            items.push_back(item);
        }
        SteamUGC()->ReleaseQueryUGCRequest(pCallback->m_handle);
        if (pCallback->m_unNumResultsReturned == kNumUGCResultsPerPage) {
            RequestItems(items.size()/kNumUGCResultsPerPage + 1);
        } else {
            if (display_items.size() != items.size() || display_items != items) {
                display_items = items;
                if (refresh_calback) {
                    refresh_calback(context);
                }
            }
            refreshing = false;
        }
        
    }
    
    void RefreshSubscribedItems() {
        if (!CSTEAM_Online() || refreshing) return;
        refreshing = true;
        items.clear();
        RequestItems(1);
    }
    
    void RequestItems(int num) {
        CSteamID steam_id = SteamUser()->GetSteamID();
        UGCQueryHandle_t query_handle = SteamUGC()->CreateQueryUserUGCRequest(steam_id.GetAccountID(), k_EUserUGCList_Subscribed, k_EUGCMatchingUGCType_Items, k_EUserUGCListSortOrder_TitleAsc, app_id, app_id, num);
        SteamAPICall_t callback = SteamUGC()->SendQueryUGCRequest(query_handle);
        ugc_query_completed_callback.Set(callback, this, &WorkshopManager::onQueryCompleted);
        
    }
    
    void Subscribe(steam_id_t item_id) {
        if (!CSTEAM_Online()/* || refreshing*/) return;
        SteamAPICall_t callback = SteamRemoteStorage()->SubscribePublishedFile(item_id);
        ugc_subscribe_result.Set(callback, this, &WorkshopManager::onSubscribe);
    }
    
    void Unsubscribe(steam_id_t item_id) {
        if (!CSTEAM_Online()/* || refreshing*/) return;
        SteamAPICall_t callback = SteamRemoteStorage()->UnsubscribePublishedFile(item_id);
        ugc_unsubscribe_result.Set(callback, this, &WorkshopManager::onUnsubscribe);
    }
    
    
    int32 GetItemsNumber() {
        return display_items.size();
    }
    
    workshop_item_t GetItemByIndex(int index) {
        return display_items[index];
    }
    
    workshop_item_t GetItemByID(steam_id_t item_id) {
        workshop_item_t item = {0};
        for (int i=0; i < display_items.size(); i++) {
            if (items[i].item_id == item_id) {
                memcpy(&item, &display_items[i], sizeof(workshop_item_t));
                break;
            }
        }
        return item;
    }
};


static WorkshopManager *workshopManager = 0;


extern "C"
int CSTEAM_Init(void) {
	printf("*** NOSTEAM Initialization ***\n");
    dnAddCloudFileNames();
    return 1;
}

extern "C"
void CSTEAM_Shutdown(void) {
	printf("*** NOSTEAM Shutdown ***\n");
}

extern "C"
void CSTEAM_ShowOverlay(const char *dialog) {
    printf("*** NOSTEAM ShowOverlay ***\n");
}

extern "C"
int CSTEAM_Online() {
	printf("*** NOSTEAM Online ***\n");
	return 1;
}

extern "C"
void CSTEAM_AchievementsInit() {
	printf("*** NOSTEAM AchievementsInit ***\n");
}

extern "C"
int CSTEAM_GetStat(const char * statID) {
	printf("*** NOSTEAM GetStat ***\n");
	return 0;
}

extern "C"
void CSTEAM_SetStat(const char* statID, int number) {
	printf("*** NOSTEAM SetStat ***\n");
}

extern "C"
void CSTEAM_UnlockAchievement(const char * achievementID) {
   printf("*** NOSTEAM UnlockAchievement ***\n");
}

extern "C"
void CSTEAM_IndicateProgress(const char * achievementID, int currentNumber, int maxNumber) {
	printf("*** NOSTEAM IndicateProgress ***\n");
}

extern "C"
void CSTEAM_DownloadFile(const char * filename) {
}

extern "C"
void CSTEAM_UploadFile(const char * filename) {
};

extern "C"
void CSTEAM_OpenCummunityHub(void) {
    printf("*** NOSTEAM OpenCummunityHub ***\n");
}

extern "C"
void CSTEAM_DeleteCloudFile(const char * filename) {
    printf("*** NOSTEAM DeleteCloudFile ***\n");
}

extern "C"
void CSTEAM_UpdateWorkshopItems(void (*callback)(void *p), void *p){
}

extern "C"
int32 CSTEAM_NumWorkshopItems() {
    return 0;
}

extern "C"
void CSTEAM_GetWorkshopItemByIndex(int index, workshop_item_t * item) {
}

extern "C"
void CSTEAM_GetWorkshopItemByID(steam_id_t item_id, workshop_item_t * item) {
}

extern "C"
void CSTEAM_SubscribeItem(steam_id_t item_id) {
}

extern "C"
void CSTEAM_UnsubscribeItem(steam_id_t item_id) {
}

extern "C"
void CSTEAM_RunFrame() {
}
