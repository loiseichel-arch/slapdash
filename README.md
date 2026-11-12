# Slapdash v0.1

Saturation multibande de **Planet Gloria** (AAX pour Pro Tools, plus AU, VST3 et une app autonome pour tester), écrite avec JUCE. Même famille visuelle que Malade : interface dessinée à la main.

## Ce qu'il fait

- **1 à 4 modules de disto identiques**, un par bande. Le bouton **+ band** ajoute un module, le **−** d'un module le retire.
- Par module :
  - **Type** : Tube (chaud, asymétrique), Tape (très doux, compresse), Transistor (plus dur), Diode (écrêtage net style pédale overdrive).
  - **Drive** (0 à +40 dB) et **More** (2e étage en cascade, 0 à +30 dB).
  - **Sag** : la marge baisse quand on joue fort, l'attaque s'écrase, la note « respire ».
  - **Grit** : couche râpeuse en parallèle, harmoniques aiguës granuleuses.
  - **Mix** : mélange propre / saturé de la bande.
  - **Pan** (stéréo seulement, grisé en mono) et **Gain** de bande (±12 dB, après le Mix).
  - **S / M / B** : Solo, Mute, Bypass (visibles à partir de 2 bandes).
  - Compensation de niveau intégrée : à −12 dBFS, monter Drive et More change la couleur, pas le volume.
- **Bandeau du haut** :
  - **Split** (par défaut) : les bandes se partagent le spectre. On fait glisser les fréquences de coupure (↔), et on monte ou descend le trait de gain de chaque bande. La somme des bandes sans disto redonne exactement le son d'origine (filtres Linkwitz-Riley avec compensation de phase).
  - **Layer** : chaque bande a sa propre plage et elles peuvent se superposer (deux distos sur les mêmes fréquences, une à gauche, une à droite). On tire un bord ou le milieu d'une barre. Les bords **s'aimantent** sur ceux des autres bandes et sur 20 Hz / 20 kHz ; **Cmd + glisser** (Ctrl sur Windows) pour régler sans aimantation. Le gain se règle en glissant verticalement sur l'étiquette « Band n · 0 dB ↕ ».
- **Fader Slapdash** : la pile de la pédale, de 9 V (neuve) à 0 V (morte). En descendant : polarisation décalée, plus de crasse, moins de niveau, et des fins de notes qui crachotent puis se coupent.
- **Master** : Input, Low cut, High cut, **Mono maker** (tout ce qui est sous la fréquence passe en mono, Off à 0, jusqu'à 1000 Hz), Mix, Output, **Auto gain** (égalise lentement le volume de sortie sur l'entrée) et **Speaker sim** (un haut-parleur seul, sans ampli).
- **Alt + clic** sur n'importe quel réglage (potard, fader, bouton, trait de gain, fréquence de coupure, bord de bande) : retour à la valeur par défaut. Double-clic aussi sur les potards et faders.
- Suréchantillonnage ×4 de toute la partie disto, pour éviter le repliement (aliasing).

Réglages à l'ouverture : 1 module pleine bande, Split, Auto gain ON, Speaker sim OFF, Mono maker Off, Slapdash 9 V.

## Compiler

Comme Malade. Sur Mac, avec Xcode et CMake :

```bash
cmake -B build -G Xcode
cmake --build build --config Release
```

Ou sans rien installer : chaque envoi sur GitHub lance la compilation dans l'onglet **Actions**. Quand elle est verte, télécharge **Slapdash-macOS** en bas de la page, puis suis `INSTALL_MAC.txt`.

## Polices (optionnel)

Comme Malade : dépose `PatrickHand-Regular.ttf` et `Caveat-Bold.ttf` dans `Assets/Fonts/`. Sans elles, l'interface utilise Chalkboard SE et Marker Felt du Mac.

## Identité

Constructeur **Planet Gloria**, code constructeur `Pglo`, code plugin `Slpd`, identifiant `com.planetgloria.slapdash`.

## Organisation du code

```
Source/
  dsp/Saturation.h           la disto d'une bande (types, Drive, More, Sag, Grit, pile)
  Parameters.h               tous les paramètres automatisables
  PluginProcessor.*          séparation en bandes, suréchantillonnage, Master
  PluginEditor.*             la fenêtre (taille selon le nombre de bandes)
  ui/BandStrip.h             bandeau du haut (Split / Layer)
  ui/BandPanel.h             un module
  ui/Components.h            potards, faders, boutons, sélecteurs
  ui/Sketch.h                couleurs, polices, traits dessinés à la main
  ui/SketchLookAndFeel.*     dessin des potards, faders et interrupteurs
```

## Limites connues de la v0.1

- Pas de système de presets intégré : utilise celui du logiciel audio.
- Le Speaker sim est un seul modèle de haut-parleur (pas de choix de baffle).
