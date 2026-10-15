# KRAVA-KICK LAB 0.3 — suivi technique

Le nom produit demandé est **KRAVA-KICK LAB**. Le nom historique du dépôt et le namespace C++ ne changent pas les fichiers de l'utilisateur. La référence graphique fournie redevient la direction : bleu électrique quadrillé, rose/cyan, fenêtres rétro et marque immédiatement lisible. Le graphite/jaune de la 0.2 n'a pas été validé.

## Audit avant modification

État de départ : `110573b`, conservé sous `archive/pre-mission-003` et `v0.2.0`. Compilation incrémentale de l'application réussie, 22 tests CTest réussis, test GUI réel réussi sous Xvfb/ALSA silencieux. Ce dernier vérifie lecture, courbes, génération, sauvegarde/rechargement audio exact, A/B et export. Une copie d'un preset édité et du WAV Warehouse est conservée dans `out/baseline-002/`.

La 0.2 possède 24 paramètres, huit bases, FM/PM, saturation suréchantillonnée, filtre résonant, sub/tail, JSON et A/B. Ses limites pour cette mission sont structurelles :

- Les distributions changent surtout les valeurs d'une architecture commune.
- L'attaque passe dans le traitement du corps ; sub et tail dépendent d'autres réglages. Les verrous ne garantissent pas les signaux isolés.
- Chaque rendu est normalisé à la même crête, ce qui cache une partie des écarts de niveau.
- Aucun parent/enfant ni bibliothèque persistante de variantes.
- Analyse limitée au spectre initial, crête/RMS/DC/durée ; pas d'estimation F0 ni descripteur de similarité.
- Rendu sur le thread graphique, inadapté aux lots de 20 candidats.

Le rendu historique 002 et son lecteur de presets restent disponibles pour conserver l'audio des fichiers existants. Le moteur 003 ajoute des couches explicites ; une conversion vers lui constitue un nouvel instrument, pas une prétendue restitution identique d'un fichier 002.

## Lot A — couches et identité

Quatre structures autonomes SUB/ATTACK/BODY/TAIL ; chaque rendu de couche dépend uniquement de ses propres gènes et de la fréquence d'échantillonnage. Durée, phase, enveloppe et état aléatoire sont locaux à la couche. Les buffers isolés conservent leur propre longueur avant ajout de silence dans le mix.

Architectures réelles : transitoire bruit/tonal/harmonique/hybride ; body sine/additif/FM-PM/ring ; tail tonale/élastique/résonante/phase warp. Le pitch du body comporte un point intermédiaire et deux segments interpolés en fréquence logarithmique.

Le PRNG est SplitMix32 : incrément `0x9e3779b9`, mélange `0x21f0aaad`, `0x735a2d97`, division par 2^32. Aucun RNG système dans le rendu. Les graines de bruit sont conservées par couche. L'identité est `K3-` suivi du SHA-256 d'une représentation canonique versionnée des gènes, mode, graine, NOVELTY et parents. Deux enregistrements identiques désignent le même kick ; une bibliothèque doit dédupliquer ces identités et refuser un conflit.

Les traitements non linéaires sont calculés à 4x (8x sous 24 kHz), puis décimés par FIR Blackman. Correction DC et extinction sont propres à chaque couche. Mix à gain fixe 0,35 ; atténuation de sécurité uniquement au-delà de −1 dBFS, jamais d'amplification automatique d'un candidat faible. Les buffers isolés restent inchangés lorsque la sécurité du mix intervient.

Les différences de timbre et les mesures ne constituent ni une preuve d'originalité absolue ni une validation musicale. L'écoute sur une sortie physique reste du ressort du producteur.

## Lot B — exploration et descripteurs

Chaque mode possède ses régions de fréquence, durée, harmoniques, FM/PM, drive et tail, et ses probabilités d'architectures. Les flux aléatoires sont séparés par constantes SUB/ATTACK/BODY/TAIL. NOVELTY agit continûment sur l'écart aux fondations et sur une probabilité quadratique de changer d'architecture. Les libellés CONTROLLED (0–30), EXPLORATION (31–70), EXPERIMENTAL (71–100) décrivent cette progression, sans créer trois moteurs discontinus.

Les mesures comprennent crête/RMS/DC, durée au-dessus de −60 dB de la crête, autocorrélation des basses fréquences avec confiance, centre spectral pondéré par la puissance sur l'ensemble du rendu (FFT Hann avec recouvrement), fractions d'énergie <200/200–2000/>2000 Hz, huit segments temporels et énergie/crête des 15 premières ms. La F0 est une estimation de périodicité dominante entre 25 et 150 Hz : les textures complexes peuvent donner un résultat ambigu.

La distance acoustique combine ces descripteurs ; la similarité ajoute la corrélation des formes d'onde alignées au déclenchement. Ces heuristiques ne modélisent pas toute la perception et ne prouvent ni qualité ni originalité. Les variations de niveau restent mesurables. Les tests utilisent des sinusoïdes de référence, le silence et une population de 100 graines fixes rendue aux deux fréquences cibles.
