# Jumpjet Vehicle Carryall System - Setup Guide

This guide provides step-by-step instructions for importing and working with the Jumpjet Vehicle Carryall feature on a new device.

## Pull Request

**PR Link:** https://github.com/BlackgamerzVN/Phobos/pull/2

**Branch:** `blackgamerzvn-carryall-logic-expansion`

**Base Branch:** `develop`

---

## Prerequisites

### Required Software

1. **Visual Studio 2022** (Community Edition or higher)
   - Download: https://visualstudio.microsoft.com/downloads/

2. **Required VS Components** (listed in `.vsconfig`):
   - `Microsoft.VisualStudio.Component.VC.Tools.x86.x64`
   - `Microsoft.VisualStudio.Component.Windows10SDK.20348`
   - `Microsoft.VisualStudio.Component.VC.ATL`

3. **Git** (for cloning and submodule management)
   - Download: https://git-scm.com/downloads

4. **GitHub CLI** (optional, for PR management)
   - Download: https://cli.github.com/

---

## Step 1: Clone the Repository

### Option A: Clone Your Fork (Recommended)

```bash
# Clone your fork
git clone https://github.com/BlackgamerzVN/Phobos.git
cd Phobos

# Initialize YRpp submodule (REQUIRED)
git submodule update --init --recursive

# Checkout the feature branch
git checkout blackgamerzvn-carryall-logic-expansion
```

### Option B: Clone and Fetch PR Branch

```bash
# Clone the repository
git clone https://github.com/BlackgamerzVN/Phobos.git
cd Phobos

# Initialize YRpp submodule (REQUIRED)
git submodule update --init --recursive

# Fetch PR branch
git fetch origin pull/2/head:blackgamerzvn-carryall-logic-expansion
git checkout blackgamerzvn-carryall-logic-expansion
```

---

## Step 2: Verify File Changes

Ensure these files exist and contain the jumpjet carryall code:

```
Phobos/
├── src/
│   ├── Ext/
│   │   ├── UnitType/
│   │   │   ├── Body.h           (Modified - 12 new fields)
│   │   │   └── Body.cpp         (Modified - INI reading + serialization)
│   │   └── Techno/
│   │       └── Hooks.JumpjetCarryall.cpp  (NEW - 371 lines)
├── docs/
│   └── New-or-Enhanced-Logics.md  (Modified - new Vehicles section)
└── Phobos.vcxproj               (Modified - added Hooks.JumpjetCarryall.cpp)
```

**Quick verification:**
```bash
git log --oneline -3
# Should show:
# 30a76b97f Add jumpjet carryall documentation to main docs
# 40de01c73 Add Ares-compatible jumpjet vehicle carryall system
# 5ed0d8385 Global default value for `LeptonMindControlOffset`...
```

---

## Step 3: Build the Project

### Option A: Using Visual Studio 2022 GUI

1. Open `Phobos.sln` in Visual Studio 2022
2. Select build configuration:
   - **Debug** (recommended for development)
   - **DevBuild** (CI nightly build)
   - **Release** (optimized build)
3. Press `Ctrl+Shift+B` or `Build > Build Solution`
4. Check output in:
   - `Debug\Phobos.dll` + `.pdb`
   - `DevBuild\Phobos.dll` + `.pdb`
   - `Release\Phobos.dll` + `.pdb`

### Option B: Using Build Scripts (Command Line)

From repository root:

```batch
# Debug build (recommended for testing)
scripts\build_debug.bat

# DevBuild (CI configuration)
scripts\build_devbuild.bat

# Release build (optimized)
scripts\build_release.bat
```

### Option C: Using MSBuild Directly

```batch
# Ensure you're in VS Developer Command Prompt
# Or run this to locate and use vswhere:
scripts\run_msbuild.bat /m /p:Configuration=Debug Phobos.sln
```

**Clean build:**
```batch
scripts\clean.bat
```

---

## Step 4: Install and Test In-Game

### Installation

1. Build `Phobos.dll` (see Step 3)
2. Locate your Yuri's Revenge game directory
3. Copy these files to game directory:
   ```
   Debug\Phobos.dll         → [Game Directory]\
   Debug\Phobos.pdb         → [Game Directory]\ (optional, for debugging)
   ```
