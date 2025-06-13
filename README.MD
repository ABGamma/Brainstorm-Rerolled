# Brainstorm Rerolled
This is a fork of both [Brainstorm](https://github.com/OceanRamen/Brainstorm) created by [OceanRamen](https://github.com/OceanRamen) as well as [Immolate](https://github.com/SpectralPack/Immolate) please give credit to them as I have simply modified the hard work they did

## Description
This repository houses both the code for the brainstorm mod as well as the code for the C++ version of the Immolate seed finder. If you would like to create your own filters simply 
add the code to the Immolate.cpp file, update the brainstorm endpoints to include whatever you need and then build it. This will generate a dll
that you can then place in the mods folder of balatro. The brainstorm mod will automatically detect the dll and use it, though you will need to update the brainstorm.lua and ui.lua 
in order to properly use your new options

## Requirements for Brainstorm
- [Lovely](https://github.com/ethangreen-dev/lovely-injector) injector -- Get it here: https://github.com/ethangreen-dev/lovely-injector/releases

## Installation

1. Install [Lovely](https://github.com/ethangreen-dev/lovely-injector) and follow the manual installation instructions.

### Windows

2. Download the [latest release](https://github.com/OceanRamen/Brainstorm/releases/) of Brainstorm.
3. Unzip the file, and place it in `.../%appdata%/balatro/mods` -- Make sure the Mod's directory name is 'Brainstorm' [^1]
4. Reload the game to activate the mod.

## Features
### Save-States
Brainstorm has the capability to save up to 5 save-states through the use of in-game key binds. 
> To create a save-state: Hold `z + 1-5`
> To load a save-state:	Hold `x + 1-5`

Each number from 0 - 5 corresponds to a save slot. To overwrite an old save, simply create a new save-state in it's slot. 

### Fast Rerolling
Brainstorm allows for super-fast rerolling through the use of an in-game key bind. 
> To fast-roll:	Press `Ctrl + r`

### Auto-Rerolling
Brainstorm can automatically reroll for parameters as specified by the user.
You can edit the Auto-Reroll parameters in the Brainstorm in-game settings page.
> To Auto-Reroll:	Press `Ctrl + a`

### Various Filters
In addition to the original filters from brainstorm I have added a few more filters that can help facilitate Crimson Bean or simply an easy early game  

ANTE 8 BEAN: This ensures you get a Turtle Bean within the first 150 items in the shop  
LATE BURGLAR: This ensures there is a burglar within the first 50-150 cards in the ante 6, 7, or 8 shop  
RETCON: This ensures that the retcon voucher appears before or in ante 8  
EARLY COPY + MONEY: This ensures that between the shop and the packs in ante 1 you get a brainstorm, blueprint, and some form of money generation
