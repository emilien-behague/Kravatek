# KRAVA-KICK LAB 0.3 — à essayer

1. Décompresse toute l'archive et ouvre **KravaKickLab.exe**.
2. Choisis **EXPERIMENTAL**, **NOVELTY 75 %**, **COUNT 20** puis **GENERATE**. Attends les cartes ; CANCEL interrompt entre deux candidats.
3. Double-clique sur les cartes pour écouter. Clique une fois pour sélectionner ton préféré.
4. Active **LOCK SUB** et **LOCK ATTACK**, passe COUNT à **8**, puis clique **EVOLVE**. Le parent reste dans HISTORY et les deux couches verrouillées sont conservées exactement.
5. Sélectionne un son et clique **SET A**, puis un autre et **SET B**. **A >**, **B >** et **A/B** comparent ces sons. **MATCH RMS** ajuste le niveau de préécoute sur 400 ms ; décoche-le pour les niveaux réels. Ce n'est pas une égalisation LUFS.
6. Glisse les points PITCH/AMP ou les potentiomètres. **KEEP EDIT** conserve l'édition comme enfant. Les paramètres grisés sont inactifs pour l'architecture choisie.
7. **SAVE** conserve toute la session et ses parents. **OPEN** la retrouve ; tu peux aussi ouvrir un fichier `.krava.json` d'un seul son.
8. **EXPORT WAV**, avec **WAV + JSON** activé, écrit le mix en PCM 24 bits à 44,1 ou 48 kHz et son fichier de paramètres. Glisse le WAV dans FL Studio. LISTEN permet les solos mais l'export reste le mix complet.
9. Essaie aussi **DEEP**, **HARD PSY**, **SCHRANZ**, **MUTANT** et **REFINE** pour des variations plus petites. REGENERATE produit un nouveau candidat ; GENERATE avec les mêmes réglages et graine reproduit le lot.

**PLAY** ou espace écoute ; un deuxième appui arrête LOOP. Si tu n'entends rien, ouvre **AUDIO** et choisis ta sortie. Utilise les barres de défilement sur petit écran. **HISTORY** retrouve les lots et parents précédents ; la session garde au plus 512 états.

Les exemples du dossier **Examples** sont écoutables directement dans ton logiciel musical. `Experimental-20/kick.session.json` ouvre un lot de 20 dans le laboratoire. Les anciens exemples restent dans `Examples/Classic` ; **CLASSIC 0.2** conserve l'ancien éditeur, et OPEN reconnaît ses presets.

NOVELTY indique l'intensité d'exploration, pas l'originalité. Les verrous protègent EVOLVE, pas une édition manuelle. Le mix peut changer la perception d'une couche isolée identique. La reproduction exacte nécessite paramètres complets, fréquence, version du moteur et mêmes conditions de calcul.

À évaluer ensemble : poids du sub, attaque, grain, place avec ta basse, intérêt des mutations, lisibilité et confort des commandes. Les tests automatiques vérifient la fiabilité ; ils ne remplacent pas ton écoute. IA/prompteur, VST3, import de samples et multibande ne sont pas disponibles.
