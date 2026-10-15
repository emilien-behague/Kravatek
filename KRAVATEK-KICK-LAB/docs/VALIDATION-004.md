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

La compilation **[Windows universal lab 0.4 nº 9](https://github.com/emilien-behague/Kravatek/actions/runs/38745968415)** a **réussi** en **6 min 16 s**, sur le commit `2dc0b7d1e5e100935140fff9102a3038f8c589e0`. Elle inclut la correction du catalogue intégré pour MSVC et le dosage de rectification de la TAIL. Configuration MSVC x64, build Release, **46 tests CTest**, dix contrôles PCM24 GSE et seize UKE indépendants, test GUI et empaquetage ont terminé avec succès.

Dans Artifacts, **KRAVA-KICK-LAB-Windows-x64** fait **10,3 Mo** ; digest publié par GitHub : `sha256:0852819b38aa71def370bcd9f6f3a1a1e3c0181ac21c4acd6eb7d4e549250ad4`. Le paquet contient **KravaKickLab.exe** à sa racine, les deux CLI, **LIRE-MOI.md**, **Profiles/core-004.json**, **Examples/Universal/** (48 WAV/JSON, session de 24 sons, mesures/distances CSV), ainsi que les anciens exemples. Chemin dans le job Windows : `build/windows-x64/Release/KravaKickLab.exe`.

**Windows-test-results** fait **772 Ko** ; digest GitHub `sha256:783db21c1486fb776c53b3766191a86663066b456cfb1704f2083d6ff2ae2b40`. Il conserve le journal MSVC, les résultats CTest, rapports de génération/PCM24 et le rapport/capture GUI.

Statut, commit, durée, présence et métadonnées des artefacts vérifiés sur le site public GitHub. **Les archives Windows n'ont pas été téléchargées dans le cloud et leurs digests n'ont pas été recalculés localement** : le téléchargement demande une connexion GitHub. Le tag `v0.4.0` ajoute ce bilan documentaire sans changement du code testé.

La première tentative nº 7 avait échoué au build. Le catalogue intégré a été réemballé en tableau d'octets pour éviter la limite MSVC des grands littéraux ; la nº 8 a ensuite réussi. La **nº 9** est la livraison finale, incluant aussi le correctif de rectification. Pour essayer : ouvrir le run nº 9, télécharger KRAVA-KICK-LAB-Windows-x64, décompresser et lancer KravaKickLab.exe.

## À évaluer humainement

Écoute du poids, du punch, de la tonalité, des distorsions et de la place avec une basse ; confort de l'interface ; sortie audio et dialogues natifs sur le PC Windows ; import dans FL Studio. L'analyse automatique ne valide aucune de ces appréciations musicales.

La première livraison couvre huit profils. Les profils spécialisés extrêmes et les autres sous-genres, l'hybridation pondérée à deux genres et le MIDI entrant restent à développer. LOOP tronque une tail dépassant le temps, avec fondu ; ONE-SHOT conserve sa durée. Bibliothèque de 512 états ; courbes paramétriques ; certains rendus de préécoute/édition synchrones. L'IA connectée, le prompteur, VST3 et l'import de samples ne sont pas implémentés.
