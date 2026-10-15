# Direction visuelle validée pour le développement

Le créateur a fourni une référence d'interface le 15 octobre 2026. Les versions suivantes doivent conserver son esthétique : fond bleu électrique quadrillé, cyan et rose néon, cadres de fenêtres rétro, logo pixel, pictogrammes géométriques et potentiomètres lumineux.

Le prototype utilise cette direction pour ses commandes réelles : PLAY, GENERATE, EXPORT WAV, AUDIO, PITCH END, DECAY et restauration d'une graine. L'onde calculée est rose ; la courbe de fréquence réellement utilisée par le moteur est cyan. Le panneau SYNTH DNA affiche les valeurs du rendu courant. Les boutons de fenêtre du cadre principal assurent les vraies actions réduire, agrandir et fermer.

| Élément | Couleur |
| --- | --- |
| Bleu de fond | `#090D71` |
| Intérieur des graphes | `#080B48` |
| Panneaux | `#10145D` |
| Cyan | `#09E9FF` |
| Rose | `#FF72EE` |
| Texte principal | `#F3E8FF` |

Le dessin est effectué directement par JUCE dans `src/app/NeonStyle.h`. Aucun fichier graphique externe n'est nécessaire au fonctionnement. Le moteur audio reste indépendant du thème.

Les modules supplémentaires visibles dans la référence (distorsion, macros, séquenceur, comparateur, bibliothèque de presets) appartiennent à la vision future. Ils ne sont pas présentés comme des fonctions disponibles dans la mission 001. Les graphes montrent les données calculées ; l'édition du pitch et de la durée se fait avec les deux potentiomètres du prototype.

Capture réelle, produite par le test de l'application :

![KRAVATEK KICK LAB — prototype néon](images/prototype-001-neon.png)