4. Ensure `Syringe.exe` is present (from Ares installation)
5. Launch via Syringe:
   ```batch
   Syringe.exe "gamemd.exe"
   ```

### Test Configuration

Create a test map with the following INI configuration:

```ini
; ===== Basic Cargo Helicopter =====
[CARGOCOPTER]
; Copy from existing jumpjet helicopter (e.g., HIND, ORCA)
Image=HIND
Primary=None              ; No weapons (defenseless)
Armor=light               ; Low armor
Speed=8                   ; Moderate speed
Locomotor={92612C46-F71F-11d1-AC98-006008055BB5}  ; Jumpjet
JumpjetAccel=10
JumpjetCruiseHeight=400
BalloonHover=yes          ; Smooth hovering

; === Jumpjet Carryall Configuration ===
JumpjetCarryall=yes
JumpjetCarryall.SizeLimit=10          ; Light/medium units only
JumpjetCarryall.AllowInfantry=yes
JumpjetCarryall.AllowVehicles=yes
JumpjetCarryall.PickupRange=384       ; 1.5 cells
JumpjetCarryall.SpeedMultiplier=0.75  ; 25% slower when loaded
JumpjetCarryall.VoicePickup=GenericVoice1
JumpjetCarryall.VoiceDropoff=GenericVoice2

Cost=1200
TechLevel=5
Prerequisite=RADAR,BARRACKS
```

### Testing Checklist

- [ ] **Pickup Test**
  1. Build CARGOCOPTER
  2. Select it, then right-click on an infantry/vehicle unit
  3. Helicopter should fly to target and pick it up (cargo disappears)
  4. Speed should decrease (if SpeedMultiplier < 1.0)

- [ ] **Dropoff Test**
  1. With cargo loaded, select Force Move (Alt+Click) to destination
  2. Helicopter should fly and drop cargo at destination
  3. Speed should return to normal

- [ ] **Size Filtering**
  1. Try picking up units larger than SizeLimit=10 (e.g., heavy tanks with Size=15+)
  2. Should refuse/fail to pick up

- [ ] **Death Handling**
  1. Pick up cargo
  2. Destroy the helicopter
  3. Cargo should be released and placed on ground

- [ ] **Save/Load**
  1. Pick up cargo
  2. Save game
  3. Load game
  4. Cargo should still be attached and functional

---

## Step 5: View Documentation

### In-Repo Documentation

**Main feature docs:**
```
docs/New-or-Enhanced-Logics.md
# Search for "Jumpjet Vehicle Carryall System" section
```

**Code documentation:**
```
src/Ext/UnitType/Body.h              # Field declarations and descriptions
src/Ext/Techno/Hooks.JumpjetCarryall.cpp  # Implementation with inline comments
```

### Example Configurations

Six detailed examples are available in the session files:

1. **Basic Cargo Helicopter** - General-purpose transport
2. **Medical Evacuation** - Infantry-only extraction
3. **Heavy Tank Transport** - Vehicle-only, large Size limit
4. **Spy Insertion** - Specific unit types, cloakable
5. **Rapid Deployment** - Fast with minimal speed penalty
6. **Soviet Heavy Transport** - Faction-specific with custom voices

**To access examples:** Check the PR description or session artifacts folder.

---

## Configuration Reference

### All Available Tags

| Tag | Type | Default | Description |
|-----|------|---------|-------------|
| `JumpjetCarryall` | boolean | `no` | Enable jumpjet carryall functionality |
| `JumpjetCarryall.SizeLimit` | integer | `-1` | Max cargo Size (-1 = unlimited, Ares-compatible) |
| `JumpjetCarryall.Types` | list(TechnoType) | - | Type whitelist (empty = all allowed) |
| `JumpjetCarryall.AllowInfantry` | boolean | `yes` | Allow picking up infantry |
| `JumpjetCarryall.AllowVehicles` | boolean | `yes` | Allow picking up vehicles |
| `JumpjetCarryall.Capacity` | integer | `1` | Max cargo count (>1 not implemented yet) |
| `JumpjetCarryall.PickupRange` | integer | `256` | Pickup distance in leptons (~1 cell = 256) |
| `JumpjetCarryall.SpeedMultiplier` | float | `1.0` | Speed multiplier when carrying cargo (1.0 = no penalty) |
| `JumpjetCarryall.VoicePickup` | VoxClass | - | Voice line played on pickup |
| `JumpjetCarryall.VoiceDropoff` | VoxClass | - | Voice line played on dropoff |
| `JumpjetCarryall.DrawCargo` | boolean | `no` | Render cargo visually (not implemented yet) |
| `JumpjetCarryall.CargoOffset` | XYZ | `0,0,-128` | Cargo render offset (future feature) |

