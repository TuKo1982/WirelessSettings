# WirelessSettings - Configuration WiFi pour AmigaOS

## Version 1.2 avec scan des réseaux WiFi

### Compilation

#### Méthode 1 : Avec le Makefile
```bash
make
```

#### Méthode 2 : Compilation directe
```bash
gcc -noixemul -s -O2 -Wall -Wno-implicit-int -o WirelessSettings WirelessSettings_modified.c -lamiga
```

**Note importante** : 
- Ne PAS utiliser `-lmui`. MUI est lié automatiquement via les fichiers proto/inline includes.
- Le flag `-Wno-implicit-int` supprime les warnings provenant des anciens headers MUI inline.

### Dépendances

- GCC 2.95 ou supérieur
- MUI 3.8 ou supérieur (muimaster.library)
- AmigaOS 3.x
- La commande `ListNetworks` doit être disponible dans C:
- **Optionnel** : OpenURL pour ouvrir le lien GitHub depuis le menu About

### Installation

```bash
make install
```

Ou manuellement :
```bash
copy WirelessSettings SYS:Prefs/
```

### Utilisation

1. **Scanner les réseaux** : Cliquez sur le bouton "Scan" (la liste déroulante est grisée jusqu'au scan)
   - *Note* : Le bouton Scan est désactivé si la commande C:ListNetworks n'est pas installée
2. **Sélectionner un réseau** : Une fois le scan terminé, la liste s'active et vous pouvez choisir un réseau
3. **Configurer** : Le SSID est automatiquement rempli (sans les mentions de fréquence)
4. **Choisir le type de chiffrement** : Open / WEP / WPA/WPA2
5. **Entrer le mot de passe** : Si nécessaire (désactivé pour "Open")
6. **Sauvegarder** : Cliquez sur "Save"
7. **Charger** : Cliquez sur "Load" pour recharger la configuration
8. **À propos** : Menu Project → About pour voir les informations et accéder au GitHub (si OpenURL est installé)

### Format du fichier ENV:WiFiNetworks

Le fichier doit contenir un nom de réseau par ligne. Les mentions de fréquence sont automatiquement filtrées :

```
MonReseau
WiFi-Voisin (5 GHz)
Hotspot-Public (2.4 GHz)
```

Dans l'exemple ci-dessus, si l'utilisateur sélectionne "WiFi-Voisin (5 GHz)", seul "WiFi-Voisin" sera copié dans le champ SSID.

### Warnings GCC

Les warnings concernant `type defaults to int` dans les inline MUI sont normaux avec GCC 2.95 et les anciens headers MUI. Ils peuvent être ignorés.

### Gestion des erreurs

**Périphérique sans support du scan WiFi** :
Si votre périphérique ne supporte pas le scan des réseaux, l'application :
- Affichera une alerte "This device does not support wireless network scanning"
- Vous invitera à entrer le SSID manuellement
- Maintiendra la liste des réseaux grisée
- Permettra quand même la configuration manuelle complète

### Fichiers de configuration

- **ENVARC:Sys/wireless.prefs** : Configuration WiFi sauvegardée
- **ENV:WiFiNetworks** : Liste temporaire des réseaux scannés

### Changelog

**v1.2** (07.02.26)
- Bouton Scan désactivé si C:ListNetworks n'est pas installé

**v1.1** (06.02.26)
- Ajout du bouton SCAN
- Ajout de la liste déroulante des réseaux
- Sélection automatique du SSID depuis la liste
- Gestion dynamique de la mémoire pour les réseaux
- Filtrage automatique des mentions de fréquence (5 GHz / 2.4 GHz)
- Liste des réseaux désactivée jusqu'au premier scan (amélioration UX)
- Détection et gestion des périphériques ne supportant pas le scan WiFi
- Ajout du menu Project > About avec informations sur l'application
- Support OpenURL pour ouvrir le GitHub depuis le dialogue About

**v1.0** (04.02.26)
- Version initiale
