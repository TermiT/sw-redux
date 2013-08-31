//
//  achievements.h
//  sw
//
//  Created by termit on 5/6/13.
//  Copyright (c) 2013 serge. All rights reserved.
//

#ifndef __sw__achievements__
#define __sw__achievements__

#ifdef __cplusplus
#include <stdio.h>
#include <string>
#include <stdlib.h>
extern "C" {
#endif

#define ACHIEVEMENT_SWORD_KILLS         "ACHIEVEMENT_SWORD_KILLS"
#define ACHIEVEMENT_SINGAPORE           "ACHIEVEMENT_SINGAPORE"
#define ACHIEVEMENT_KABOOM              "ACHIEVEMENT_KABOOM"
#define ACHIEVEMENT_SAILOR_MOON         "ACHIEVEMENT_SAILOR_MOON"
#define ACHIEVEMENT_SNAKE               "ACHIEVEMENT_SNAKE"
#define ACHIEVEMENT_SUMO                "ACHIEVEMENT_SUMO"
#define ACHIEVEMENT_COOKIES             "ACHIEVEMENT_COOKIES"
#define ACHIEVEMENT_CRAZY_RABBIT        "ACHIEVEMENT_CRAZY_RABBIT"
#define ACHIEVEMENT_RABBITS_SEX         "ACHIEVEMENT_RABBITS_SEX"
#define ACHIEVEMENT_ENTER_THE_WANG      "ACHIEVEMENT_ENTER_THE_WANG"
#define ACHIEVEMENT_CODE_OF_HONOR       "ACHIEVEMENT_CODE_OF_HONOR"
#define ACHIEVEMENT_SECRETS             "ACHIEVEMENT_SECRETS"
#define ACHIEVEMENT_WANTON_DESTRUCTION  "ACHIEVEMENT_WANTON_DESTRUCTION"
#define ACHIEVEMENT_TWIN_DRAGON         "ACHIEVEMENT_TWIN_DRAGON"
#define ACHIEVEMENT_SPEED_RUNNER        "ACHIEVEMENT_SPEED_RUNNER"
#define ACHIEVEMENT_CORPORATE_VIOLENCE  "ACHIEVEMENT_CORPORATE_VIOLENCE"
#define ACHIEVEMENT_ZILLACIDE           "ACHIEVEMENT_ZILLACIDE"
#define ACHIEVEMENT_SPEED_RACER         "ACHIEVEMENT_SPEED_RACER"
#define ACHIEVEMENT_PACHINKO            "ACHIEVEMENT_PACHINKO"
#define ACHIEVEMENT_MY_BAD              "ACHIEVEMENT_MY_BAD"
    
#define STAT_SWORD_KILLS        "STAT_SWORD_KILLS"
#define STAT_COOKIES            "STAT_COOKIES"
#define STAT_SECRETS            "STAT_SECRETS"
#define STAT_CORPORATE_VIOLENCE "STAT_CORPORATE_VIOLENCE"
#define STAT_ZILLACIDE          "STAT_ZILLACIDE"
#define STAT_PACHINKO           "STAT_PACHINKO"
#define STAT_MY_BAD             "STAT_MY_BAD"
    
#define MAX_SWORD_KILLS         100
#define MAX_COOKIES             50
#define MAX_SECRETS             70
#define MAX_CORPORATE_VIOLENCE  50
#define MAX_ZILLACIDE           500
#define MAX_PACHINKO            10
#define MAX_MY_BAD              20
    
    
    
void recordKill(short spriteNum);
void recordKillBunny();
void recordCutInHalf();
void recordFortuneCookie();
void recordLevelEnd(short level, short skill, short kills, short totalKills, short secretsFound, short totalSecrets, const char * bestTime, long second_tics);
void recordWinPachinko();
void recordPlayedPachinko();
void recordSecretArea(short sectnum, short level);
void recordWangTalk(int soundNum);
void recordBunnyMaxPopulation();
void recordHitByVehicle();
void recordNuclearExplosion();
    
#ifdef __cplusplus
}
#endif


#endif /* defined(__sw__achievements__) */