### Filter Priority (Applied in Order)

1. ✅ **Type Check** - Must be `InfantryClass` or `UnitClass`
2. ✅ **Category Filter** - `AllowInfantry` / `AllowVehicles`
3. ✅ **Size Filter** - Cargo's `Size ≤ SizeLimit` (Ares-compatible)
4. ✅ **Type Whitelist** - If `Types=` specified, cargo must be in list
5. ✅ **State Restrictions**:
   - Not over water (no pickup on water terrain)
   - Not already being carried
   - Not mind-controlled by different owner
   - Not at extreme height differences
   - Carrier must have free capacity

### Balancing Guidelines

**Speed Penalties:**
- Light cargo (infantry, recon): `0.85-0.95` (5-15% slower)
- Medium cargo (APCs, IFVs): `0.7-0.85` (15-30% slower)
- Heavy cargo (tanks): `0.5-0.7` (30-50% slower)

**Pickup Ranges:**
- Default/precise: `256` leptons (~1 cell)
- Generous: `384-512` leptons (~1.5-2 cells)
- Tight: `128-192` leptons (~0.5-0.75 cells)

**Cost Recommendations:**
- Infantry-only transports: 600-1000
- Light vehicle transports: 1000-1500
- Heavy vehicle transports: 1500-2500

---

## Known Limitations & TODOs

### Not Yet Implemented

- **Multi-Cargo Support** - `Capacity > 1` needs linked list traversal
  - Current implementation: Single cargo only
  - Code location: `Hooks.JumpjetCarryall.cpp` (marked with TODO)

- **Visual Cargo Rendering** - `DrawCargo=yes` flag exists but not functional
  - Requires coordinate transformation in `UnitClass_Draw_It` hook
  - Code location: Line ~250 in `Hooks.JumpjetCarryall.cpp` (TODO)

- **Falling Damage** - Dropped cargo lands safely (no damage on drop)
  - Could be future enhancement if needed

### By Design Limitations

- **No Air-to-Air Pickup** - Cargo must be on ground (jumpjet-to-jumpjet not supported)
- **No Water Pickup** - Cannot pick up units over water terrain
- **No Mind-Control Transfer** - Cannot pick up units with different mind-control owner

---

## Troubleshooting

### Build Errors

**Error: `CL.exe not found` / `LINK not found`**
- Solution: Install Visual Studio 2022 with required components (see Prerequisites)
- Or: Run build scripts from "Developer Command Prompt for VS 2022"

**Error: `YRpp/...h not found`**
- Solution: Initialize submodule: `git submodule update --init --recursive`

**Error: `Hooks.JumpjetCarryall.cpp not found`**
- Solution: Verify you're on the correct branch: `git checkout blackgamerzvn-carryall-logic-expansion`

### In-Game Issues

**Pickup doesn't work:**
1. Check `JumpjetCarryall=yes` is set on the carrier
2. Verify cargo passes filters (Size, Type, Infantry/Vehicle)
3. Ensure carrier is close enough (within `PickupRange`)
4. Check carrier has `BalloonHover=yes` and JumpjetLocomotor

**Cargo disappears but not carried:**
- This is a bug - cargo should be attached to carrier
- Check `UnitClass::AttachTrigger` pointer in debugger
- Verify serialization is working (save/load test)

**Speed penalty not applied:**
- Verify `JumpjetCarryall.SpeedMultiplier` is set to less than `1.0`
- Check hook at `0x4CE5B8` (GetCurrentSpeed) is active

**Crash on pickup/dropoff:**
- Enable debug build (`scripts\build_debug.bat`)
- Run with debugger attached
- Check for null pointer dereferences in hooks

---

## File Locations Reference

### Source Code
```
src/Ext/UnitType/Body.h                    # Field declarations (lines 86-98, 159-170)
src/Ext/UnitType/Body.cpp                  # INI reading (139-151), serialization (216-227)
src/Ext/Techno/Hooks.JumpjetCarryall.cpp   # Core logic (371 lines total)
```

