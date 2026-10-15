# Validation 0.2 — 15 octobre 2026

La version 001 possède un [rapport historique séparé](VALIDATION-001.md). Ses résultats ne valident pas la 0.2.

## Résultats exécutés dans le cloud Linux

- Application et moteur compilés en Release, C++20/GCC 14.2, JUCE épinglé à `ea77444750de45e036e0279bb0291144bf8e3688`.
- **22 tests CTest réussis, aucun échec, aucun test ignoré.** `cli-invalid` est un rejet attendu : son code d'erreur est le comportement testé.
- **22 tests réussis en Debug avec AddressSanitizer et UndefinedBehaviorSanitizer.**
- Huit familles rendues à 8, 44,1, 48, 96 et 192 kHz ; variations, reproductibilité, plages invalides, filtre à résonance maximale et sweeps extrêmes.
- Effet sur le signal de chacun des 24 paramètres, dans un contexte où sa couche est active ; isolation du sub ; mutation nulle et verrouillage total sans modification du son.
- EVOLVE préserve les paramètres SUB/ATTACK verrouillés ; GENERATE est reproductible et NOVELTY ouvre les régions expérimentales.
- Régression d'alias sur une sinusoïde 1800 Hz, drive 30 dB, rendu 48 kHz : composante mesurée à 600 Hz **−69,57 dB par rapport à la fondamentale**. Cette mesure ciblée ne prouve pas l'absence d'aliasing pour tous les réglages.
- Régression sur une tail à ratio non entier : la modulation utilise un accumulateur de phase indépendant. Le nouveau test échouait avant correction (saut maximal 0,276) ; le maximum après correction est 0,0196251, sous la limite de 0,04 pour ce signal contrôlé.
- Analyse crête/RMS calibrée sur une sinusoïde de niveau connu ; spectre Hann 8192 points calibré en dBFS ; cas minuscule et NaN contrôlés.
- Lecture mono/stéréo, redéclenchement, concurrence, boucle au tempo, silence entre coups et STOP testés.
- Test réel de l'application sous Xvfb : drive, édition des deux courbes, DNA, EVOLVE, GENERATE, historique, régénération, sauvegarde JSON/rechargement audio exact, rejet d'un preset incomplet, A/B avec RMS de préécoute égalisé à la fréquence réelle du périphérique, export WAV et capture PNG. Accès aux commandes de bas de page vérifié par défilement dans un viewport de 1100 × 708 pixels.
- Callbacks JUCE exécutés sur une sortie ALSA **virtuelle silencieuse** ; aucune écoute physique déduite de ce résultat.
- Huit WAV lus et mesurés indépendamment par `wave`/Python ; export GUI Warehouse identique **octet pour octet** à l'export CLI correspondant.
- `tools/setup_cloud.py` réexécuté avec succès : dépendances locales, build incrémental et 22 tests. Le script ne fait pas d'installation système.
- Workflow Windows vérifié par actionlint 1.7.7 sans erreur.

## Exemples mesurés

Tous : mono, 48 kHz, PCM 24 bits, crête −3 dBFS à la précision de quantification. Première et dernière valeurs nulles ; moyenne DC absolue inférieure à 4 × 10⁻¹⁰.

| Base, graine 1 | Durée | RMS du fichier complet |
| --- | --- | --- |
| Reference | 320 ms | −16,54 dBFS |
| Warehouse / Hard Techno | 410 ms | −10,73 dBFS |
| Hard Psy | 190 ms | −11,08 dBFS |
| Schranz | 460 ms | −9,21 dBFS |
| Hardcore | 560 ms | −6,33 dBFS |
| Deep | 720 ms | −18,88 dBFS |
| Experimental | 850 ms | −14,88 dBFS |
| Mutant | 930 ms | −15,24 dBFS |

Ces niveaux décrivent des signaux, pas un classement de qualité. La normalisation en crête ne donne pas la même sonie. La comparaison A/B utilise une fenêtre RMS fixe de 400 ms, avec silence ajouté aux sons plus courts et atténuation du plus fort.

## Fichiers locaux et reproduction

- `build-linux/KravatekKickLab` : binaire **Linux**, pas un `.exe` Windows.
- `out/examples-002/*.wav` : huit exemples ; `out/audio-validation-002.json` : mesures et SHA-256.
- `out/cloud-smoke/gui-smoke.json`, `gui-smoke.wav`, `prototype-002.png` : dernière vérification graphique.
- `docs/images/instrument-002.png` : capture réelle conservée dans Git.
- `build-linux/Testing/` et `build-sanitized/Testing/` : résultats des suites exécutées.

```bash
python3 tools/setup_cloud.py
python3 tools/run_cloud_smoke.py
```

Les détecteurs mémoire se reproduisent avec `KRAVATEK_BUILD_APP=OFF`, `KRAVATEK_ENABLE_SANITIZERS=ON`, configuration Debug, puis build/CTest. Les sorties et dépendances sont dans le workspace et ignorées par Git ; les processus Xvfb temporaires sont arrêtés par le script.

## Windows

La compilation **[Windows instrument 0.2 #5](https://github.com/emilien-behague/Kravatek/actions/runs/38725215481)** a **réussi** sur le commit `53b1c187bd9cde118b8d48b68614699951e06ff9`, en **4 min 05 s**. Ce commit inclut la correction du modulateur de tail à ratio non entier. La configuration MSVC x64, la compilation Release, les 22 tests CTest, le test fonctionnel GUI et l'empaquetage ont terminé avec succès.

Dans **Artifacts**, le paquet **KRAVATEK-KICK-LAB-Windows-x64** fait **2,67 Mo**. Digest publié par GitHub : `sha256:d4abb78db82f93ccfdd9bb40ce0f4c0620cd6d9fce9a355955a7fca4fcbbc5b1`. Il contient **KravatekKickLab.exe**, **kravatek-render.exe**, **LIRE-MOI.md** et huit WAV dans **Examples/**. Le binaire est construit par le workflow à `build/windows-x64/Release/KravatekKickLab.exe`.

Le second paquet **Windows-test-results** fait **190 Ko**, digest GitHub `sha256:2393099c2ccd99981e57595122762db9b234fd652b71dc2bb7149e83d10a4bda`. Il conserve les résultats CTest et le rapport/capture de l'interface Windows.

Le statut, le commit, la durée, la présence et les métadonnées des artefacts ont été vérifiés sur le site public GitHub. **Les archives Windows n'ont pas été téléchargées dans le cloud et leurs digests n'ont pas été recalculés localement** : le téléchargement des artefacts demande une session GitHub. Le créateur peut ouvrir l'exécution ci-dessus, télécharger le paquet dans Artifacts, le décompresser et lancer l'exécutable. La première refonte (avant correction finale de phase) avait également réussi [l'exécution #4](https://github.com/emilien-behague/Kravatek/actions/runs/38724708360) ; utiliser le paquet de **#5**.

## À valider ensemble

L'écoute sur une sortie physique Windows, les dialogues natifs OPEN/SAVE/EXPORT et l'import FL Studio ne sont pas vérifiés dans le cloud. La pertinence musicale des familles, du punch, du sub et des mutations nécessite une écoute en contexte. La saisie au clavier, les petits écrans et les périphériques propres au PC du créateur méritent également un essai réel.

Le rendu reste effectué sur le thread d'interface, hors callback ; sa réactivité sur un PC moins puissant est une limite à surveiller. Le prompteur, les enveloppes multipoints libres, le multibande, l'import de samples et le VST3 ne sont pas implémentés.
