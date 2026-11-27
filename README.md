# Slapdash v0.4.1

Saturation multibande de **Planet Gloria** (AAX pour Pro Tools, plus AU, VST3 et une app autonome pour tester), écrite avec JUCE. Même famille visuelle que Malade : interface dessinée à la main.

## Ce qu'il fait

- **1 à 4 modules de disto identiques**, un par bande. Le bouton **+ band** propose **Add** (bande neuve, réglages par défaut) ou **Duplicate** (copie de la bande voisine, y compris sa plage de fréquences en mode Layer). Le **−** d'un module le retire.
- Par module :
  - **Type** :
    - **Transistor** : neutre. Écrêtage symétrique, aucun filtre.
    - **Tube** : un peu plus de médiums dans l'écrêtage, asymétrie qui grandit quand on attaque (harmoniques paires), haut adouci.
    - **Tape** : les aigus saturent en premier (accentuation / désaccentuation), hystérésis douce, petite bosse de tête vers 90 Hz.
    - **Fuzz** : écrêtage dur asymétrique, médiums légèrement creusés.
    - **Le Drive est calibré pareil sur les 4 types** : à réglage égal, à peu près autant de saturation, pour comparer les types sans retoucher le Drive.
    - La couleur des filtres grandit avec le Drive : peu de Drive = son ouvert (plat à ±1,5 dB jusqu'à 15 kHz à 30 %), la couleur vient quand on pousse. Le Transistor ne filtre jamais.
  - **Drive** (0 à +40 dB) et l'interrupteur **More**, qui prolonge la course : Drive à fond sans More = Drive à zéro avec More (jusqu'à +80 dB).
  - **Sag** : une alimentation qui se vide sur les attaques fortes (4 ms) et se recharge lentement (280 ms) : le point d'écrêtage baisse, l'attaque s'écrase, puis la note « respire ».
  - **Mix** : mélange propre / saturé de la bande.
  - **Pan** (stéréo seulement, grisé en mono) et **Gain** de bande (−24 à +12 dB, après le Mix).
  - **S / M / B** : Solo, Mute, Bypass (visibles à partir de 2 bandes).
  - Compensation de niveau intégrée, mesurée pour chaque type (`Source/dsp/CompTable.h`, refaite avec `tools/calibrate_comp.cpp`) : à −18 dBFS, monter Drive et More change la couleur, pas le volume.
- **En-tête** :
  - **on** : allume ou contourne tout le plugin (le son passe intact, sans clic).
  - **Presets** (sous les boutons) : nom du preset, flèches ‹ › pour passer au suivant, clic sur le nom pour la liste, **Save**, **Delete**. **Init** = tous les réglages par défaut. Un astérisque * s'affiche quand un réglage a bougé depuis le chargement. Un preset rappelle tout le plugin sauf le bouton on ; Link ne recopie pas les valeurs d'un preset sur les autres bandes. Les presets sont des fichiers dans `Documents/Planet Gloria/Slapdash/Presets` (sous-dossiers possibles : enregistrer « Drums/Snare » crée le dossier Drums) ; « Open presets folder » dans la liste ouvre ce dossier.
  - **Link** : les bandes bougent ensemble. Tourner Drive, Sag, Mix ou Gain (ou basculer More) sur une bande déplace le même réglage des autres bandes **du même écart**, chacune depuis sa propre valeur. Une bande qui arrive en butée s'y arrête, les autres continuent. Le Type et le Pan restent indépendants.
- **Bandeau du haut** :
  - **Split** (par défaut) : les bandes se partagent le spectre. On fait glisser les fréquences de coupure (↔), et on monte ou descend le trait de gain de chaque bande. La somme des bandes sans disto redonne exactement le son d'origine (filtres Linkwitz-Riley avec compensation de phase).
  - **Layer** : chaque bande a sa propre plage et elles peuvent se superposer (deux distos sur les mêmes fréquences, une à gauche, une à droite). On tire un bord ou le milieu d'une barre. Les bords **s'aimantent** sur ceux des autres bandes et sur 20 Hz / 20 kHz ; **Cmd + glisser** (Ctrl sur Windows) pour régler sans aimantation. Le gain se règle en glissant verticalement sur l'étiquette « Band n · 0 dB ↕ ».
- **Fader Slapdash** : la pile, de 9 V (neuve) à 0 V (morte). **Une seule pile pour tout le plugin, sur le master** : un étage transistor simple après la somme des modules, avant Low cut / High cut. Le Mix des modules ne la change donc pas ; seul le **Mix** du master la dose. En descendant : alimentation plus basse (ça écrête plus tôt), polarisation décalée, un peu plus de gain, moins de niveau ; à 9 V l'étage est hors circuit. Sous le fader, **gate** (par défaut) : les fins de notes crachotent puis se coupent ; **open** : la même couleur, sans gate.
- **Master** : Input, Low cut, High cut, **Mono maker** (tout ce qui est sous la fréquence passe en mono, Off à 0, jusqu'à 1000 Hz), Mix, Output, **Auto gain** (égalise lentement le volume de sortie sur l'entrée) et **Speaker sim** (un haut-parleur seul, sans ampli).
- **Alt + clic** sur n'importe quel réglage (potard, fader, bouton, trait de gain, fréquence de coupure, bord de bande) : retour à la valeur par défaut. Double-clic aussi sur les potards et faders.
- Suréchantillonnage ×4 de toute la partie disto, pour éviter le repliement (aliasing).

Réglages à l'ouverture : 1 module pleine bande, Split, on, Link OFF, Auto gain ON, Speaker sim OFF, Mono maker Off, Slapdash 9 V.

