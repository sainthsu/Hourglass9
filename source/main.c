#include "common.h"
#include "draw.h"
#include "fs.h"
#include "hid.h"
#include "menu.h"
#include "platform.h"
#include "i2c.h"
#include "decryptor/keys.h"
#include "decryptor/nand.h"
#include "decryptor/nandfat.h"
#include "decryptor/game.h"
#include "decryptor/xorpad.h"
#include "decryptor/selftest.h"
#include "bottomlogo_bgr.h"

#include "string_zh.h"

#define SUBMENU_START 1

MenuInfo menu[] =
{
    {
        #ifndef VERSION_NAME
        "Hourglass9 Main Menu", 5,
        #else
        VERSION_NAME, 5,
        #endif
        {
            { STR_MAINMENU_1,    NULL,                   SUBMENU_START + 0 },
            { STR_MAINMENU_2,    NULL,                   SUBMENU_START + 1 },
            { STR_MAINMENU_3,           NULL,                   SUBMENU_START + 2 },
            { STR_MAINMENU_4,             NULL,                   SUBMENU_START + 3 },
            { STR_MAINMENU_5,           &ValidateNandDump,      0 }
        }
    },
    {
        STR_SYSNAND_OPTIONS, 4, // ID 0
        {
            { STR_SYSNAND_OPTIONS_1,               &DumpNand,              NB_MINSIZE },
            { STR_SYSNAND_OPTIONS_2,  &RestoreNand,           N_NANDWRITE | NR_KEEPA9LH },
            { STR_SYSNAND_OPTIONS_3,           &DumpHealthAndSafety,   0 },
            { STR_SYSNAND_OPTIONS_4,         &InjectHealthAndSafety, N_NANDWRITE }
        }
    },
    {
        STR_EMUNAND_OPTIONS, 4, // ID 1
        {
            { STR_EMUNAND_OPTIONS_1,               &DumpNand,              N_EMUNAND | NB_MINSIZE },
            { STR_EMUNAND_OPTIONS_2,              &RestoreNand,           N_NANDWRITE | N_EMUNAND | N_FORCEEMU },
            { STR_SYSNAND_OPTIONS_3,           &DumpHealthAndSafety,   N_EMUNAND },
            { STR_SYSNAND_OPTIONS_4,         &InjectHealthAndSafety, N_NANDWRITE | N_EMUNAND }
        }
    },
    {
        STR_GAMECART_OPTIONS, 6, // ID 2
        {
            { STR_GAMECART_OPTIONS_1,             &DumpGameCart,          0 },
            { STR_GAMECART_OPTIONS_2,             &DumpGameCart,          CD_TRIM },
            { STR_GAMECART_OPTIONS_3,   &DumpGameCart,          CD_DECRYPT },
            { STR_GAMECART_OPTIONS_4,   &DumpGameCart,          CD_DECRYPT | CD_TRIM },
            { STR_GAMECART_OPTIONS_5,             &DumpGameCart,          CD_DECRYPT | CD_MAKECIA },
            { STR_GAMECART_OPTIONS_6,          &DumpPrivateHeader,     0 }
        }
    },
    {
        STR_MISC_OPTIONS, 6, // ID 3
        {
            { STR_MISC_OPTIONS_1,         &ConvertSdToCia,        GC_CIA_DEEP },
            { STR_MISC_OPTIONS_2,         &ConvertSdToCia,        GC_CIA_DEEP | N_EMUNAND },
            { STR_MISC_OPTIONS_3,             &DumpGbaVcSave,         0 },
            { STR_MISC_OPTIONS_4,           &InjectGbaVcSave,       N_NANDWRITE },
            { STR_MISC_OPTIONS_5,                  &NcchPadgen,            0 },
            { STR_MISC_OPTIONS_6,                  &SystemInfo,            0 }
        }
    },
    {
        NULL, 0, { { 0 } } // empty menu to signal end
    }
};


void Reboot()
{
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2);
    while(true);
}


void PowerOff()
{
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 0);
    while (true);
}


u32 InitializeH9()
{
    u32 errorlevel = 0; // 0 -> none, 1 -> autopause, 2 -> critical
    
    ClearScreenFull(true, true);
    if (bottomlogo_bgr_size == 320 * 240 * 3)
        memcpy(BOT_SCREEN, bottomlogo_bgr, 320 * 240 * 3);
    
    DebugClear();
    #ifndef BUILD_NAME
    DebugColor(COLOR_ACCENT, "-- Hourglass9 --");
    #else
    DebugColor(COLOR_ACCENT, "-- %s --", BUILD_NAME);
    #endif
    
    // a little bit of information about the current menu
    if (sizeof(menu)) {
        u32 n_submenus = 0;
        u32 n_features = 0;
        for (u32 m = 0; menu[m].n_entries; m++) {
            n_submenus = m;
            for (u32 e = 0; e < menu[m].n_entries; e++)
                n_features += (menu[m].entries[e].function) ? 1 : 0;
        }
        Debug(STR_COUNT_MENUS, n_submenus, n_features);
    }
    
    Debug(STR_INITIALIZING);
    Debug("");    
    
    if ((*(vu32*) 0x101401C0) != 0)
        errorlevel = 2;
    Debug(STR_CHECKING_A9, (*(vu32*) 0x101401C0) ? STR_FAIL : STR_SUCCESS);
    if (InitFS()) {
        Debug(STR_INIT_SUCCESS);
        FileGetData("h9logo.bin", BOT_SCREEN, 320 * 240 * 3, 0);
        Debug("Build: %s", BUILD_NAME);
        Debug(STR_WORKDIR, GetWorkDir());
        if (SetupTwlKey0x03() != 0) // TWL KeyX / KeyY
            errorlevel = 2;
        if ((GetUnitPlatform() == PLATFORM_N3DS) && (SetupCtrNandKeyY0x05() != 0))
            errorlevel = 2; // N3DS CTRNAND KeyY
        if (LoadKeyFromFile(0x25, 'X', NULL)) // NCCH 7x KeyX
            errorlevel = (errorlevel < 1) ? 1 : errorlevel;
        if (LoadKeyFromFile(0x18, 'X', NULL)) // NCCH Secure3 KeyX
            errorlevel = (errorlevel < 1) ? 1 : errorlevel;
        if (LoadKeyFromFile(0x1B, 'X', NULL)) // NCCH Secure4 KeyX
            errorlevel = (errorlevel < 1) ? 1 : errorlevel;
        if (SetupAgbCmacKeyY0x24()) // AGBSAVE CMAC KeyY
            errorlevel = (errorlevel < 1) ? 1 : errorlevel;
        Debug(STR_INIT_FINAL);
        RemainingStorageSpace();
    } else {
        Debug(STR_INIT_FAIL);
            errorlevel = 2;
    }
    Debug("");
    Debug(STR_INIT, (errorlevel < 2) ? STR_SUCCESS : STR_FAIL);
    
    if (CheckButton(BUTTON_L1|BUTTON_R1) || (errorlevel > 1)) {
        DebugColor(COLOR_ASK, STR_A_TO, (errorlevel > 1) ? STR_EXIT : STR_CONTINUE);
        while (!(InputWait() & BUTTON_A));
    }
    
    return errorlevel;
}


int main()
{
    u32 menu_exit = MENU_EXIT_REBOOT;
    
    if (InitializeH9() <= 1) {
        menu_exit = ProcessMenu(menu, SUBMENU_START);
    }
    DeinitFS();
    
    ClearScreenFull(true, true);
    (menu_exit == MENU_EXIT_REBOOT) ? Reboot() : PowerOff();
    return 0;
}
