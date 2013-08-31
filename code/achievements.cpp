//
//  achievements.cpp
//  sw
//
//  Created by termit on 5/6/13.
//  Copyright (c) 2013 serge. All rights reserved.
//

#include "achievements.h"
#include "csteam.h"
#include "megawang.h"
#include "names2.h"

#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif


void recordCountAchievement(const char* achievement, const char* stat, int max_count, int show_progress_every, bool show_first) {
    int number = CSTEAM_GetStat(stat);
	if (number > -1) {
		number++;
		CSTEAM_SetStat(stat, number);
		if (((number == 1 && show_first) || (number % show_progress_every == 0)) && number != max_count)  {
			CSTEAM_IndicateProgress(achievement, number, max_count);
		}
		if (number >= max_count) {
			CSTEAM_UnlockAchievement(achievement);
		}
	}
}

void recordKill(short spriteNum) {
    extern short Level;
    switch (spriteNum) {
        case SERP_RUN_R0:
            if (Level == 5) {
                CSTEAM_UnlockAchievement(ACHIEVEMENT_SNAKE);
            }
            break;
        case SUMO_RUN_R0:
            CSTEAM_UnlockAchievement(ACHIEVEMENT_SUMO);
            break;
        default:
            break;
    }
    recordCountAchievement(ACHIEVEMENT_CORPORATE_VIOLENCE, STAT_CORPORATE_VIOLENCE, MAX_CORPORATE_VIOLENCE, 10, false);
    recordCountAchievement(ACHIEVEMENT_ZILLACIDE, STAT_ZILLACIDE, MAX_ZILLACIDE, 50, false);
}

void recordKillBunny() {
//    printf("%s\n", __func__);
}

void recordCutInHalf() {
    recordCountAchievement(ACHIEVEMENT_SWORD_KILLS, STAT_SWORD_KILLS, MAX_SWORD_KILLS, 10, true);
}

void recordFortuneCookie() {
    recordCountAchievement(ACHIEVEMENT_COOKIES, STAT_COOKIES, MAX_COOKIES, 5, true);
}

void recordLevelEnd(short level, short skill, short kills, short totalKills, short secretsFound, short totalSecrets, const char * bestTime, long second_tics) {
    if (level == 4 && swGetAddon() == 0) {
        printf("finished Enter the Wang\n");
        CSTEAM_UnlockAchievement(ACHIEVEMENT_ENTER_THE_WANG);
    } else if (level == 20) {
        switch (swGetAddon()) {
            case 0:
                CSTEAM_UnlockAchievement(ACHIEVEMENT_CODE_OF_HONOR);
                break;
            case 1:
                CSTEAM_UnlockAchievement(ACHIEVEMENT_WANTON_DESTRUCTION);
                break;
            case 2:
                CSTEAM_UnlockAchievement(ACHIEVEMENT_TWIN_DRAGON);
                break;
            default:
                break;
        }
    }
    
    if (kills >= totalKills) {
//        printf("killed everyone\n");
    }
    
    if (secretsFound >= totalSecrets) {
//      printf("found every secret\n");
    }
    
    int minutes, seconds;
    if ((sscanf(bestTime, "%d : %d", &minutes, &seconds) == 2) && (minutes * 60 + seconds > second_tics)) {
        CSTEAM_UnlockAchievement(ACHIEVEMENT_SPEED_RUNNER);
    }
    
}

void recordWinPachinko() {
    recordCountAchievement(ACHIEVEMENT_PACHINKO, STAT_PACHINKO, MAX_PACHINKO, 2, true);
}

void recordPlayedPachinko() {
//    printf("%s\n", __func__);
}

void recordSecretArea(short sectnum, short level) {
//    printf("%s sectnum: %d, level: %d\n", __func__, sectnum, level);
    if (swGetAddon() != 0) return;
    if (level == 1 && sectnum == 72) {
        CSTEAM_UnlockAchievement(ACHIEVEMENT_SINGAPORE);
    } else if (level == 3 && sectnum == 355) {
    } else if (sectnum == 559 && level == 18) {
        CSTEAM_UnlockAchievement(ACHIEVEMENT_SAILOR_MOON);
    }
    recordCountAchievement(ACHIEVEMENT_SECRETS, STAT_SECRETS, MAX_SECRETS, 5, true);
}

void recordWangTalk(int soundNum) {
//    printf("%s soundNum: %d\n", __func__, soundNum);
    switch (soundNum) {
        case 373:
            CSTEAM_UnlockAchievement(ACHIEVEMENT_SPEED_RACER);
            break;
        case 375:
//            printf("Larra Croft\n");
            break;
        case 446:
            CSTEAM_UnlockAchievement(ACHIEVEMENT_CRAZY_RABBIT);
            break;
        case 563:
//            printf("Mechanic Girl\n");
            break;
        default:
            break;
    }
}

void recordBunnyMaxPopulation() {
    if(swGetAddon() == 0)
        CSTEAM_UnlockAchievement(ACHIEVEMENT_RABBITS_SEX);
}

void recordHitByVehicle() {
    recordCountAchievement(ACHIEVEMENT_MY_BAD, STAT_MY_BAD, MAX_MY_BAD, 5, true);
}

void recordNuclearExplosion() {
    CSTEAM_UnlockAchievement(ACHIEVEMENT_KABOOM);
}