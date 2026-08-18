/*
 * Copyright (C) 2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  patch.c
 * @brief Patching some of the .so internal functions or bridging them to native
 *        for better compatibility.
 */

#include <kubridge.h>
#include <so_util/so_util.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include "utils/logger.h"

extern so_module so_mod;
static so_hook CMvAppC1Ev_hook;
static so_hook CMvPlayerC2Ei_hook;
static so_hook CMvGameState_hook;

static so_hook _Z12GsFSFileSizePKci_hook;
static so_hook _ZN12CMvGameState15IsExistGameDataEi_hook;
static so_hook _ZN12CMvGameState12LoadGameDataEi_hook;
static so_hook _ZN20GVUIPlayerController19InitialPlayerPadSetEv_hook;
static so_hook _ZN11CMvGraphics10SetQualityE16EnumQualityLevel_hook;

static so_hook CGsStateManagerI12CMvGameStateERunPS0_h_hook;
static so_hook _ZN9CMvPlayer13DrawCharacterEii_hook;

void *g_CMvApp_instance = NULL;
void *g_CMvPlayer_instance = NULL;
void *g_CMvGameState_instance = NULL;

int game_data_exists = -1;

extern int settings_graphicsqualty;

typedef struct CGsStateManagerNode {
    void *ptr;
    int32_t adj;
    struct CGsStateManagerNode *next;
} CGsStateManagerNode;

// the game corrupts its StateManager during play and causes a crash
// this rebuilds the StateManager to a working state.
// For this portion of the code AI Assistance was used.
static int CGsStateManagerNode_Invoke(CGsStateManagerNode *node, void *pObj) {
    void *ptr = node->ptr;
    int32_t adj = node->adj;
    void *adjThis = (void *)((char *)pObj + (adj >> 1));
    int (*func)(void *);

    if (adj & 1) {
        void **vtable = *(void ***)adjThis;
        func = *(int (**)(void *))((char *)vtable + (uintptr_t)ptr);
    } else {
        func = (int (*)(void *))ptr;
    }

    return func(adjThis);
}

// the game corrupts its StateManager during play and causes a crash
// this rebuilds the StateManager to a working state.
// For this portion of the code AI Assistance was used.
int CGsStateManagerRun_patched(void *this, void *pObj, int which) {
    CGsStateManagerNode *head = *(CGsStateManagerNode **)this;
    if (!head) { return -1; }

    if ((which & 0xFF) == 0) {
        return CGsStateManagerNode_Invoke(head, pObj);
    }

    int result = 0;
    CGsStateManagerNode *node = head;
    while (node) {
        CGsStateManagerNode *next = node->next; 
        result = CGsStateManagerNode_Invoke(node, pObj);
        node = next;
    }
    return result;
}

void CMvAppC1Ev_patched(void *this) {
    g_CMvApp_instance = this;
    SO_CONTINUE(void *, CMvAppC1Ev_hook, this);
}

void CMvPlayerC2Ei_patched(void *this, int param) {
    g_CMvPlayer_instance = this;
    l_debug("CMvPlayer::CMvPlayer: Hooked into function");
    SO_CONTINUE(void *, CMvPlayerC2Ei_hook, this, param);
}

void _ZN12CMvGameStateC2Ev(void *this) {
    g_CMvGameState_instance = this;
    l_debug("CMvGameState::CMvGameState: Hooked into function");
    SO_CONTINUE(void *, CMvGameState_hook, this);
}

void _ZN12CMvGameState15IsExistGameDataEi_patched(void *this, int param) {
    l_debug("CMvGameState::IsExistGameData: Hooked into function --> param=%d", param);
    if(game_data_exists != -1){
        game_data_exists = -1;
        SO_CONTINUE(void *, _ZN12CMvGameState15IsExistGameDataEi_hook, this, game_data_exists);
    } 
}

void _ZN12CMvGameState12LoadGameDataEi_patched(void *this, int param) {
    l_debug("CMvGameState::LoadGameData: Hooked into function --> param=%d", param);
    game_data_exists = param;
    SO_CONTINUE(void *, _ZN12CMvGameState12LoadGameDataEi_hook, this, param);
}

int _Z12GsFSFileSizePKci_patched(const char *path, int *intparam) {
    char new_path[256];
    struct stat st;

    snprintf(new_path, sizeof(new_path), "%s%s", "ux0:/data/zenonia1//", path);

    if (stat(new_path, &st) == 0) {
        l_debug("Stat: %s -> size=%d", new_path, (int)st.st_size);
        return (int)st.st_size;
    }

    return -1;
}

void _ZN20GVUIPlayerController19InitialPlayerPadSetEv_patched(void *this) {        // how annoying. THIS is what gets rid of the touch display.
    l_debug("GVUIPlayerController::InitialPlayerPadSet: Hooked into function");
}

void _ZN11CMvGraphics10SetQualityE16EnumQualityLevel_patched(void *this, int param) {
    l_debug("CMvGraphics::SetQuality Hooked into function %d vs %d", param , settings_graphicsqualty);

    // set the upper and lower bounds, just in case.
    if(settings_graphicsqualty < 0) {
        settings_graphicsqualty = 0;
    }

    if(settings_graphicsqualty > 2) {
        settings_graphicsqualty = 2;
    }

    SO_CONTINUE(void *, _ZN11CMvGraphics10SetQualityE16EnumQualityLevel_hook, this, settings_graphicsqualty);
}

void _ZN9CMvPlayer13DrawCharacterEii_patched(void *this, int param1, int param2) {
    return 0;
}

void so_patch(void) {
    CMvAppC1Ev_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN6CMvAppC1Ev"),
        (uintptr_t)&CMvAppC1Ev_patched);

    CMvPlayerC2Ei_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN9CMvPlayerC2Ei"),
        (uintptr_t)&CMvPlayerC2Ei_patched);

    CMvGameState_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN12CMvGameStateC2Ev"),
        (uintptr_t)&_ZN12CMvGameStateC2Ev);

    _Z12GsFSFileSizePKci_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_Z12GsFSFileSizePKci"),
        (uintptr_t)&_Z12GsFSFileSizePKci_patched);

    _ZN12CMvGameState15IsExistGameDataEi_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN12CMvGameState15IsExistGameDataEi"),
        (uintptr_t)&_ZN12CMvGameState15IsExistGameDataEi_patched);

    _ZN12CMvGameState12LoadGameDataEi_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN12CMvGameState12LoadGameDataEi"),
        (uintptr_t)&_ZN12CMvGameState12LoadGameDataEi_patched);

    _ZN20GVUIPlayerController19InitialPlayerPadSetEv_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN20GVUIPlayerController19InitialPlayerPadSetEv"),
        (uintptr_t)&_ZN20GVUIPlayerController19InitialPlayerPadSetEv_patched); 

    _ZN11CMvGraphics10SetQualityE16EnumQualityLevel_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN11CMvGraphics10SetQualityE16EnumQualityLevel"),
        (uintptr_t)&_ZN11CMvGraphics10SetQualityE16EnumQualityLevel_patched); 

    CGsStateManagerI12CMvGameStateERunPS0_h_hook = hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN15CGsStateManagerI12CMvGameStateE3RunEPS0_h"),
        (uintptr_t)&CGsStateManagerRun_patched);

}
