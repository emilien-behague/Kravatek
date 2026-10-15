# Validation 0.4 — 15 octobre 2026

Les rapports [0.3](VALIDATION-003.md), [0.2](VALIDATION-002.md) et [001](VALIDATION-001.md) restent historiques. [MISSION-004](MISSION-004.md) contient l'audit, le périmètre livré et les fichiers modifiés.

## Vérifications exécutées sous Linux

- Baseline 0.3 : build, 35 tests et GUI réussis avant modification ; tag `archive/pre-mission-004` et WAV/JSON témoins.
- Build Release C++20/JUCE 8.0.16-security réussi avec GCC 14.2. **46/46 tests CTest réussis**, aucun ignoré : 22 historiques, 8 génétiques, 6 universal DSP, 5 persistance/historique, 5 profils.
- **36/36 tests DSP réussis en Debug avec AddressSanitizer et UndefinedBehaviorSanitizer**. Les dix tests JSON/profils utilisant JUCE ne font pas partie de cette suite instrumentée.
- Six fondations d'architecture distinctes et déterministes à 44,1/48 kHz ; huit distorsions distinctes ; effet réel de chaque contrôle FX dans un contexte actif, stabilité des paramètres extrêmes, crête/DC/bords vérifiés.
- Accordage et estimation aux notes MIDI 24, 33 et 60 testés aux deux fréquences ; référence 55 Hz mesurée. Les numéros MIDI servent à l'accordage, pas à recevoir des événements MIDI.
- Rendu, buffer de lecture et WAV complets de **4 secondes à 192 kHz** testés, sans troncature du buffer.
- Régression d'alias UKE ciblée : composante 600 Hz à **−73,26 dB** de la fondamentale 1800 Hz, tanh drive 30 dB et rendu 48 kHz. Les cas GSE/Classic restent testés. Cette mesure ne valide pas tous les traitements/réglages extrêmes ; le numérique produit volontairement de la texture.
- Catalogue : huit profils validés, distributions/graines déterministes, refus des plages/champs inconnus/probabilités invalides et fingerprints incohérents. Les caractères et architectures forcées changent le signal dans leurs contextes actifs.
- Campagne : **48 WAV/JSON** (8 profils × 3 graines × 2 fréquences), 24 audios distincts à 48 kHz ; paramètres, crêtes, DC, bords, reproductibilité, relecture JSON et mesures vérifiés. Une session de 24 sons et deux CSV sont produits pour l'écoute.
- Verrous sur tous les groupes aux deux fréquences, mutation nulle, parents immuables et couches de versions mixtes ; SUB/ATTACK 003 restent identiques dans un enfant 004.
- Les cinq WAV témoins 003 restent identiques octet pour octet après rappel. Les sauvegardes mixtes 003/004 et les recettes complètes de profils sont rechargées exactement.
- Contrôle indépendant Python `wave` : **16 exports UKE PCM24**, fréquences, tailles, crêtes, DC et bords ; chaque rappel JSON produit un WAV identique octet pour octet.
- Smoke GUI réel sous Xvfb/ALSA silencieux : ancien parcours 20 → 8, 32 candidats via les huit profils du navigateur, longue 808 accordée à 55 Hz, ouverture du panneau SYNTH/FX, édition distorsion/phase, 8 mutations UKE SUB/ATTACK exactes, A/B ajusté, SAVE/OPEN, historique sur **72 états** et accès en petit viewport. Callbacks audio JUCE exécutés, **aucune écoute physique revendiquée**.
- `tools/setup_cloud.py` réexécuté sur la 0.4 : dépendances locales, build et 46 tests ; aucune installation système. Workflow vérifié par actionlint 1.7.7 sans erreur.

## Références mesurées

Graine individuelle 2, NOVELTY 60 %, 48 kHz. Durée effective : dernier échantillon au-dessus de −60 dB de la crête. RMS sur le fichier complet, centre spectral pondéré en puissance avec prise en compte du début du signal. Les variations de niveau restent présentes ; elles ne constituent pas une note de qualité.

