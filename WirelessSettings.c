#include <exec/types.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* --- 1. DEFINITIONS --- */
#ifndef IPTR
typedef unsigned long IPTR;
#endif

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) (((ULONG)(a)<<24)|((ULONG)(b)<<16)|((ULONG)(c)<<8)|(ULONG)(d))
#endif

/* Format: $VER: <Nom> <Ver>.<Rev> (<Date>) */
const char verTag[] = "$VER: WirelessSettings 1.1 (06.02.26) Renaud Schweingruber";

#define PREFS_PATH "ENVARC:Sys/wireless.prefs"
#define NETWORKS_PATH "ENV:WiFiNetworks"
#define MAX_NETWORKS 50
#define ID_SAVE    0x100
#define ID_LOAD    0x101
#define ID_SCAN    0x102

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

/* Charger les réseaux WiFi depuis ENV:WiFiNetworks */
int LoadNetworksList(char **networkArray, int maxNetworks) {
    BPTR fh;
    char line[256];
    int count = 0;
    int i, len;

    fh = Open(NETWORKS_PATH, MODE_OLDFILE);
    if (!fh) return 0;

    while (FGets(fh, line, 256) && count < maxNetworks - 1) {
        /* Supprimer le retour à la ligne */
        len = strlen(line);
        for (i = 0; i < len; i++) {
            if (line[i] == '\n' || line[i] == '\r') {
                line[i] = 0;
                break;
            }
        }
        
        /* Ignorer les lignes vides */
        if (line[0] == 0) continue;
        
        /* Détecter le message d'erreur "scan not supported" */
        if (strstr(line, "does not support wireless network scanning")) {
            Close(fh);
            return -1; /* Code d'erreur spécial */
        }

        /* Allouer et copier le nom du réseau */
        networkArray[count] = (char *)malloc(strlen(line) + 1);
        if (networkArray[count]) {
            strcpy(networkArray[count], line);
            count++;
        }
    }

    Close(fh);
    
    /* Terminer le tableau avec NULL (requis par MUI Cycle) */
    networkArray[count] = NULL;
    
    return count;
}

/* Libérer la mémoire allouée pour la liste des réseaux */
void FreeNetworksList(char **networkArray, int count) {
    int i;
    for (i = 0; i < count; i++) {
        if (networkArray[i]) {
            free(networkArray[i]);
            networkArray[i] = NULL;
        }
    }
}

/* Nettoyer le SSID en enlevant les mentions de fréquence */
void StripFrequencyTags(const char *source, char *dest, int maxLen) {
    char *pos;
    int len;
    
    /* Copier la chaîne source */
    strncpy(dest, source, maxLen - 1);
    dest[maxLen - 1] = 0;
    
    /* Chercher et supprimer " (5GHz)" */
    pos = strstr(dest, " (5GHz)");
    if (pos) {
        *pos = 0;
    }
    
    /* Chercher et supprimer " (2.4GHz)" */
    pos = strstr(dest, " (2.4GHz)");
    if (pos) {
        *pos = 0;
    }
    
    /* Supprimer les espaces en fin de chaîne */
    len = strlen(dest);
    while (len > 0 && dest[len - 1] == ' ') {
        dest[len - 1] = 0;
        len--;
    }
}

