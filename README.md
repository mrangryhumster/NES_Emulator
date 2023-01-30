# NES Emulator
'Simple' NES emulator written in C++ just for fun

Some games work just fine, but some require precise ppu-cpu sync (which my emulator lacks, maybe i'll fix it someday)

Currently implemented mappers : 
 **000**, **001**, **002**, **007**

**'Working' features**
 * Save states 
 * Rewind  
 * Memory view for PPU and CPU bus
 * CPU state viewer with simple disassembler
 * PPU contents viewer

 **Screenshots**
![SCREENSHOT_0](/images/full_scr.png)
![SCREENSHOT_1](/images/castelvania_scr.png)
![SCREENSHOT_2](/images/contra_scr.png)

**Controls:**
**Emulator**
 Action | Key
 -|-
 Save in slot 1 | Shift + F1 (up to 8 slots - F1,F2,F3, etc)
 Load from slot 1 | F1 (up to 8 slots - F1,F2,F3, etc)
 Rewind | R (Active while hold, arrow left to rewind, arrow right to advance if you missed your spot while rewinding)

**Player 1 (and only)**
 Action | Key
 -|-
 Start | W
 Select | Q
 A | S
 B | A
 Arrows | *keyboard arrows


**Build for linux (GCC/CLANG)**
```
cmake .
make
```
_And your binary will be in project root folder_

**Build for windows (MSVC)**
```
cmake -S . -B build
cmake --build build --config Release
```
_And you binary will be somethere in build folder (depends on configuration, for Release config it will be in build/Release)_