### Project Files
```
Phobos.vcxproj          # Build configuration (line 189: ClCompile entry)
Phobos.sln              # Visual Studio solution file
.vsconfig               # Required VS components list
```

### Documentation
```
docs/New-or-Enhanced-Logics.md    # Main feature documentation (Vehicles section)
docs/Whats-New.md                 # Changelog (TODO - not updated yet)
CREDITS.md                        # Credits (TODO - not updated yet)
```

### Submodules
```
YRpp/                   # Game binary type definitions (must be initialized)
```

---

## Development Workflow

### Making Changes

1. **Code Changes:**
   ```bash
   # Make edits to files
   # Build and test
   scripts\build_debug.bat
   
   # Commit changes
   git add .
   git commit -m "Description of changes"
   git push origin blackgamerzvn-carryall-logic-expansion
   ```

2. **Adding New Features:**
   - New config tags: Edit `UnitType/Body.h`, `Body.cpp` (LoadFromINIFile, Serialize)
   - New hooks: Add to `Hooks.JumpjetCarryall.cpp` using `DEFINE_HOOK` macro
   - Update documentation in `docs/New-or-Enhanced-Logics.md`

3. **Testing:**
   - Always test with Debug build first
   - Use `.pdb` files for debugging crashes
   - Test save/load compatibility
   - Test multiplayer sync if applicable

### Updating YRpp Submodule

If you need newer game type definitions:

```bash
cd YRpp
git fetch origin
git checkout <newer-commit>
cd ..
git add YRpp
git commit -m "Update YRpp submodule to <commit>"
```

---

## Ares Compatibility

This implementation is **fully compatible** with Ares aircraft carryalls:

- **Separate Code Paths**: Jumpjet carryalls use different hooks than aircraft carryalls
- **Size Property Reuse**: Both use vanilla `Size=` on TechnoTypes (no conflicts)
- **SizeLimit Convention**: `JumpjetCarryall.SizeLimit=-1` matches Ares' unlimited convention
- **Filter Logic**: Identical filtering behavior to Ares for consistency

**You can have both systems active simultaneously:**
- Aircraft carryalls (Ares): `[AircraftType]►Carryall=yes, Carryall.SizeLimit=X`
- Jumpjet carryalls (Phobos): `[UnitType]►JumpjetCarryall=yes, JumpjetCarryall.SizeLimit=X`

---

## Additional Resources

### Phobos Documentation
- Main docs: https://phobos.readthedocs.io/
- GitHub repo: https://github.com/Phobos-developers/Phobos

### Ares Documentation
- Carryall reference: https://ares-developers.github.io/Ares-docs/new/carryalls.html

### YRpp Documentation
- GitHub repo: https://github.com/Phobos-developers/YRpp
- Type definitions for gamemd.exe binary

### Build System
- MSBuild reference: https://docs.microsoft.com/en-us/visualstudio/msbuild/
- Syringe (DLL injector): https://github.com/Ares-Developers/Syringe

---

## Support & Contact

**PR Link:** https://github.com/BlackgamerzVN/Phobos/pull/2

**Branch:** `blackgamerzvn-carryall-logic-expansion`

**Author:** BlackgamerzVN

**Phobos Team:** https://github.com/Phobos-developers

---

## Quick Start Summary

```bash
# 1. Clone repo
git clone https://github.com/BlackgamerzVN/Phobos.git
cd Phobos

# 2. Initialize submodule (REQUIRED!)
git submodule update --init --recursive

# 3. Checkout branch
git checkout blackgamerzvn-carryall-logic-expansion

# 4. Build (requires VS 2022)
scripts\build_debug.bat

# 5. Copy to game directory
copy Debug\Phobos.dll "C:\Path\To\YR\Phobos.dll"

# 6. Launch via Syringe
cd "C:\Path\To\YR"
Syringe.exe "gamemd.exe"
```

**Test INI:**
```ini
[MyCarryall]
; ... copy base helicopter stats ...
JumpjetCarryall=yes
JumpjetCarryall.SizeLimit=10
JumpjetCarryall.SpeedMultiplier=0.75
```

---

**Last Updated:** 2026-07-22  
**Phobos Version:** develop branch + jumpjet carryall feature  
**Status:** ✅ Code complete, untested in-game
