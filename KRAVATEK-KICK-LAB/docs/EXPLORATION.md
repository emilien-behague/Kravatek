# Contrat d'exploration

Les paramètres DSP sont la vérité sauvegardée. Une graine seule ne reconstitue pas une édition manuelle ni une chaîne d'EVOLVE.

| Opération | Entrées | Garantie |
| --- | --- | --- |
| Factory | Famille | Base fixe, aucune promesse de genre musical validée par écoute |
| GENERATE / REGENERATE | Famille, graine, NOVELTY | Même résultat dans les mêmes conditions de rendu |
| EVOLVE | État source complet, nouvelle graine, amplitude, NOVELTY, verrous | Même transformation ; amplitude zéro conserve le son |
| SAVE / OPEN | État complet, schéma 002 | Reconstitution exacte testée sur une même plateforme |

NOVELTY élargit les régions de tail/mouvement, de repliement, de résonance et de décroissance FM. Ce contrôle biaise la recherche ; il n'évalue ni originalité ni qualité musicale.

Les sept verrous EVOLVE sont :

- PITCH : départ, fin, durée de chute et courbure.
- AMP : durée, attaque, extinction et courbure du body.
- BODY : drive, profondeur et durée FM/PM, harmoniques.
- ATTACK : niveau/durée du bruit et sa graine.
- TAIL : niveau, durée, ratio et mouvement.
- FX : cutoff, résonance, sweep et morphing de distorsion.
- SUB : fin de pitch, niveau/decay du sub et durée totale. Ce verrou peut donc empêcher certains changements dans les groupes PITCH/AMP déverrouillés.

Le gain de normalisation du mix et les interactions entre couches peuvent changer. Les verrous conservent les paramètres désignés ; ils ne promettent pas une séparation audio parfaite des couches dans le résultat normalisé. Tous verrouillés, l'audio reste identique.

## Futur PROMPT-TO-KICK — non implémenté

Le futur adaptateur français/anglais devra produire une proposition structurée : schéma, famille, paramètres explicites, verrous et graine. Il devra appeler la validation DSP existante, afficher les paramètres interprétés et permettre leur correction avant rendu. Aucun texte ne pourra exécuter du code ou contourner les plages du moteur. Les tests devront porter sur des descriptions françaises/anglaises équivalentes, les contraintes contradictoires et les valeurs hors plage.

Ce contrat et les fonctions typées `generate`/`evolve` préparent l'intégration. Il n'existe encore ni parseur linguistique ni modèle IA dans l'application ; aucun bouton de prompteur actif n'est présent.
