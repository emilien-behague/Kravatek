# Audit et périmètre de la version 0.2

La version 001 est conservée dans Git sous `archive/prototype-001` (commit `ba21a40`). Son moteur propre, sa lecture sans allocation dans le callback, son export PCM 24 bits et sa compilation MSVC étaient opérationnels. Ils ne suffisaient pas à en faire un instrument de sound design.

## Diagnostic

| Faiblesse de la 001 | Conséquence musicale | Travail de la 0.2 |
| --- | --- | --- |
| Une sinusoïde, une descente exponentielle presque fixe | Timbres trop proches ; grain et tail peu sculptables | Corps harmonique, FM/PM à décroissance réglable, tail indépendante et sub à fréquence fixe |
| Génération dans des plages étroites | Exploration peu intéressante | Huit bases, génération par graine, recherche bornée par NOVELTY et EVOLVE par groupes |
| Deux paramètres accessibles | Peu de prise sur l'attaque, la dynamique ou les textures | 24 paramètres, deux courbes éditables, sept groupes DNA |
| Habillage néon dominant, contrôles dispersés | Hiérarchie insuffisante et faible densité de fonctions utiles | Identité graphite/jaune, six modules numérotés et mesures au même endroit |
| Aucun preset complet ni comparaison | Impossible de retrouver une modification manuelle avec la graine seule | JSON versionné, historique de session, A/B avec atténuation RMS de préécoute |
| Affichage limité à l'onde | Pas de repère spectral ni dynamique | Spectre FFT, crête, RMS, facteur de crête et durée |

## Catégories de référence et limites de la comparaison

La mission cite KICK 3 et Kick Ninja. Les pages officielles tentées (`https://www.sonicacademy.com/products/kick-3` et `https://thehim.com/kick-ninja/`) sont refusées par le proxy du cloud (CONNECT 403). Aucun essai de ces logiciels ni inventaire vérifié de leurs versions courantes n'a donc été effectué ici. Les catégories suivantes cadrent le besoin d'un synthétiseur spécialisé ; elles ne constituent pas une comparaison commerciale validée.

| Catégorie | Exigence acoustique | État KRAVATEK 0.2 |
| --- | --- | --- |
| Oscillateurs, phase et accordage | Attaque reproductible, grave accordable | Phase initiale fixe, fondamentale cible réglable ; pas de déphasage libre |
| Enveloppes de pitch et d'amplitude | Placement du punch et place pour la bassline | Trois poignées par courbe ; modèle paramétrique, pas de dessin multipoint libre |
| Couches et transitoires | Séparer impact, corps et texture | Bruit d'attaque synthétique, body, tail modulée et sub propre ; pas d'import de samples |
| Distorsions et filtres | Grain contrôlé sans sacrifier le sub | Tanh, morphing sine-fold, passe-bas résonant ; pas de multibande ni compresseur |
| Exploration et bibliothèque | Reproduire, conserver et comparer | NOVELTY, EVOLVE, DNA, historique 24 états de session, presets JSON ; pas de navigateur de banque |
| Intégration musicale | Évaluer dans un morceau | Boucle au tempo et WAV ; pas de VST3 ni synchronisation DAW |

La différence visée est l'exploration déterministe avec conservation de groupes sonores. Son intérêt musical doit être jugé sur des sessions et des écoutes, pas déduit du nombre de paramètres.

## Lots

1. **Moteur et exploration** : couches, non-linéarités suréchantillonnées, filtre stable, paramètres bornés et tests. Compilation puis 21 tests réussis avant la refonte graphique.
2. **Instrument et mesure** : contrôles, courbes, historique, sérialisation, spectre calibré et comparaison de niveau. Un test supplémentaire vérifie les mesures ; le test GUI exerce les interactions réelles.
3. **Distribution** : compilation Windows, test GUI, paquet et huit exemples WAV.

À développer séparément : enveloppes multipoints, contrôle fin de phase, traitements multibandes si les écoutes en montrent le besoin, import de couches, banque persistante, plugin et prompteur. Aucun de ces éléments n'est annoncé opérationnel dans la 0.2.
