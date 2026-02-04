#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <string.h>
#include <stdio.h>

/* --- 1. DEFINITIONS --- */
#ifndef IPTR
typedef unsigned long IPTR;
#endif

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) (((ULONG)(a)<<24)|((ULONG)(b)<<16)|((ULONG)(c)<<8)|(ULONG)(d))
#endif

/* Format: $VER: <Nom> <Ver>.<Rev> (<Date>) */
const char verTag[] = "$VER: WirelessSettings 1.0 (04.02.26) Renaud Schweingruber";

#define PREFS_PATH "ENVARC:Sys/wireless.prefs"
#define ID_SAVE    0x100
#define ID_LOAD    0x101 /* Changement : ID_LOAD remplace ID_CONNECT */

static const char *encryption_types[] = { "Open", "WEP", "WPA/WPA2", NULL };
struct Library *MUIMasterBase;

/* --- UTILITAIRES --- */
BOOL CheckPrefsExist(void) {
    BPTR lock = Lock(PREFS_PATH, SHARED_LOCK);
    if (lock) { UnLock(lock); return TRUE; }
    return FALSE;
}

void LoadWirelessPrefs(char *ssid, char *pass, ULONG *encType) {
    BPTR fh = Open(PREFS_PATH, MODE_OLDFILE);
    char line[256];
    char *start, *end;
    *encType = 0;

    /* Reset des buffers pour éviter de garder les anciennes valeurs */
    ssid[0] = 0;
    pass[0] = 0;

    if (fh) {
        while (FGets(fh, line, 256)) {
            if ((start = strstr(line, "ssid=\""))) {
                start += 6;
                if ((end = strchr(start, '\"'))) { *end = 0; strcpy(ssid, start); }
            }
            else if ((start = strstr(line, "wep_key0=\""))) {
                *encType = 1; start += 10;
                if ((end = strchr(start, '\"'))) { *end = 0; strcpy(pass, start); }
            }
            else if ((start = strstr(line, "#psk=\""))) {
                *encType = 2; start += 6;
                if ((end = strchr(start, '\"'))) { *end = 0; strcpy(pass, start); }
            }
            else if (strstr(line, "psk=")) { *encType = 2; }
        }
        Close(fh);
    }
}

