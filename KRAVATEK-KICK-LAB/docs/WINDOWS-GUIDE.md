# KRAVA-KICK LAB 0.4 — Universal Kick Engine

1. Décompresse entièrement l'archive et ouvre **KravaKickLab.exe**.
2. Pour comparer immédiatement les huit familles : **OPEN → Examples/Universal/reference.session.json**. Les 24 sons sont déjà dans la bibliothèque. Un clic sélectionne, un double-clic écoute. Désactive **LOOP** pour entendre les longues 808 entièrement.
3. Choisis **GENRE**, puis **SUBGENRE / PROFILE**. Le navigateur règle les prochaines générations : il ne remplace pas le son déjà sélectionné. Les huit profils actuels sont Classic House, Classic Techno, Full-On Psytrance, Driven Hard Techno, Tonal Hardstyle, Driven Hardcore, Long Tuned 808 et Hybrid Experimental.
4. Règle **NOVELTY**, **COUNT 4/8/12/20**, puis **GENERATE**. **REGENERATE** produit un nouveau candidat. Même graine de lot et mêmes réglages donnent le même lot dans les mêmes conditions de calcul.
5. **CHARACTER** transforme le caractère ; **NEXT ARCHITECTURE** force une architecture de synthèse, même hors de l'esthétique habituelle du genre. **NEXT NOTE** accorde les prochains sons. Les noms de notes utilisent les numéros MIDI, mais cette version ne reçoit pas de clavier MIDI.
6. Sélectionne ton préféré, active **LOCK SUB / ATTACK / BODY / TAIL**, puis **EVOLVE**. Le parent reste intact. Tu peux changer de genre avant EVOLVE pour aller progressivement vers une autre famille ; ce n'est pas encore un mélange pondéré de deux profils.
7. Pour comparer : sélectionne un son, **SET A** ; sélectionne un autre, **SET B** ; puis **A >**, **B >** ou **A/B**. **MATCH RMS** atténue le plus fort sur une fenêtre de 400 ms. Désactive-le pour les niveaux réels. Les fichiers ne changent pas. Ce n'est pas une égalisation LUFS.
8. **SYNTH / FX** ouvre les pages BODY FX, TAIL FX et SYNTH/GLIDE : distorsion, pré/postfiltre, seconde saturation, multibande, quantification/sample hold, enveloppe de drive, phase du sub, glides et membrane. Les paramètres grisés sont sans effet dans le contexte choisi. Cliquer en dehors ferme le panneau.
9. Les potentiomètres et les trois points PITCH/AMP éditent le son. **KEEP EDIT** ajoute un enfant ; SAVE/EXPORT/EVOLVE le font également. Les verrous concernent EVOLVE, pas une édition manuelle volontaire. Les valeurs sont saisissables au clavier.
10. **SAVE** conserve la session complète. **EXPORT WAV** avec **WAV + JSON** activé écrit du mono PCM 24 bits à 44,1 ou 48 kHz et son fichier de paramètres. Glisse le WAV dans FL Studio.

**TAIL > BEAT** signale une durée utile dépassant un temps au tempo choisi. Le lecteur LOOP actuel tronque alors la tail avec une courte extinction : désactive LOOP pour entendre le son complet. Les recommandations BPM des profils ne bloquent aucun genre ; le lecteur accepte 40–400 BPM.

Si tu n'entends rien, ouvre **AUDIO** et choisis ta sortie. Les barres de défilement donnent accès à l'ensemble sur petit écran. HISTORY retrouve parents et lots ; jusqu'à 512 états par session. Pour repartir au-delà : sauvegarde et ouvre une nouvelle session ou relance l'application.

**Examples/Universal** contient 48 WAV/JSON (8 profils × 3 graines × 2 fréquences), les mesures CSV et la session de 24 sons. Les exemples GSE 0.3 et Classic sont conservés. Choisis GENRE **GENETIC 0.3** pour les cinq anciens modes ; **CLASSIC 0.2** retrouve l'éditeur historique. Les anciens presets s'ouvrent sans conversion audio forcée.

NOVELTY n'est pas un score d'originalité. Les verrous préservent les signaux isolés ; les autres couches peuvent en changer la perception dans le mix. La reproduction exacte demande paramètres complets, version du moteur, fréquence et mêmes conditions mathématiques. Une graine seule ne remplace pas SAVE.

À écouter ensemble : rondeur House, articulation Psy avec une bassline, agressivité Hard Techno/Hardcore, tonalité Hardstyle, longueur/glide des 808 et intérêt des mutations. Les mesures ne valident pas la qualité musicale.

Les profils spécialisés Schranz, Rawstyle, Gabber, Frenchcore et Uptempo, les autres sous-genres, l'hybridation pondérée de deux genres et le MIDI entrant sont les étapes suivantes. Le mode Schranz historique reste disponible. IA/prompteur, VST3 et import de samples ne sont pas développés.
