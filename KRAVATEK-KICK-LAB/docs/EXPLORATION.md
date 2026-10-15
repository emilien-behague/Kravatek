# Contrat d'exploration génétique 0.3

Les paramètres complets sont la vérité sauvegardée. Une graine seule ne reconstitue ni les éditions ni toute une chaîne EVOLVE. La reproductibilité binaire concerne une même version de moteur, plateforme, fréquence et mêmes conditions mathématiques.

| Opération | Entrées | Garantie |
| --- | --- | --- |
| GENERATE | Mode, graine de lot, NOVELTY, nombre | Lot déterministe ; chaque kick conserve sa graine effective et ses gènes |
| REGENERATE | Mode, NOVELTY, prochaine graine déterministe | Un nouveau candidat |
| EVOLVE | Parent complet, graine de lot, mode, NOVELTY, quatre verrous | Enfants distincts dans l'historique ; parent conservé |
| REFINE | Même opération, amplitude réduite | Architectures/graines internes conservées, variations subtiles autorisées |
| SAVE / OPEN | Session ou fichier de son versionné | Gènes rechargés exactement ; provenance et version contrôlées |

NOVELTY agit continûment sur l'amplitude de recherche et la probabilité d'architectures. CONTROLLED 0–30 %, EXPLORATION 31–70 %, EXPERIMENTAL 71–100 % sont des libellés, pas trois algorithmes discontinus, ni un score d'originalité.

Chaque mode a ses régions et probabilités : DEEP favorise sub/tails régulières, HARD PSY des durées courtes et corps tonals, SCHRANZ drive/grain et interactions, EXPERIMENTAL et MUTANT des structures plus mouvantes. Ces biais ne garantissent pas un résultat musical précis.

Les quatre verrous SUB, ATTACK, BODY et TAIL conservent tous les gènes de leur couche, y compris graine, phase et enveloppe. Les tests comparent les buffers bruts entiers à 44,1 et 48 kHz. Leur perception dans le mix peut évoluer avec les autres couches et la sécurité commune. Les verrous concernent EVOLVE : une édition manuelle volontaire reste possible. Tous verrouillés, l'audio reste identique et la bibliothèque peut contenir plusieurs provenances pour ce même audio.

En exploration, une similarité supérieure à 0,985 entraîne jusqu'à quatre propositions déterministes ; la plus différente est conservée et les proximités restantes sont signalées. REFINE et tous-verrouillés évitent cette recherche. La similarité combine descripteurs audio et corrélation alignée ; elle ne mesure pas l'originalité ni la qualité.

## Futur prompteur — non implémenté

Le futur adaptateur français/anglais devra produire un génome structuré soumis aux mêmes validations et visible avant rendu. Aucun parseur linguistique, IA ou service connecté n'est implémenté. Aucune commande active n'en simule le fonctionnement.