/* --- MAIN --- */
int main(int argc, char **argv) {
    Object *app, *win;
    Object *mainGroup, *settingsGroup, *buttonsGroup;
    Object *strSSID, *strPass, *cycEnc, *btnSave, *btnLoad;

    char bufSSID[33] = "";
    char bufPass[65] = "";
    ULONG initEnc = 0;
    ULONG sigs = 0;
    ULONG res = 0;
    BOOL running = TRUE;
    STRPTR s, p;
    ULONG enc;
    char cmd[256];
    BPTR fh;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;

    LoadWirelessPrefs(bufSSID, bufPass, &initEnc);

    /* --- CONSTRUCTION UI --- */
    strSSID = MUI_NewObject(MUIC_String,
                            MUIA_Frame, MUIV_Frame_String,
                            MUIA_String_Contents, (IPTR)bufSSID,
                            MUIA_String_MaxLen, 32,
                            TAG_DONE);

    cycEnc = MUI_NewObject(MUIC_Cycle,
                           MUIA_Cycle_Entries, (IPTR)encryption_types,
                           MUIA_Cycle_Active, (IPTR)initEnc,
                           TAG_DONE);

    strPass = MUI_NewObject(MUIC_String,
                            MUIA_Frame, MUIV_Frame_String,
                            MUIA_String_Contents, (IPTR)bufPass,
                            MUIA_String_MaxLen, 64,
                            MUIA_String_Secret, TRUE,
                            TAG_DONE);

    btnSave = MUI_MakeObject(MUIO_Button, (IPTR)"_Save");
    btnLoad = MUI_MakeObject(MUIO_Button, (IPTR)"_Load"); /* Nouveau Libellé */

    settingsGroup = MUI_NewObject(MUIC_Group,
                                  MUIA_Frame, MUIV_Frame_Group,
                                  MUIA_FrameTitle, (IPTR)"Settings",
                                  MUIA_Group_Columns, 2,
                                  MUIA_Group_Child, MUI_MakeObject(MUIO_Label, (IPTR)"SSID :", (IPTR)"\33r"),
                                  MUIA_Group_Child, strSSID,
                                  MUIA_Group_Child, MUI_MakeObject(MUIO_Label, (IPTR)"Encryption :", (IPTR)"\33r"),
                                  MUIA_Group_Child, cycEnc,
                                  MUIA_Group_Child, MUI_MakeObject(MUIO_Label, (IPTR)"Password :", (IPTR)"\33r"),
                                  MUIA_Group_Child, strPass,
                                  TAG_DONE);

    buttonsGroup = MUI_NewObject(MUIC_Group,
                                 MUIA_Group_Horiz, TRUE,
                                 MUIA_Group_Child, btnSave,
                                 MUIA_Group_Child, btnLoad,
                                 TAG_DONE);

    mainGroup = MUI_NewObject(MUIC_Group,
                              MUIA_Group_Child, settingsGroup,
                              MUIA_Group_Child, buttonsGroup,
                              TAG_DONE);

    win = MUI_NewObject(MUIC_Window,
                        MUIA_Window_Title, (IPTR)"Wireless Settings",
                        MUIA_Window_ID,    (IPTR)MAKE_ID('W','I','F','I'),
                        MUIA_Window_RootObject, mainGroup,
                        TAG_DONE);

    app = MUI_NewObject(MUIC_Application,
                        MUIA_Application_Title, (IPTR)"WirelessSettings",
                        MUIA_Application_Window, win,
                        TAG_DONE);

    if (!app) { CloseLibrary(MUIMasterBase); return 20; }

    /* --- INITIALISATION ETATS --- */

    /* Le bouton Load est désactivé si le fichier n'existe pas */
    set(btnLoad, MUIA_Disabled, !CheckPrefsExist());

    if (initEnc == 0) set(strPass, MUIA_Disabled, TRUE);

    /* --- NOTIFICATIONS --- */
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2,
             MUIM_Application_ReturnID, (IPTR)MUIV_Application_ReturnID_Quit);
    DoMethod(btnSave, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2,
             MUIM_Application_ReturnID, (IPTR)ID_SAVE);
    DoMethod(btnLoad, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2,
             MUIM_Application_ReturnID, (IPTR)ID_LOAD);

    /* Gestion champs password */
    DoMethod(cycEnc, MUIM_Notify, MUIA_Cycle_Active, 0,
             strPass, 3, MUIM_Set, MUIA_Disabled, TRUE);
    DoMethod(cycEnc, MUIM_Notify, MUIA_Cycle_Active, 1,
             strPass, 3, MUIM_Set, MUIA_Disabled, FALSE);
    DoMethod(cycEnc, MUIM_Notify, MUIA_Cycle_Active, 2,
             strPass, 3, MUIM_Set, MUIA_Disabled, FALSE);

    set(win, MUIA_Window_Open, TRUE);

    while (running) {
        res = DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs);

        if (res == MUIV_Application_ReturnID_Quit) {
            running = FALSE;
        }
        else if (res == ID_SAVE) {
            get(strSSID, MUIA_String_Contents, (IPTR *)&s);
            get(strPass, MUIA_String_Contents, (IPTR *)&p);
            get(cycEnc, MUIA_Cycle_Active, (IPTR *)&enc);

            /* Validation WPA < 8 caractères */
            if (enc == 2 && strlen(p) < 8) {
                MUI_Request(app, win, 0, (IPTR)"Wrong password", (IPTR)"_OK",
                            (IPTR)"The password must contain at least 8 characters.");
            }
            else {
                if (enc == 2) {
                    sprintf(cmd, "C:wpa_passphrase \"%s\" \"%s\" > %s", s, p, PREFS_PATH);
                    Execute((STRPTR)cmd, 0, 0);
                }
                else {
                    fh = Open(PREFS_PATH, MODE_NEWFILE);
                    if (fh) {
                        FPrintf(fh, "network={\n\tssid=\"%s\"\n\tscan_ssid=0\n\tkey_mgmt=NONE\n", s);
                        if (enc == 1) FPrintf(fh, "\twep_key0=\"%s\"\n\twep_tx_keyidx=0\n", p);
                        FPuts(fh, "}\n");
                        Close(fh);
                    }
                }

                /* Si le fichier est créé, on active le bouton Load */
                if (CheckPrefsExist()) set(btnLoad, MUIA_Disabled, FALSE);
                DisplayBeep(NULL);
            }
        }
        else if (res == ID_LOAD) {
            /* 1. On recharge les variables depuis le fichier */
            LoadWirelessPrefs(bufSSID, bufPass, &initEnc);

            /* 2. On met à jour l'interface (Gadgets) avec les nouvelles valeurs */
            set(strSSID, MUIA_String_Contents, (IPTR)bufSSID);
            set(strPass, MUIA_String_Contents, (IPTR)bufPass);
            set(cycEnc, MUIA_Cycle_Active, (IPTR)initEnc);

            /* 3. On met à jour l'état du champ mot de passe */
            /* Si c'est Open (0), on grise. Sinon on active. */
            if (initEnc == 0) set(strPass, MUIA_Disabled, TRUE);
            else set(strPass, MUIA_Disabled, FALSE);
        }

        if (running && sigs) {
            sigs = Wait(sigs | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C) running = FALSE;
        }
    }

    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