/* --- MAIN --- */
int main(int argc, char **argv) {
    Object *app, *win;
    Object *mainGroup, *scanGroup, *settingsGroup, *buttonsGroup;
    Object *strSSID, *strPass, *cycEnc, *btnSave, *btnLoad, *btnScan, *cycNetworks;
    Object *menuStrip, *menuProject, *menuAbout;

    char bufSSID[33] = "";
    char bufPass[65] = "";
    ULONG initEnc = 0;
    ULONG sigs = 0;
    ULONG res = 0;
    BOOL running = TRUE;
    STRPTR s, p;
    ULONG enc, selectedNetwork;
    char cmd[256];
    BPTR fh;
    
    /* Gestion de la liste des réseaux */
    char *networks[MAX_NETWORKS];
    int networkCount = 0;
    int i;
    
    /* Initialiser le tableau à NULL */
    for (i = 0; i < MAX_NETWORKS; i++) {
        networks[i] = NULL;
    }
    
    /* Ajouter un message par défaut */
    networks[0] = (char *)malloc(50);
    strcpy(networks[0], "Click SCAN to list networks");
    networks[1] = NULL;

    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;

    LoadWirelessPrefs(bufSSID, bufPass, &initEnc);

    /* --- CONSTRUCTION MENU --- */
    
    menuAbout = MUI_NewObject(MUIC_Menuitem,
                              MUIA_Menuitem_Title, (IPTR)"About...",
                              MUIA_UserData, (IPTR)0x200,
                              TAG_DONE);
    
    menuProject = MUI_NewObject(MUIC_Menu,
                                MUIA_Menu_Title, (IPTR)"Project",
                                MUIA_Family_Child, menuAbout,
                                TAG_DONE);
    
    menuStrip = MUI_NewObject(MUIC_Menustrip,
                              MUIA_Family_Child, menuProject,
                              TAG_DONE);

    /* --- CONSTRUCTION UI --- */
    
    /* Cycle pour afficher les réseaux WiFi détectés */
    cycNetworks = MUI_NewObject(MUIC_Cycle,
                                MUIA_Cycle_Entries, (IPTR)networks,
                                MUIA_Cycle_Active, 0,
                                TAG_DONE);
    
    btnScan = MUI_MakeObject(MUIO_Button, (IPTR)"_Scan");
    
    /* Groupe Scan (nouveau) */
    scanGroup = MUI_NewObject(MUIC_Group,
                              MUIA_Frame, MUIV_Frame_Group,
                              MUIA_FrameTitle, (IPTR)"Available Networks",
                              MUIA_Group_Columns, 2,
                              MUIA_Group_Child, cycNetworks,
                              MUIA_Group_Child, btnScan,
                              TAG_DONE);

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
    btnLoad = MUI_MakeObject(MUIO_Button, (IPTR)"_Load");

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
                              MUIA_Group_Child, scanGroup,
                              MUIA_Group_Child, settingsGroup,
                              MUIA_Group_Child, buttonsGroup,
                              TAG_DONE);

    win = MUI_NewObject(MUIC_Window,
                        MUIA_Window_Title, (IPTR)"Wireless Settings",
                        MUIA_Window_ID,    (IPTR)MAKE_ID('W','I','F','I'),
                        MUIA_Window_Menustrip, menuStrip,
                        MUIA_Window_RootObject, mainGroup,
                        TAG_DONE);

    app = MUI_NewObject(MUIC_Application,
                        MUIA_Application_Title, (IPTR)"WirelessSettings",
                        MUIA_Application_Window, win,
                        TAG_DONE);

    if (!app) { 
        FreeNetworksList(networks, MAX_NETWORKS);
        CloseLibrary(MUIMasterBase); 
        return 20; 
    }

    /* --- INITIALISATION ETATS --- */

    /* Le bouton Load est désactivé si le fichier n'existe pas */
    set(btnLoad, MUIA_Disabled, !CheckPrefsExist());

    /* La liste des réseaux est grisée jusqu'au premier scan */
    set(cycNetworks, MUIA_Disabled, TRUE);

    if (initEnc == 0) set(strPass, MUIA_Disabled, TRUE);

    /* --- NOTIFICATIONS --- */
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2,
             MUIM_Application_ReturnID, (IPTR)MUIV_Application_ReturnID_Quit);
    DoMethod(btnSave, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2,
             MUIM_Application_ReturnID, (IPTR)ID_SAVE);
    DoMethod(btnLoad, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2,
             MUIM_Application_ReturnID, (IPTR)ID_LOAD);
    DoMethod(btnScan, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2,
             MUIM_Application_ReturnID, (IPTR)ID_SCAN);
    
    /* Notification menu About */
    DoMethod(menuAbout, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
             (IPTR)app, 2, MUIM_Application_ReturnID, (IPTR)0x200);

    /* Notification : sélection d'un réseau dans le Cycle */
    DoMethod(cycNetworks, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             (IPTR)app, 2, MUIM_Application_ReturnID, (IPTR)0x103);

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
            get(strSSID, MUIA_String_Contents, &s);
            get(strPass, MUIA_String_Contents, &p);
            get(cycEnc, MUIA_Cycle_Active, &enc);

            /* Validation WPA < 8 caractères */
            if (enc == 2 && strlen(p) < 8) {
                MUI_Request(app, win, 0, "Wrong password", "_OK",
                            "The password must contain at least 8 characters.");
            }
            else {
                if (enc == 2) {
                    BPTR nullFh;
                    sprintf(cmd, "C:wpa_passphrase \"%s\" \"%s\" > %s", s, p, PREFS_PATH);
                    nullFh = Execute((STRPTR)cmd, 0, 0);
                }
                else {
                    fh = Open(PREFS_PATH, MODE_NEWFILE);
                    if (fh) {
                        (void)FPrintf(fh, "network={\n\tssid=\"%s\"\n\tscan_ssid=0\n\tkey_mgmt=NONE\n", s);
                        if (enc == 1) (void)FPrintf(fh, "\twep_key0=\"%s\"\n\twep_tx_keyidx=0\n", p);
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
            if (initEnc == 0) set(strPass, MUIA_Disabled, TRUE);
            else set(strPass, MUIA_Disabled, FALSE);
        }
        else if (res == ID_SCAN) {
            BPTR nullFh;
            /* Lancer la commande de scan */
            nullFh = Execute((STRPTR)"C:ListNetworks SHORT >ENV:WiFiNetworks", 0, 0);
            
            /* Attendre un peu que la commande se termine */
            Delay(100); /* 2 secondes (50 = 1 seconde sur AmigaOS) */
            
            /* Libérer l'ancienne liste */
            FreeNetworksList(networks, networkCount);
            
            /* Charger la nouvelle liste */
            networkCount = LoadNetworksList(networks, MAX_NETWORKS);
            
            if (networkCount == -1) {
                /* Erreur : périphérique ne supporte pas le scan */
                MUI_Request(app, win, 0, "Scan Error", "_OK",
                            "This device does not support\nwireless network scanning.\n\nPlease enter the SSID manually.");
                /* Réinitialiser avec le message par défaut */
                networks[0] = (char *)malloc(50);
                strcpy(networks[0], "Click SCAN to list networks");
                networks[1] = NULL;
                networkCount = 0;
                set(cycNetworks, MUIA_Cycle_Entries, (IPTR)networks);
                /* Laisser la liste grisée */
                set(cycNetworks, MUIA_Disabled, TRUE);
            }
            else if (networkCount > 0) {
                /* Mettre à jour le Cycle avec la nouvelle liste */
                set(cycNetworks, MUIA_Cycle_Entries, (IPTR)networks);
                set(cycNetworks, MUIA_Cycle_Active, 0);
                /* Activer la liste maintenant qu'il y a des réseaux */
                set(cycNetworks, MUIA_Disabled, FALSE);
            } else {
                /* Aucun réseau trouvé */
                networks[0] = (char *)malloc(50);
                strcpy(networks[0], "No networks found");
                networks[1] = NULL;
                set(cycNetworks, MUIA_Cycle_Entries, (IPTR)networks);
                /* Activer quand même pour afficher le message */
                set(cycNetworks, MUIA_Disabled, FALSE);
            }
        }
        else if (res == 0x103) {
            /* Un réseau a été sélectionné dans le Cycle */
            get(cycNetworks, MUIA_Cycle_Active, &selectedNetwork);
            
            /* Copier le nom du réseau sélectionné dans le champ SSID */
            if (selectedNetwork < networkCount && networks[selectedNetwork]) {
                /* Ne pas copier le message "Click SCAN..." */
                if (strcmp(networks[selectedNetwork], "Click SCAN to list networks") != 0 &&
                    strcmp(networks[selectedNetwork], "No networks found") != 0) {
                    char cleanSSID[33];
                    /* Nettoyer le SSID (enlever les mentions de fréquence) */
                    StripFrequencyTags(networks[selectedNetwork], cleanSSID, sizeof(cleanSSID));
                    set(strSSID, MUIA_String_Contents, (IPTR)cleanSSID);
                }
            }
        }
        else if (res == 0x200) {
            /* Menu About */
            LONG result;
            result = MUI_Request(app, win, 0, "About WirelessSettings", "_Visit GitHub|_OK",
                        "\33c\33bWirelessSettings v1.1 (06.02.2026)\33n\n\n"
                        "WiFi Configuration Tool for AmigaOS\n\n"
                        "Renaud Schweingruber\n"
                        "renaud.schweingruber@protonmail.com\n\n"
                        "https://github.com/TuKo1982/WirelessSettings");
            
            /* Si l'utilisateur clique sur "Visit GitHub" (retourne 1) */
            if (result == 1) {
                BPTR nullFh;
                /* Utiliser la commande OpenURL si disponible */
                nullFh = Execute((STRPTR)"C:OpenURL https://github.com/TuKo1982/WirelessSettings", 0, 0);
            }
        }

        if (running && sigs) {
            sigs = Wait(sigs | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C) running = FALSE;
        }
    }

    /* Nettoyage */
    FreeNetworksList(networks, networkCount);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    return 0;
}