| Profil | Durée utile | RMS | Centre spectral | Énergie <200 Hz |
| --- | --- | --- | --- | --- |
| house.classic | 298 ms | -22.16 dBFS | 64.5 Hz | 98.7 % |
| techno.classic | 300 ms | -21.15 dBFS | 112.1 Hz | 88.5 % |
| psytrance.full-on | 145 ms | -20.21 dBFS | 93.4 Hz | 95.1 % |
| hard-techno.driven | 349 ms | -15.52 dBFS | 171.3 Hz | 89.0 % |
| hardstyle.tonal | 826 ms | -14.85 dBFS | 93.7 Hz | 92.9 % |
| hardcore.driven | 358 ms | -13.46 dBFS | 119.8 Hz | 89.1 % |
| 808.long | 2407 ms | -18.37 dBFS | 39.4 Hz | 100.0 % |
| experimental.hybrid | 666 ms | -19.94 dBFS | 287.0 Hz | 63.3 % |

Les CSV conservent aussi F0/confiance, crête, trois bandes, transient et gain de sécurité. L'énergie <200 Hz comprend des harmoniques basses : ce n'est pas une mesure isolée du SUB. Le solo SUB permet d'en écouter la couche séparément. Les différences descripteurs et les hashs distincts ne prouvent pas la qualité musicale ou l'originalité.

## Fichiers et reproduction

```bash
python3 tools/setup_cloud.py
python3 tools/run_cloud_smoke.py
python3 tools/validate_genetic_exports.py --renderer build-linux/krava-genome-render --output out/examples-004 --universal
```

- `build-linux/KravaKickLab` : exécutable **Linux**.
- `build-linux/test-results-004.xml`, `build-sanitized/test-results-004-sanitized.xml` : tests réellement exécutés.
- `build-linux/test-output/universal-reference/` : 48 WAV/JSON, `reference.session.json`, `measurements.csv`, `profile-distances.csv`.
- `out/Universal-Reference-004.zip` : copie regroupée des références ; les sorties sont ignorées par Git.
- `out/examples-004/validation.json` : contrôles PCM24 indépendants et SHA-256.
- `out/cloud-smoke/gui-smoke.json`, `universal-session.json`, `gui-smoke-004.wav`, `.krava.json`, `instrument-004.png` : scénario réel.
- `docs/images/instrument-004.png` : capture compilée conservée dans Git.

Le catalogue source est `profiles/core-004.json`. Le paquet utilise sa copie externe `Profiles/core-004.json`. Le chargement externe dans le cloud a été exercé par l'application ; le fallback intégré provient du même fichier source. Les programmes n'exigent aucun sample externe.

## Windows

Compilation MSVC x64 et livraison 0.4 en attente de vérification après publication. Le workflow produit **KRAVA-KICK-LAB-Windows-x64** ; le chemin de compilation est `build/windows-x64/Release/KravaKickLab.exe`. Le paquet contient l'exécutable à sa racine, les deux CLI, le guide, le catalogue et `Examples/Universal/`. Le résultat effectif sera consigné ici après le run, sans réutiliser le succès de la 0.3 comme preuve.

## À évaluer humainement

Écoute du poids, du punch, de la tonalité, des distorsions et de la place avec une basse ; confort de l'interface ; sortie audio et dialogues natifs sur le PC Windows ; import dans FL Studio. L'analyse automatique ne valide aucune de ces appréciations musicales.

La première livraison couvre huit profils. Les profils spécialisés extrêmes et les autres sous-genres, l'hybridation pondérée à deux genres et le MIDI entrant restent à développer. LOOP tronque une tail dépassant le temps, avec fondu ; ONE-SHOT conserve sa durée. Bibliothèque de 512 états ; courbes paramétriques ; certains rendus de préécoute/édition synchrones. L'IA connectée, le prompteur, VST3 et l'import de samples ne sont pas implémentés.
