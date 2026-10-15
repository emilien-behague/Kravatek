# Validation 0.3 — 15 octobre 2026

Les rapports [001](VALIDATION-001.md) et [0.2](VALIDATION-002.md) restent historiques. Le [suivi de mission](MISSION-003.md) contient l'audit initial, les fichiers modifiés et les limites détaillées.

## Résultats réellement exécutés sous Linux

- Build Release application C++20/JUCE et CLI réussi, GCC 14.2, JUCE épinglé `ea77444750de45e036e0279bb0291144bf8e3688`.
- **35/35 tests CTest réussis**, aucun ignoré : 22 régressions historiques, 8 tests génétiques, 5 tests de persistance/historique/export.
- **30/30 tests DSP réussis sous AddressSanitizer et UndefinedBehaviorSanitizer**, en Debug sans JUCE. Les 5 tests JSON/GUI ne font pas partie de cette suite instrumentée.
- Baseline 0.2 compilée et testée avant modification, copie de presets/audio et point Git `archive/pre-mission-003`.
- Génération de paramètres valides pour les cinq modes et plusieurs niveaux NOVELTY ; PRNG et identité SHA-256 vérifiés sur références. Graine identique : paramètres/audio identiques ; graines différentes : différences constatées.
- Verrous testés sur tous les groupes, dont SUB+ATTACK sur 20 mutations extrêmes aux deux fréquences cibles ; comparaison exacte des vecteurs audio isolés, longueurs incluses. Parents immuables, mutation nulle et tous-verrouillés contrôlés.
- JSON : round-trip exact de 52 génomes et de 100 presets legacy, rejet de sessions/identités/plages invalides, populations 4/8/12/20 et sauvegarde/relecture audio exacte.
- WAV PCM24, fréquence, taille, métadonnées, quantification et signaux invalides testés. Le WAV Warehouse 002 reste identique octet pour octet au témoin antérieur.
- Références sinusoïdales pour crête/RMS, centre spectral, bandes et F0 ; silence et petits buffers contrôlés. La confiance F0 ne garantit pas une note unique pour les textures complexes.
- Régression d'alias ciblée GSE : **−66,39 dB** à 600 Hz par rapport à une fondamentale 1800 Hz, drive 30 dB, 48 kHz. Le test historique reste à **−69,57 dB**. Ces mesures ne prouvent pas l'absence d'aliasing dans tous les cas extrêmes.
- Test GUI réel : parcours 20 EXPERIMENTAL à 75 % → parent sélectionné → 8 mutations SUB+ATTACK exactes → édition → A/B ajusté/réel → solo → sauvegarde/relecture d'une bibliothèque de 31 états → WAV/JSON. Les fonctions CLASSIC 0.2 restent aussi vérifiées. Le petit viewport est parcouru par défilement.
- Callbacks JUCE exécutés sur une sortie ALSA virtuelle silencieuse ; **aucune écoute physique revendiquée**.
- Dix exports des cinq modes à 44,1/48 kHz lus indépendamment par `wave`/Python : mono, PCM24, fréquence, longueur, crête, DC et bords vérifiés ; chaque rechargement depuis `.krava.json` produit un WAV identique octet pour octet. Script : `tools/validate_genetic_exports.py`, mesures : `out/examples-003/validation.json`.
- Workflow Windows contrôlé avec actionlint 1.7.7, sans erreur.

## Population fixe de 100 candidats

100 graines réparties entre les cinq modes, rendues à 44,1 et 48 kHz, soit 200 rendus. Aucun plantage, NaN, infini ou dépassement de crête permis par les assertions.

| Mesure | Résultat Linux Release |
| --- | --- |
| Identités / audios distincts à 48 kHz | 100 / 100 |
| Architectures BODY / TAIL représentées | 4 / 4 |
| Distance moyenne / maximale des descripteurs | 0,178927 / 0,427551 |
| RMS min / max du fichier complet | −27,3683 / −12,9234 dBFS |
| Crête maximale | −3,9796 dBFS |
| Réductions de sécurité dans cette population | 0 |
| Temps observé avec d'autres tests concurrents | 15,4 s pour 200 rendus |

Ce temps n'est pas un benchmark garanti sur le PC du créateur. La plage de RMS confirme que la dynamique n'est pas cachée par une normalisation systématique. La diversité des descripteurs ne démontre ni qualité musicale ni originalité absolue. Les fortes variations entre échantillons de textures agressives ne constituent pas, seules, un détecteur de clics ; les régressions de continuité utilisent des signaux contrôlés.

## Reproduction et fichiers locaux

```bash
python3 tools/setup_cloud.py
python3 tools/run_cloud_smoke.py
```

- `build-linux/KravaKickLab` : application **Linux** ; les noms des anciens fichiers personnels sont inchangés.
- `build-linux/test-results-003.xml`, `build-linux/test-output/genetic-100-report.json` : suite complète et population fixe.
- `build-sanitized/test-results-003-sanitized.xml` : suite DSP ASan/UBSan.
- `out/cloud-smoke/gui-smoke.json`, `genetic-session.json`, `gui-smoke-003.wav`, `.krava.json`, `instrument-003.png` : scénario GUI réel.
- `docs/images/instrument-003.png` : capture réelle conservée dans Git.

Les builds et sorties sont ignorés par Git. Les détecteurs mémoire utilisent `KRAVATEK_BUILD_APP=OFF`, `KRAVATEK_ENABLE_SANITIZERS=ON`, build Debug et CTest. Le script cloud n'installe pas de paquets système.

## Windows

La compilation MSVC x64 de la 0.3 doit être vérifiée après publication. Le chemin cible est `build/windows-x64/Release/KravaKickLab.exe`. Le workflow produit **KRAVA-KICK-LAB-Windows-x64** avec l'exécutable, deux CLI, le guide et les exemples ; la présence et le succès seront consignés ici après l'exécution effective. Ne pas utiliser un résultat 0.2 comme validation 0.3.

## À essayer ensemble

Sur ton PC Windows : sortie audio, dialogues natifs, confort de la fenêtre, écoute A/B et import WAV dans FL Studio. En contexte musical : grave Deep, articulation Hard Psy avec la basse, grain Schranz et intérêt des mutations. Les premiers solos/préécoutes peuvent brièvement bloquer l'interface, la bibliothèque est limitée à 512 états, les courbes restent paramétriques. IA/prompteur, VST3, import de samples et multibande sont absents.
