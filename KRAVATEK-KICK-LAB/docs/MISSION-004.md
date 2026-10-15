# Mission 004 — Universal Kick Engine, premier ensemble

## Audit et périmètre

Base vérifiée : `9aee786` / `v0.3.0`, sauvegardée sous `archive/pre-mission-004`. Build incrémental réussi, 35 tests CTest réussis, GUI 20 kicks → 8 mutations avec SUB/ATTACK exacts, bibliothèque et export réussis. Les cinq WAV témoins et la session sont conservés dans `out/baseline-004/`.

Le code existant fournit quatre couches indépendantes, pitch en deux segments, FM/PM/ring, transitoires synthétiques, filtres résonants, waveshaping tanh/sine-fold suréchantillonné, graines SplitMix32, NOVELTY, EVOLVE, verrous exacts, bibliothèque, sauvegarde, mesures et A/B. Ses limites : cinq modes internes, pas de profils externes, pas de membrane, pas de 808 au-delà de 1,5 seconde, pas de choix de distorsion ou de chaîne multibande, pas de navigateur de genres.

Le premier ensemble vise huit profils : House, Techno, Psytrance, Hard Techno, Hardstyle, Hardcore, 808 et Experimental. Les nombreuses déclinaisons spécialisées, l'hybridation entre deux profils et le MIDI entrant restent des lots suivants. Aucun menu ne prétendra les proposer avant leur implémentation et validation.

## Lot A — composants et compatibilité

Le moteur UKE `KRAVA-UKE-004.1` coexiste avec GSE 003.1 et le moteur 002. Chaque couche conserve sa version de rendu. Une couche 003 verrouillée garde ainsi son ancien traitement même au sein d'un kick 004. Les identités K3 et les fichiers anciens restent reconnus ; les nouveaux fichiers utilisent K4 et un schéma de session 004. Les gènes audio complets sont sauvegardés.

