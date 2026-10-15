# Validation — mission 001

Validation effectuée dans le cloud Debian 13, le **15 octobre 2026**. Aucun accès au PC Windows du créateur ni à FL Studio.

## Exécuté avec succès

- Compilation Release du moteur C++20 et de l'application autonome JUCE avec GCC 14.2.0.
- **14 tests CTest réussis, aucun échec et aucun test ignoré.** Le contrôle `cli-invalid` est un rejet attendu : une valeur invalide doit produire un code d'erreur, et c'est ce comportement qui passe.
- Les mêmes **14 tests réussis avec AddressSanitizer et UndefinedBehaviorSanitizer**, en compilation Debug.
- 512 variations contrôlées ; fréquences d'échantillonnage 8, 44,1, 48, 96 et 192 kHz ; buffers jusqu'à deux secondes.
- Oscillateur à phase continue, pitch exponentiel, fréquence effectivement rendue en fin de kick proche de 52 Hz, enveloppe décroissante, niveau de crête, DC et extrémités silencieuses.
- Reproductibilité à graine et paramètres identiques, effet des deux réglages manuels, lecture identique au buffer de rendu, redéclenchement lissé et publication concurrente de commandes de lecture.
- WAV mono PCM 24 bits : en-tête, valeurs PCM, métadonnées de graine et paramètres, bourrage RIFF des tailles impaires et erreurs de fichiers.
- Test réel de l'application graphique sous Xvfb : GENERATE, restauration de la graine 424242, PITCH END à 57 Hz, DECAY à 400 ms, PLAY répété, affichage de la forme d'onde et export d'un WAV de 19 200 échantillons.
- Périphérique ALSA **virtuel silencieux** actif et callbacks JUCE exécutés. Ce résultat ne prouve pas une écoute sur des haut-parleurs ou un casque.
- Export GUI et export CLI avec la même graine et les mêmes réglages **identiques octet pour octet**.
- Lecture du WAV par le module indépendant `wave` de Python.
- Script `tools/setup_cloud.py` exécuté puis réexécuté avec succès ; dépendances conservées, compilation incrémentale et tests réussis.
- Configuration GitHub Actions contrôlée par **actionlint 1.7.7** sans erreur. Archive officielle de l'outil vérifiée par SHA-256.

## Mesures du WAV de référence

Graine 1, paramètres de référence du cahier des charges.

| Mesure | Résultat |
| --- | --- |
| Fréquence / profondeur | 48 000 Hz / PCM 24 bits |
| Canaux / échantillons | 1 / 15 360 |
| Durée | 320 ms |
| Crête | −2,999999945 dBFS, écart dû à la quantification 24 bits |
| RMS | −16,54436 dBFS |
| Moyenne DC normalisée | −7,76 × 10⁻¹² |
| Premier et dernier échantillons | 0 / 0 |
| SHA-256 | `7f42c033648aafd8155727cbd8ed0bb403b712c9358505a8b1375d466357ef33` |

Le moteur flottant avant conversion atteint −3 dBFS et une moyenne DC d'environ −9,20 × 10⁻¹¹. Il n'y a ni sample externe, ni distorsion, ni limiteur.

## Fichiers produits dans cette instance

Chemins relatifs au dossier `KRAVATEK-KICK-LAB` :

- `build-linux/KravatekKickLab` : application Linux compilée ; ce fichier ne fonctionne pas sous Windows.
- `build-linux/kravatek-render` : exporteur Linux.
- `out/Kravatek-reference-001.wav` : premier kick à écouter/importer dans FL Studio.
- `out/audio-validation.json` : mesures indépendantes du WAV.
- `out/cloud-smoke/gui-smoke.wav` : kick exporté via l'interface testée.
- `out/cloud-smoke/gui-smoke.json` : résultat fonctionnel et compteurs de callbacks.
- `out/cloud-smoke/prototype-001.png` : capture réelle de l'interface.
- `build-sanitized/Testing/` et `build-linux/Testing/` : résultats CTest.

Les rendus, binaires et dossiers de compilation sont ignorés par Git. Les sources et instructions permettent de les régénérer. Aucun processus Xvfb lancé par les scripts n'est laissé actif.

## Windows : état exact et étape restante

**Aucune compilation de l'application Windows avec MSVC n'a été exécutée dans cette instance. Aucun `KravatekKickLab.exe` Windows validé n'est fourni à ce stade.**

La machine fournit Linux et GCC, sans Visual Studio/MSVC. Une tentative avec un compilateur Windows MinGW local a été arrêtée après le diagnostic explicite de JUCE : « MinGW is not supported. Please use an alternative compiler. » Le framework n'a pas été modifié pour contourner cette restriction.

La cible Windows x64 est préparée avec CMake, un script à double-cliquer et une compilation GitHub Actions utilisant Visual Studio 2022 / MSVC. Le workflow compile l'application, exécute les tests, vérifie l'interface et produit un paquet téléchargeable seulement après réussite. Le dialogue de sauvegarde natif, les sorties physiques Windows, l'écoute musicale et l'import FL Studio restent à contrôler sur Windows.

L'accès Git HTTPS au dépôt fonctionne. L'accès à `https://api.github.com` échoue actuellement avant toute authentification avec un refus réseau **403 lors du CONNECT**. Ce refus ne prouve pas un jeton manquant. L'ajout de `api.github.com` aux paramètres réseau est nécessaire pour suivre et piloter la compilation GitHub Actions depuis ce cloud. Les sources doivent également être publiées dans le dépôt pour déclencher le workflow ; une sauvegarde Git locale n'effectue pas cette publication.

## Reproduire les contrôles

Depuis `/workspace/Kravatek/KRAVATEK-KICK-LAB` :

```bash
python3 tools/setup_cloud.py
python3 tools/run_cloud_smoke.py
```

Pour les détecteurs mémoire, avec les outils cloud préparés :

```bash
/workspace/.kravatek-tools/python/bin/cmake -S . -B build-sanitized -G Ninja \
  -DCMAKE_MAKE_PROGRAM=/workspace/.kravatek-tools/python/bin/ninja \
  -DCMAKE_BUILD_TYPE=Debug -DKRAVATEK_BUILD_APP=OFF -DKRAVATEK_ENABLE_SANITIZERS=ON
/workspace/.kravatek-tools/python/bin/cmake --build build-sanitized --parallel 2
/workspace/.kravatek-tools/python/bin/ctest --test-dir build-sanitized --output-on-failure --parallel 2
```