## Nouveau en v0.4.1

- **Drive calibré pareil sur tous les types.** Avant, Tape demandait environ 84 % pour saturer autant que Transistor ou Tube à 50 %, et Fuzz saturait déjà autant à 0 %. Tape reçoit maintenant le Drive à pleine échelle (−3 dB), Fuzz perd son gain fixe de +15,6 dB (−4 dB). Calage mesuré sur la compression d'un signal type batterie : les 4 types tombent à moins de 2,5 dB les uns des autres sur toute la course. Table de compensation refaite. Les sessions et presets en Tape ou Fuzz sonnent un peu différemment.

## Nouveau en v0.4

- **La pile passe au master** : avant, elle était dans le circuit de chaque module, avant son Mix (à Mix 0 % le fader ne faisait plus rien). Conséquence : elle ne passe plus par le circuit propre à chaque type (Tube et Fuzz ne réagissent plus différemment à la pile vide).
- **Presets** dans l'en-tête (la fenêtre gagne 36 px de haut pour leur ligne).

## Nouveau en v0.3

- Types dans l'ordre **Transistor** (nouveau, neutre), **Tube**, **Tape**, **Fuzz** ; le Screamer disparaît (il faisait redite avec le Speaker sim). Les sessions existantes changent de type.
- Filtres de couleur plus doux et qui grandissent avec le Drive : plus de son terne à faible Drive (sur un bus batterie par exemple).
- La Fuzz ne coupe plus les notes faibles d'elle-même.
- **More** devient un interrupteur qui prolonge la course du Drive.
- **Grit** supprimé.
- Fader Slapdash : mode **gate / open**.

## Nouveau en v0.2

- Les quatre types refaits pour qu'ils sonnent vraiment différemment ; Transistor devient **Fuzz**, Diode devient **Screamer** (les sessions v0.1 s'ouvrent, avec le nouveau son).
- Sag et Grit refaits, beaucoup plus audibles.
- Bouton **on** (bypass général), bouton **Link**, menu **Add / Duplicate**.
- Gain de bande jusqu'à **−24 dB**.

## Télécharger et installer

Chaque envoi sur GitHub compile le plugin (onglet **Actions**, environ 15 minutes) et fabrique deux installateurs :

- **Mac** : `Slapdash-<version>-mac.dmg`. Ouvre-le, double-clique sur « Install Slapdash.pkg » et choisis les formats (VST3, Audio Unit, app ; AAX en option). Universel Apple Silicon + Intel, macOS 11 et plus. L'installateur n'est pas signé par Apple : la première fois, fais clic droit > Ouvrir, ou Réglages Système > Confidentialité et sécurité > « Ouvrir quand même ». Plus besoin du Terminal ni de `sudo xattr` : les fichiers installés par l'installateur ne sont pas mis en quarantaine.
- **Windows** : `Slapdash-<version>-windows-setup.exe` (VST3 et app ; AAX en option), 64 bits. Non signé : si Windows affiche « Windows a protégé votre ordinateur », clique sur « Informations complémentaires » puis « Exécuter quand même ».

Pour donner le plugin à quelqu'un : une version marquée d'une étiquette (`v0.3.0`…) publie les deux installateurs sur la page **Releases** du dépôt, téléchargeables sans compte GitHub. Les fichiers de l'onglet Actions, eux, demandent d'être connecté et disparaissent après 30 jours.

Les scripts : `installer/mac/make_dmg.sh` et `installer/windows/installer.iss`.

## Compiler

Comme Malade. Sur Mac, avec Xcode et CMake :

```bash
cmake -B build -G Xcode
cmake --build build --config Release
```

Sur Windows (Visual Studio) : `cmake -B build -A x64` puis `cmake --build build --config Release`.


## Polices

Les polices **Patrick Hand** et **Caveat** (licence libre SIL OFL, dans `Assets/Fonts/`) sont intégrées au plugin : l'interface est la même sur Mac et sur Windows.

## Identité

Constructeur **Planet Gloria**, code constructeur `Pglo`, code plugin `Slpd`, identifiant `com.planetgloria.slapdash`.

## Organisation du code

```
Source/
  dsp/Saturation.h           la disto d'une bande (types, Drive, More, Sag) et la pile du master
  PresetManager.h            les presets (même fichier dans tous les plugins Planet Gloria)
  ui/PresetBar.h             la ligne des presets de l'en-tête
  dsp/CompTable.h            compensation de niveau mesurée par type
  LinkManager.h              le bouton Link
  Parameters.h               tous les paramètres automatisables
  PluginProcessor.*          séparation en bandes, suréchantillonnage, Master
  PluginEditor.*             la fenêtre (taille selon le nombre de bandes)
  ui/BandStrip.h             bandeau du haut (Split / Layer)
  ui/BandPanel.h             un module
  ui/Components.h            potards, faders, boutons, sélecteurs
  ui/Sketch.h                couleurs, polices, traits dessinés à la main
  ui/SketchLookAndFeel.*     dessin des potards, faders et interrupteurs
tools/
  calibrate_comp.cpp         remesure la compensation (après un changement de son)
  ui_snapshot.cpp            Linux : teste la pile et les presets, enregistre des images de la fenêtre (-DSLAPDASH_TESTS=ON)
```

## Limites connues

- Le Speaker sim est un seul modèle de haut-parleur (pas de choix de baffle).
- Link ne recopie que les gestes faits dans l'interface ; une automation dessinée à la main dans le logiciel audio reste propre à chaque bande.
