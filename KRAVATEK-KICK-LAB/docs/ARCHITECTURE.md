# Architecture 0.3

## Moteurs versionnés

Le moteur historique `src/dsp/KickSynth` reste inchangé pour les presets 002 ; CLASSIC 0.2 conserve son éditeur. Le nouveau `src/genetic` ajoute le moteur **KRAVA-GSE-003.1** sans prétendre convertir les anciens sons à l'identique. L'export PCM24 partagé a été comparé à un WAV témoin 002 : résultat identique octet pour octet.

`Genome` contient l'ensemble des gènes SUB/ATTACK/BODY/TAIL, graines, mode, NOVELTY, version, identité SHA-256 et parents. SplitMix32 fournit les flux déterministes. Chaque couche utilise uniquement ses propres gènes et la fréquence de rendu : phases, bruit, filtres, enveloppes et longueur native indépendants. Le pitch du BODY possède deux segments en fréquence logarithmique à travers un point intermédiaire.

`LayerRenderer` suréchantillonne les traitements non linéaires à 4x (8x sous 24 kHz), puis décime par FIR Blackman. Chaque couche reçoit sa correction DC et ses tapers. Le mix additionne à gain fixe 0,35 et n'applique qu'une atténuation de sécurité si nécessaire au-delà de −1 dBFS. Le suréchantillonnage réduit un risque mesuré d'aliasing ; il ne garantit pas l'absence d'alias pour tous les réglages extrêmes.

`Generator` sépare génération et évolution, avec régions/probabilités propres aux cinq modes, interpolation progressive et architectures distinctes. EVOLVE recopie intégralement les groupes verrouillés. `Population` dérive les graines de lot, propose au plus quatre candidats contre les doublons proches et prend en charge annulation/progression. `Library` stocke des snapshots immuables et déduplique les identités, jusqu'à 512 états.

`Features` analyse le signal réel : crête/RMS/DC, durée effective, périodicité grave estimée avec confiance, énergie par bande et par segment, centre spectral et transient. La similarité est une heuristique documentée dans [MISSION-003](MISSION-003.md), pas un jugement musical.

## Persistance et export

`SessionIO` utilise JUCE Core pour le JSON versionné. Les nombres flottants des gènes sont stockés en chaînes décimales à 17 chiffres significatifs, puis lus par `std::from_chars` : les dernières décimales ne dépendent plus de l'arrondi JSON de JUCE. Types, tailles, versions, plages, identités et filiations locales sont validés avant activation.

La session conserve bibliothèque complète, génération visible, A/B, verrous et réglages ; le fichier associé à un WAV conserve un génome et sa fréquence. Le WAV intègre également ce JSON. Les parents externes restent référencés par identité ; seule une session contenant les parents peut les rouvrir. Les sauvegardes 002 nouvelles ajoutent `_exact` sans casser la lecture historique.

## Lecture et interface

`Playback` conserve ses buffers préalloués et son protocole atomique sans allocation/verrou dans le callback. STOP et retrigger utilisent une courte extinction. La boucle lisse les tails dépassant le prochain temps. Les tests de concurrence et de lecture legacy restent actifs.

`GeneticLab` exécute les lots et les rafraîchissements de bibliothèque dans un `std::jthread` annulable ; les résultats rejoignent le thread graphique via SafePointer/callAsync. Les modifications manuelles sont regroupées par timer et créent un enfant à la validation. Le rendu d'une édition, certains solos et la première préécoute restent synchrones : réactivité à vérifier sur PC lent.

La préécoute est rendue à la fréquence réelle du périphérique ; l'export utilise le choix 44,1/48 kHz. A/B ajuste le RMS sur 400 ms avec silence ajouté, par atténuation du plus fort uniquement. MATCH désactivé rétablit gain 1. Le rendu sauvegardé reste inchangé. Le spectre affiché est la FFT Hann initiale héritée ; les descripteurs d'énergie et de centre spectral analysent l'ensemble du rendu.
