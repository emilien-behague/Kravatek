# Architecture 0.2

## Moteur sans JUCE

`KickSynth` conserve la voie sinusoïdale de référence. Les paramètres sont validés avant allocation/rendu. Toutes les phases commencent à zéro ; le générateur entier de bruit et les mutations sont déterministes. Les bibliothèques mathématiques peuvent produire des différences de derniers bits entre plateformes.

La voie enrichie fonctionne à 4 fois la fréquence de sortie, ou 8 fois en dessous de 24 kHz :

1. Pitch paramétrique du body ; modulation combinée de fréquence et phase par un oscillateur à ratio 1,73, avec décroissance indépendante. Partiels impairs 3/5/7.
2. Transitoire bruité filtré, enveloppe de body avec attaque et extinction cosinus, décroissance à courbure réglable.
3. Saturation tanh et morphing vers un sine-fold borné.
4. Tail à oscillateur propre : ratio, extinction, rebond de pitch, interaction de phase avec le body et drive évolutif. Montée cosinus de 12 ms.
5. Passe-bas résonant à topologie préservée sur body et tail. Damping positif, cutoff borné et sweep exponentiel.
6. Sub sinusoïdal à la fondamentale cible, mixé **après** le filtre, avec extinction indépendante.
7. Filtre FIR Blackman centré puis décimation ; tapers de bord, correction DC pondérée et normalisation de crête à −3 dBFS.

Le suréchantillonnage est justifié par une régression spectrale sur saturation ; il ne garantit pas l'absence d'aliasing sur toute combinaison extrême. Le filtre FIR atténue aussi la zone haute (coupure de conception à 0,4 fois la fréquence de sortie). La correction DC et la normalisation portent sur le mix final : verrouiller une couche ne garantit pas sa stricte égalité dans un mix qui change de gain ou d'interaction. Le test d'isolation vérifie son indépendance lorsque le sub est écouté seul.

`Variations` sépare presets, mutation, génération et évolution. Voir [le contrat d'exploration](EXPLORATION.md).

`Analysis` calcule crête, RMS sur la durée totale, DC, durée et FFT radix-2 de 8192 points, fenêtre Hann sur le début du kick, affichage logarithmique de 64 bandes (maximum spectral de chaque bande, dBFS, plancher −120). La fréquence affichée comme F0 TARGET est la consigne du moteur : aucun détecteur de fondamentale polyphonique n'est revendiqué.

## Lecture et application

`Playback` possède ses buffers avant le callback, communique par atomiques et ne verrouille ni n'alloue pendant le traitement audio. La boucle insère du silence jusqu'au prochain temps ; une tail dépassant ce temps est lissée sur les 5 dernières ms. STOP et retrigger utilisent une courte extinction. Les tests couvrent le séquencement et la concurrence.

Le rendu hors ligne est effectué sur le thread d'interface, coalescé par un timer de 30 ms, hors callback. C'est une limite de réactivité à surveiller pour les futurs moteurs plus coûteux. L'export utilise le rendu 48 kHz ; la préécoute recalcule le même moteur à la fréquence réelle du périphérique. A/B compare le RMS sur 400 ms avec silence ajouté pour les kicks plus courts ; seul le plus fort est atténué, sans amplification. Cela égalise l'énergie de cette fenêtre, pas la sonie perceptuelle LUFS.

`PresetState` sérialise tous les paramètres et graines, verrous, famille, tempo, NOVELTY et EVOLVE avec un schéma versionné et 17 chiffres de précision. L'import refuse les types invalides, champs absents et valeurs hors plages de l'éditeur avant de toucher l'état courant. L'historique contient 24 états de session, sauvegardables individuellement ; il n'est pas une base de données persistante.
