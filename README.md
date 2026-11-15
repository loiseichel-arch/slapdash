# Slapdash v0.2

Saturation multibande de **Planet Gloria** (AAX pour Pro Tools, plus AU, VST3 et une app autonome pour tester), écrite avec JUCE. Même famille visuelle que Malade : interface dessinée à la main.

## Ce qu'il fait

- **1 à 4 modules de disto identiques**, un par bande. Le bouton **+ band** propose **Add** (bande neuve, réglages par défaut) ou **Duplicate** (copie de la bande voisine, y compris sa plage de fréquences en mode Layer). Le **−** d'un module le retire.
- Par module :
  - **Type**, chacun un petit circuit complet (filtrage avant et après l'écrêtage) :
    - **Tube** : médiums poussés, asymétrie qui grandit quand on attaque (harmoniques paires), haut doux. Chaud, gras.
    - **Tape** : les aigus saturent en premier (accentuation / désaccentuation), hystérésis douce, bosse de tête vers 90 Hz. Rond, compressé.
    - **Fuzz** : gain énorme, écrêtage dur asymétrique, médiums creusés, se ferme sur les notes faibles.
    - **Screamer** : seuls les médiums vont dans les diodes, le grave reste propre (façon Tube Screamer). Serré, médium. Sur un signal très grave, il sonne plus bas en volume (le grave ne passe pas dans l'écrêtage) ; l'Auto gain compense.
  - **Drive** (0 à +40 dB) et **More** (2e étage en cascade, 0 à +30 dB).
  - **Sag** : une alimentation qui se vide sur les attaques fortes (4 ms) et se recharge lentement (280 ms) : le point d'écrêtage baisse, l'attaque s'écrase, puis la note « respire ».
  - **Grit** : jusqu'à 50 %, une râpe aiguë franche ; au-delà, une fuzz à l'octave du dessus s'ajoute (redressement, façon Octavia).
  - **Mix** : mélange propre / saturé de la bande.
  - **Pan** (stéréo seulement, grisé en mono) et **Gain** de bande (−24 à +12 dB, après le Mix).
  - **S / M / B** : Solo, Mute, Bypass (visibles à partir de 2 bandes).
  - Compensation de niveau intégrée, mesurée pour chaque type (`Source/dsp/CompTable.h`, refaite avec `tools/calibrate_comp.cpp`) : à −18 dBFS, monter Drive et More change la couleur, pas le volume.
- **En-tête** :
  - **on** : allume ou contourne tout le plugin (le son passe intact, sans clic).
  - **Link** : les bandes bougent ensemble. Tourner Drive, More, Sag, Grit, Mix ou Gain sur une bande déplace le même réglage des autres bandes **du même écart**, chacune depuis sa propre valeur. Une bande qui arrive en butée s'y arrête, les autres continuent. Le Type et le Pan restent indépendants.
- **Bandeau du haut** :
  - **Split** (par défaut) : les bandes se partagent le spectre. On fait glisser les fréquences de coupure (↔), et on monte ou descend le trait de gain de chaque bande. La somme des bandes sans disto redonne exactement le son d'origine (filtres Linkwitz-Riley avec compensation de phase).
  - **Layer** : chaque bande a sa propre plage et elles peuvent se superposer (deux distos sur les mêmes fréquences, une à gauche, une à droite). On tire un bord ou le milieu d'une barre. Les bords **s'aimantent** sur ceux des autres bandes et sur 20 Hz / 20 kHz ; **Cmd + glisser** (Ctrl sur Windows) pour régler sans aimantation. Le gain se règle en glissant verticalement sur l'étiquette « Band n · 0 dB ↕ ».
- **Fader Slapdash** : la pile de la pédale, de 9 V (neuve) à 0 V (morte). En descendant : polarisation décalée, plus de crasse, moins de niveau, et des fins de notes qui crachotent puis se coupent.
- **Master** : Input, Low cut, High cut, **Mono maker** (tout ce qui est sous la fréquence passe en mono, Off à 0, jusqu'à 1000 Hz), Mix, Output, **Auto gain** (égalise lentement le volume de sortie sur l'entrée) et **Speaker sim** (un haut-parleur seul, sans ampli).
- **Alt + clic** sur n'importe quel réglage (potard, fader, bouton, trait de gain, fréquence de coupure, bord de bande) : retour à la valeur par défaut. Double-clic aussi sur les potards et faders.
- Suréchantillonnage ×4 de toute la partie disto, pour éviter le repliement (aliasing).

Réglages à l'ouverture : 1 module pleine bande, Split, on, Link OFF, Auto gain ON, Speaker sim OFF, Mono maker Off, Slapdash 9 V.

## Nouveau en v0.2

- Les quatre types refaits pour qu'ils sonnent vraiment différemment ; Transistor devient **Fuzz**, Diode devient **Screamer** (les sessions v0.1 s'ouvrent, avec le nouveau son).
- Sag et Grit refaits, beaucoup plus audibles.
- Bouton **on** (bypass général), bouton **Link**, menu **Add / Duplicate**.
- Gain de bande jusqu'à **−24 dB**.

## Compiler

Comme Malade. Sur Mac, avec Xcode et CMake :

```bash
cmake -B build -G Xcode
cmake --build build --config Release
```

Sur Windows (Visual Studio 2022) : `cmake -B build -G "Visual Studio 17 2022" -A x64` puis `cmake --build build --config Release`.

Ou sans rien installer : chaque envoi sur GitHub lance la compilation dans l'onglet **Actions**. Quand elle est verte, télécharge en bas de la page **Slapdash-macOS-…** (Mac, universel, macOS 11 et plus) ou **Slapdash-Windows** (Windows 64 bits : VST3, AAX, app), puis suis `INSTALL_MAC.txt` ou `INSTALL_WINDOWS.txt`.

## Polices

Les polices **Patrick Hand** et **Caveat** (licence libre SIL OFL, dans `Assets/Fonts/`) sont intégrées au plugin : l'interface est la même sur Mac et sur Windows.

## Identité

Constructeur **Planet Gloria**, code constructeur `Pglo`, code plugin `Slpd`, identifiant `com.planetgloria.slapdash`.

## Organisation du code

```
Source/
  dsp/Saturation.h           la disto d'une bande (types, Drive, More, Sag, Grit, pile)
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
```

## Limites connues

- Pas de système de presets intégré : utilise celui du logiciel audio.
- Le Speaker sim est un seul modèle de haut-parleur (pas de choix de baffle).
- Link ne recopie que les gestes faits dans l'interface ; une automation dessinée à la main dans le logiciel audio reste propre à chaque bande.