Six points de départ d'architecture configurent réellement les modules : Analog, Psy Punch, Distorted Hard, Tonal/Pitched, Hybrid/Acoustic et Experimental. Les ajouts comprennent : modes de membrane inharmoniques à amortissement séparé avec bruit filtré, oscillateur saw PolyBLEP, tails gated/reverse, harmoniques de tail, glide du sub et de la tail, durées jusqu'à 4 secondes et accordage par numéro de note MIDI 24–60 (outil d'accordage, pas MIDI entrant).

Une chaîne de distorsion propre à BODY et TAIL propose atan, hard clip, tanh, polynomial, foldback, asymétrie, rectification et numérique (quantification + sample hold). Pré/postfiltrage, seconde saturation, enveloppe de drive et saturation de deux bandes complémentaires. Le SUB ne traverse pas ces chaînes. Les non-linéarités restent calculées à 4x avec FIR de décimation ; le numérique peut volontairement produire des textures riches en alias, et l'oversampling ne garantit pas l'absence d'alias sur tous les réglages.

Validation exécutée du premier noyau : 39 tests réussis, dont architectures distinctes et déterministes aux deux fréquences cibles, huit distorsions distinctes, paramètres extrêmes, accordage 55 Hz mesuré, rendu long, DC/bords/crêtes, couches de versions mixtes et sauvegarde/relecture de six génomes 004 au sein d'une bibliothèque 003. Les cinq WAV témoins 003 restent identiques octet pour octet après rechargement avec le nouveau code. Aucun résultat d'écoute humaine n'est déduit de ces tests.

## Lot B — huit profils externes et campagne

`profiles/core-004.json` contient huit profils opérationnels : Classic House, Classic Techno, Full-On Psytrance, Driven Hard Techno, Tonal Hardstyle, Driven Hardcore, Long Tuned 808 et Hybrid Experimental. Ce ne sont pas encore toutes les déclinaisons de leurs familles. Chaque profil possède identité/révision, famille, recommandations BPM, région fondamentale, glide/intervalle, probabilités d'architectures et modules, traitements et distributions nommées. Un schéma commun valide les paramètres ; aucune grande cascade de conditions par genre. NOVELTY agit sur l'étendue des distributions et la probabilité de sélectionner les modules secondaires.

Les requêtes séparent profil, architecture forcée éventuelle, caractère, note éventuelle, graine et NOVELTY. Les douze choix de caractère (dont PROFILE sans transformation) modifient des ensembles explicites de gènes ; ils peuvent volontairement dépasser l'esthétique du profil. La définition complète du profil est sauvegardée dans les réglages de session et porte un SHA-256. Une modification ultérieure du catalogue ne change pas les génomes sauvegardés ni la recette de cette session.

La campagne a réellement exporté 48 WAV/JSON (8 profils × 3 graines × 2 fréquences). Les 24 audios à 48 kHz sont distincts ; les mesures et les distances entre profils sont dans `build-linux/test-output/universal-reference/`. Vérifications ciblées réussies : catalogue/rejets, campagne, verrous, recettes persistantes, caractères/architectures forcées, références acoustiques et ancienne population de 100 candidats.

Une faiblesse des mesures 003 a été corrigée pour UKE : l'analyse Hann initiale sous-pondérait les transitoires au début du fichier. L'analyse UKE utilise des fenêtres racine de Hann périodiques centrées, recouvrement 50 % et padding aux deux extrémités, pour une somme des fenêtres au carré constante. Un signal de référence grave + burst aigu vérifie la fraction d'énergie haute. L'ancien calcul reste utilisé pour les décisions de diversité GSE, afin de ne pas changer ses éventuelles relances à graines identiques. Les métriques ne constituent pas une validation musicale.


## Intégration du premier ensemble

Le navigateur GENRE / SUBGENRE / CHARACTER / NEXT ARCHITECTURE / NEXT NOTE règle les prochaines générations ; les paramètres du son sélectionné restent éditables séparément. Les huit profils sont réellement disponibles. GENETIC 0.3 conserve les anciens modes ; CLASSIC 0.2 reste accessible. Le panneau SYNTH/FX ajoute trois pages de contrôles personnalisés pour les chaînes BODY/TAIL et les glides, phases et membrane. Les paramètres sans effet dans le contexte sont désactivés. L'éditeur de session indique désormais moteur et identité dans le cartouche.

Le lecteur et l'export acceptent les buffers de quatre secondes jusqu'à 192 kHz ; l'interface exporte à 44,1/48 kHz. Les durées affichées ne sont plus bornées à 1,5 seconde. Le moniteur signale TAIL > BEAT. LOOP reste monophonique et tronque la tail au prochain temps avec extinction : ONE-SHOT est nécessaire pour écouter une longue 808 entièrement. Les recommandations BPM des profils ne constituent pas des restrictions de genre ; la plage générale de lecture est 40–400 BPM.

L'estimation F0 UKE utilise un préfiltrage à 600 Hz et une décimation vers 4 kHz pour couvrir les notes proposées, au lieu de limiter toute estimation aux basses fréquences 003. Les références aux notes MIDI 24, 33 et 60 sont testées aux deux fréquences cibles ; il ne s'agit pas d'un récepteur MIDI. L'ancien estimateur GSE reste inchangé. La périodicité des textures complexes reste ambiguë.

Le smoke test a exécuté les anciens parcours, puis 32 candidats via les huit profils du navigateur, l'accordage 808 à 55 Hz, le contrôle d'une durée dépassant 1,5 seconde, les contrôles de distorsion/phase du nouveau panneau, 8 mutations UKE avec SUB/ATTACK exacts, A/B ajusté, SAVE/OPEN avec recette exacte et HISTORY sur 72 états. Le retour d'historique conserve le son sélectionné dans la zone visible. Le test utilise une sortie silencieuse ; aucune écoute physique n'est revendiquée.

La campagne de référence produit aussi `reference.session.json` : 24 sons issus des huit profils, ouvrables ensemble pour A/B. Les 48 WAV/JSON et deux CSV sont regroupés dans le paquet Windows sous `Examples/Universal/`.

## Fichiers modifiés

- `src/genetic/UniversalDsp`, `Genome`, `LayerRenderer` : architectures, gènes, versions par couche, distorsions et accordage.
- `src/genetic/Profile`, `ProfileIO`, `BundledProfiles.h.in`, `profiles/core-004.json` : schéma, distributions, catalogue externe et snapshot intégré.
- `src/genetic/Generator`, `Population`, `SessionIO`, `Features` : évolution, recettes, compatibilité, persistance et mesures.
- `src/app/GeneticLab`, `UniversalControls`, `Main`, `MainComponent`, `InstrumentStyle`, `Windows.rc` : navigateur, panneau, monitoring, ergonomie et version 0.4.
- `src/dsp/Playback.h`, `WavWriter.cpp`, `src/cli/GeneticMain.cpp` : buffers longs, export et outil universel.
- `tests/UniversalTests`, `ProfileTests`, `StateTests`, `GeneticTests`, `CMakeLists.txt` : vérifications, campagne et compilation.
- `tools/run_cloud_smoke.py`, `validate_genetic_exports.py`, `.github/workflows/windows.yml`, README et documentation : exécution reproductible et livraison.

## Limites et lots suivants

Le lot A et le premier ensemble du lot B sont livrés avec leur intégration de navigateur. L'ensemble de la mission n'est pas terminé. Les profils spécialisés Schranz, Rawstyle, Gabber, Frenchcore et Uptempo doivent encore recevoir leurs algorithmes/régions propres (lot C). Le mode Schranz 003 reste opérationnel. Les autres sous-genres, DnB/Dubstep, familles globales et profils acoustiques spécifiques restent à étendre avec validation (lot D).

Le mélange pondéré de deux profils n'est pas développé (lot E). Changer le genre cible avant EVOLVE permet une transformation progressive, mais ne constitue pas un hybride 70/30 de deux genres. Le profil Hybrid Experimental combine des modules, pas deux profils musicaux pondérés. Le MIDI entrant reste à développer ; NEXT NOTE est un outil d'accordage numérique. Pas d'API IA, prompteur, VST3 ni import de samples.

La bibliothèque reste limitée à 512 états. Certaines premières préécoutes, solos et éditions restent synchrones. Les courbes restent paramétriques à trois poignées. Les textures numériques peuvent volontairement générer des artefacts spectraux ; le test d'alias ciblé ne qualifie pas tous les réglages. La qualité musicale, le confort sur le PC du créateur, les dialogues natifs Windows et l'import FL Studio restent à évaluer humainement.
