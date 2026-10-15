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

## Lot C — DNA au niveau des échantillons

EVOLVE copie entièrement les gènes de chaque couche verrouillée, y compris phase, graine, enveloppes et niveaux. Les couches déverrouillées sont interpolées vers une proposition du mode choisi. Le raffinement à faible amplitude conserve les architectures et graines internes ; l'exploration permet leur remplacement progressif. Les parents sont passés par référence constante et jamais modifiés.

Le test SUB+ATTACK compare les vecteurs flottants complets, avec leur longueur native, sur 20 mutations extrêmes à chacune des deux fréquences cibles. Il vérifie aussi BODY+TAIL, tous verrouillés, évolution nulle, provenance et intégrité du parent. Cette garantie concerne les couches isolées avant gain commun du mix. Leur perception peut évoluer lorsque d'autres couches changent, sans modification de leur signal isolé.

## Lot D — populations et sessions

Les lots 4/8/12/20 possèdent des graines dérivées explicitement de la graine de lot. Chaque enfant conserve sa graine effective et l'identifiant de son parent. La bibliothèque ajoute des snapshots immuables et déduplique les identités ; elle ne remplace jamais le parent. Jusqu'à 512 kicks sont conservés par session. Le raffinement conserve les variations subtiles. En exploration, une similarité supérieure à 0,985 déclenche au plus quatre propositions déterministes ; si aucune n'est assez différente, la proximité reste signalée. Tous les verrous actifs produisent volontairement le même audio et ne déclenchent pas de boucle de recherche infinie.

Les sessions JSON conservent les génomes, filiations, générations visibles, sélections A/B, paramètres du laboratoire et fréquence de rendu. Les fichiers associés à un WAV contiennent le génome sélectionné, sa provenance et sa fréquence. Les parents externes peuvent être référencés par leur identité ; SAVE SESSION conserve l'ensemble de l'historique chargé.

Une régression a révélé une perte de derniers bits dans les conversions numériques JSON de JUCE pour des petites durées. Les nouveaux gènes flottants sont donc écrits en chaînes décimales à 17 chiffres significatifs et relus avec `std::from_chars`. Les nouvelles sauvegardes 002 ajoutent un objet `_exact` ; la lecture des anciens fichiers sans cet objet conserve le comportement précédent. Tests : round-trip exact de 52 génomes et de 100 presets legacy, comparaison audio après rechargement, refus des versions/identités/paramètres invalides, WAV PCM 24 bits avec métadonnées, historique et populations reproductibles.

L'outil `krava-genome-render` permet de générer des exemples et de rerendre un kick depuis une session sauvegardée sans interface. Le WAV Warehouse historique a été comparé au témoin d'avant modification et reste identique octet pour octet.

## Lot E — laboratoire utilisable

L'application démarre dans GENETIC LAB. Le bouton CLASSIC 0.2 conserve l'ancien éditeur et les presets 002 s'y ouvrent automatiquement. Le nouveau nom produit et l'identité bleu quadrillé/rose/cyan suivent la correction artistique du créateur. Capture réelle : [instrument-003.png](images/instrument-003.png).

Les lots sont rendus par un worker annulable, avec progression et publication des résultats sur le thread graphique. Les cartes affichent onde réelle, nom, graine, génération, durée et RMS. HISTORY retrouve les parents et les lots précédents ; SAVE conserve toute cette bibliothèque. KEEP EDIT ajoute un enfant lorsque les paramètres ou courbes sont édités : l'original reste intact. Les paramètres sans effet pour l'architecture courante sont désactivés.

PITCH permet de déplacer le départ, le véritable point intermédiaire et l'arrivée ; AMP agit sur attaque/courbure/extinction du body. LISTEN permet d'écouter séparément SUB, ATTACK, BODY ou TAIL, avec leur forme d'onde réelle. L'export reste celui du mix. Les deux sélecteurs A/B acceptent tout kick de la bibliothèque. MATCH RMS atténue uniquement le plus fort selon une fenêtre de 400 ms, à la fréquence réelle du périphérique, avec silence ajouté aux sons plus courts. Désactiver MATCH RMS restitue les niveaux réels. Cette comparaison n'est pas une égalisation de sonie LUFS.

Le test GUI a réellement exécuté : génération de 20 EXPERIMENTAL à 75 %, vérification de 20 signaux distincts, choix d'un parent, 8 mutations SUB+ATTACK verrouillées et identiques au niveau des buffers, parent inchangé, édition pitch intermédiaire/amplitude/drive, A/B ajusté et réel, solo SUB identique, sauvegarde/rechargement exact d'une bibliothèque de 31 états, WAV+JSON et petit viewport avec défilement. La sortie ALSA de test est silencieuse : ce résultat ne constitue pas une écoute physique.

## Fichiers modifiés par la mission

- `src/genetic/Genome`, `LayerRenderer`, `Generator`, `Features`, `Population`, `SessionIO` : identité, couches, exploration, mesures, provenance et persistance.
- `src/app/GeneticLab`, `GeneticGraph`, `GenomeCards` : laboratoire, courbes et bibliothèque.
- `src/app/Main`, `MainComponent`, `InstrumentStyle`, `Windows.rc` : intégration, compatibilité, marque et interface.
- `src/dsp/WavWriter`, `src/app/PresetState`, `src/cli/GeneticMain` : export partagé et sauvegarde exacte, CLI génétique.
- `tests/GeneticTests`, `StateTests`, `LegacyChecks`, `CMakeLists.txt` : vérifications des lots et intégration.
- `tools/run_cloud_smoke.py`, `build-windows.cmd`, `.github/workflows/windows.yml`, README et documentation : validation reproductible et livraison.

## Limites explicites

Le rendu de lots est en arrière-plan ; l'édition d'un seul son, certains solos et le premier rendu de préécoute restent synchrones et peuvent brièvement bloquer l'interface sur une machine lente. Annuler agit entre candidats. Une bibliothèque contient au plus 512 états ; il faut sauvegarder puis ouvrir une nouvelle session pour poursuivre au-delà. Les verrous régissent EVOLVE, pas une édition manuelle volontaire.

Les courbes sont paramétriques à trois poignées, pas des enveloppes multipoints libres. Les gènes sont tous persistés, mais l'interface expose une sélection de paramètres à forte valeur ajoutée. La comparaison acoustique et la F0 restent des estimations ; aucun test ne valide la qualité musicale. Les interactions du mix et son atténuation de sécurité peuvent modifier la perception d'une couche dont le signal isolé reste strictement identique.

Pas de prompteur/IA, VST3, import de samples, multibande, marketplace ni banque commerciale. La reproduction exacte exige aussi la même version de moteur, fréquence, plateforme et conditions mathématiques. Les dialogues natifs Windows, les périphériques du créateur et FL Studio nécessitent encore un essai sur son PC.
