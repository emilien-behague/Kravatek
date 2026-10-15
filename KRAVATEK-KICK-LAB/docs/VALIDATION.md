# Validation 0.4.1 — changement de style

## Problème corrigé

En 0.4.0, choisir un genre modifiait uniquement la recette des prochaines générations. Le son sélectionné restait identique. De plus, une boucle déjà lancée pouvait conserver son ancien buffer après une génération ou la sélection d'une carte. La validation complète de cette version reste dans [VALIDATION-004.md](VALIDATION-004.md).

En 0.4.1, GENRE / PROFILE / CHARACTER / ARCHITECTURE / NOTE et les modes GSE rendent puis écoutent un candidat, sans changer COUNT ni écraser l'historique. Un changement de famille réinitialise les surcharges de caractère, architecture et note. DNA LOCK utilise le chemin d'évolution existant et conserve les couches concernées. Quatre verrous expliquent explicitement pourquoi le son est conservé. STOP pendant un rendu empêche la relance ; LOOP désactivé donne une écoute unique. Les sélections de cartes et les lots terminés remplacent également le buffer d'une boucle active.

Les moteurs DSP, versions de rendu, catalogue et formats de paramètres restent identiques à la 0.4.0.

## Vérifications exécutées dans le cloud

- Baseline `d480059` / tag `v0.4.0` : **46/46 tests réussis** avant modification ; sauvegarde Git existante conservée.
- Build Release C++20/JUCE 8.0.16-security / GCC 14.2 de la 0.4.1 réussi.
- **46/46 tests CTest réussis**, zéro ignoré ; rapport `build-linux/test-results-041.xml`. Les campagnes de génération, diversité, couches verrouillées, persistance et exports restent actives.
- Application JUCE réelle sous Xvfb et ALSA silencieux : test GUI réussi, version **0.4.1**, callbacks audio exécutés.
- Régression UI : **18 actions sur les menus**, **17 écoutes acceptées** ; l'action arrêtée par STOP n'est pas relancée. Les huit familles sont choisies avec la **même graine de lot 78000** ; chaque sélection donne un signal différent du précédent et son propre profil effectif.
- Vérification exacte du buffer transmis au lecteur : mix du candidat sélectionné à la fréquence réelle du périphérique, sortie du solo et de l'A/B, sans gain de comparaison résiduel.
- CHARACTER, architecture, accordage C3 / MIDI 48, profil chargé depuis un snapshot, modes historiques, mutation avec SUB/ATTACK verrouillés et cas des quatre couches verrouillées vérifiés.
- Ancien son immuable, un seul candidat par aperçu, COUNT préservé ; nouvelle génération et clic simple d'une carte pendant la boucle transmettent le nouveau signal au lecteur.
- Les scénarios antérieurs restent exercés : 20 candidats GSE → 8 enfants ; 32 candidats UKE / 8 profils → 8 enfants ; SAVE/OPEN, PCM24/JSON, panneau SYNTH/FX, A/B et petit écran. Historique final : **94 états**.
- Workflow contrôlé par actionlint 1.7.7 sans erreur. Le script cloud et le job Windows exigent explicitement les résultats de la régression de style.

Rapport conservé : [gui-041.json](reports/gui-041.json). Capture compilée : [instrument-041.png](images/instrument-041.png). Sorties complètes : `out/cloud-smoke/`. Aucun test d'écoute humaine n'est revendiqué. Les ASan/UBSan de la 0.4.0 sont historiques ; ils n'ont pas été réexécutés pour ce correctif d'interface.

## Windows

Compilation et tests Windows à lancer sur le commit du correctif ; aucun nouveau `.exe` Windows n'est annoncé avant leur réussite. Le paquet attendu est `KRAVA-KICK-LAB-Windows-x64` ; l'exécutable est `KravaKickLab.exe` à sa racine, et `build/windows-x64/Release/KravaKickLab.exe` dans le job.

## Essai simple

1. Ouvrir **KravaKickLab.exe 0.4.1**.
2. Sans DNA LOCK, choisir **HOUSE**, puis **HARDCORE**, puis **808 / HIP-HOP** : chaque sélection calcule et joue son aperçu, sans appuyer sur GENERATE.
3. Désactiver LOOP pour entendre la longue 808 en entier ; changer NOTE permet de vérifier l'accordage.
4. Verrouiller SUB et ATTACK, puis changer de style : ces couches sont conservées et le reste évolue. HISTORY retrouve les précédents sons.

Les limites DSP, les huit profils réellement disponibles et les futures familles restent ceux documentés dans [MISSION-004.md](MISSION-004.md). Le rendu demande un court calcul en arrière-plan ; le menu est désactivé pendant celui-ci. La bibliothèque reste limitée à 512 états. Le ressenti musical et le fonctionnement sur la sortie audio du PC sont à valider à l'écoute par le créateur.
