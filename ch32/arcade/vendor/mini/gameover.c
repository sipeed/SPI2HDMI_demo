/*
 * gameover.c
 *
 *  Created on: Sep 19, 2024
 *      Author: Max
 */

#include <string.h>
#include <stdlib.h>

#include "debug.h"
#include "ch32v00x.h"
#include "termGFX.h"
#include "st7735.h"
#include "buttons.h"
#include "tick.h"
#include "sound.h"
#include "engine.h"
#include "menu.h"

#include  "gameover.h"

static gameover_type_t gameover_type;
static uint32_t gameover_score;
static uint32_t gameover_highscore;
static buttons_names_t gameover_button;
static char *gameover_msg;
static uint8_t frames_passed;
static uint8_t blink;
static uint8_t restart_armed;


void GAMEOVER_Main(void)
{
    // Might need modification based on max score in games.
    char score_buf[12];
    uint8_t offset = 0;

    frames_passed++;

    tGFX_Clear(0);

    tGFX_SetCursor(0, 0);
    tGFX_Print(">GAME OVER", MENU_COLOR_FOREGROUND, 0);
    tGFX_SetCursor(0, 1);

    switch(gameover_type)
    {
        case GAMEOVER_TYPE_SCORE:

            itoa(gameover_score, score_buf, 10);
            tGFX_Print(">SCORE:", MENU_COLOR_FOREGROUND, 0);
            tGFX_Print(score_buf, MENU_COLOR_FOREGROUND, 0);

            tGFX_SetCursor(0, 2);
            itoa(gameover_highscore, score_buf, 10);
            tGFX_Print(">HIGH SCORE:", MENU_COLOR_FOREGROUND, 0);
            tGFX_Print(score_buf, MENU_COLOR_FOREGROUND, 0);

            offset++;

            break;

        case GAMEOVER_TYPE_MESSAGE:
            tGFX_Print(">", MENU_COLOR_FOREGROUND, 0);
            tGFX_Print(gameover_msg, MENU_COLOR_FOREGROUND, 0);
            break;

        default:
            break;

    }

    tGFX_SetCursor(0, 2 + offset);
    tGFX_Print(">UP/J RESTART", MENU_COLOR_FOREGROUND, 0);
    tGFX_SetCursor(0, 3 + offset);
    tGFX_Print(">K/Q MENU", MENU_COLOR_FOREGROUND, 0);
    tGFX_SetCursor(0, 4 + offset);
    tGFX_Print(">", MENU_COLOR_FOREGROUND, 0);

    if(frames_passed == 15)
    {
        frames_passed = 0;
        blink++;
    }

    if(blink%2) tGFX_SetChar('_', 1, 4 + offset, MENU_COLOR_FOREGROUND, 0);
    else tGFX_SetChar(0, 1, 4 + offset, 15, 0);

    uint8_t pressed=BTN_IsPressed(gameover_button)|BTN_IsPressed(BTN_ACTION);
    if(!pressed)restart_armed=1;
    if(restart_armed && (input_take(gameover_button)|input_take(BTN_ACTION)))
        app_select(app_current);
}

void GAMEOVER_Setup(gameover_type_t type, uint32_t score, uint32_t highscore, buttons_names_t button, const char *msg)
{
    gameover_type = type;
    gameover_score = score;
    gameover_highscore = highscore;
    (void)button;gameover_button = BTN_UP;
    gameover_msg = (char*)msg;
    frames_passed = 0;
    blink = 0;
    restart_armed=0;
    input_take(gameover_button);input_take(BTN_ACTION);
}

void GAMEOVER_Show(void)
{
    ENG_UpdateInFrameCallback(&GAMEOVER_Main);
}
