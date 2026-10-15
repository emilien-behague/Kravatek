# Architecture du prototype 001

## Synthèse indépendante de l'interface

`KickParameters` décrit le son. `renderKick()` renvoie un `RenderedKick` mono et ne dépend ni de JUCE ni d'un périphérique. Ce module peut être réutilisé dans un futur VST3 ; le prototype actuel calcule le kick hors du callback audio.

L'oscillateur additionne les incréments de phase et conserve sa phase quand la fréquence change. La fréquence suit `end + (start - end) × exp(-ln(100) × t / pitchDrop)`. À 60 ms, 99 % de la descente est effectuée. Une attaque en demi-cosinus, une décroissance exponentielle et une extinction finale en demi-cosinus encadrent le son.

Le rendu retire le décalage DC à l'aide d'une correction pondérée par la même enveloppe, ce qui conserve les extrémités silencieuses. Une normalisation hors ligne fixe la crête à −3 dBFS. Aucun limiteur, compresseur ou traitement de distorsion n'est utilisé. Les paramètres non finis ou hors des plages prises en charge sont rejetés.

Les graines utilisent un générateur entier déterministe fixé dans les sources. La graine 1 est réservée au kick de référence. Les variations touchent légèrement le pitch initial, le pitch final, le temps de descente, la durée et l'attaque. Les deux valeurs visibles sont arrondies à la résolution de leurs contrôles.

## Lecture et concurrence

Le thread de l'interface calcule un rendu à 48 kHz pour l'affichage/export et, si nécessaire, un rendu du même moteur à la fréquence réelle du périphérique. Aucun rééchantillonnage approximatif par saut d'indices n'est effectué.

`Playback` préalloue trois buffers de deux secondes à 192 kHz au démarrage. Une boîte aux lettres atomique transfère la propriété d'un buffer du producteur GUI au consommateur audio. Le producteur ne réécrit pas un buffer utilisé par le consommateur. Le callback copie seulement des échantillons et met à jour des atomiques : aucune allocation, libération, entrée/sortie fichier ou prise de verrou.

Lors d'un nouveau PLAY, la voix courante s'éteint en 5 ms avant le redémarrage du nouveau kick. Les clics intermédiaires remplacent la commande en attente. En cas de saturation temporaire des trois buffers, une nouvelle commande est refusée et l'interface le signale ; les buffers actifs restent intacts. Le son n'est pas empilé avec lui-même.

La classe s'utilise avec un seul producteur et un seul consommateur. `reset()` et `setSampleRate()` se font pendant l'arrêt du callback, via les événements de changement du périphérique JUCE. Le producteur peut encore publier une commande pendant `reset()`.

## Export

`writeWav24()` reçoit le rendu affiché et écrit un RIFF/WAVE PCM mono 24 bits à 48 kHz. Les échantillons sont arrondis sur 24 bits, sans dither aléatoire. Un chunk standard `LIST/INFO/ICMT` contient du JSON avec le numéro du moteur, la graine et tous les paramètres. Les tailles et bourrages RIFF, y compris les nombres d'échantillons impairs, sont testés.

Le code de lecture et l'export consomment les sorties de `renderKick()` ; aucun second algorithme de synthèse n'est utilisé. Les noms de fichiers Unicode sont transmis avec leur représentation native à l'exporteur. L'interface signale les erreurs d'écriture et laisse la confirmation d'écrasement au sélecteur de fichiers.

## Limites de la mission

Pas de samples externes, de distorsion, de séquenceur, de MIDI, de VST3 ou de système complet de presets. L'édition de la graine restaure sa variation de base ; les deux réglages manuels doivent aussi être conservés pour retrouver une variation modifiée. Le WAV contient ces informations, mais l'application ne recharge pas encore un preset depuis un WAV.

Le test graphique utilise les mêmes gestionnaires que les contrôles visibles et vérifie un véritable fichier exporté. Le dialogue natif de sauvegarde et le routage physique Windows nécessitent encore une validation sur une machine Windows.
