# LifePlay v1.0 — UE 5.8 Realism, GTA Control & Interaction AI Roadmap

**Project:** *LifePlay v1.0* — Unreal Engine 5.8

**Target Aesthetic:** Grounded, realistic open-world modern city & lifelike characters (GTA-inspired visual fidelity)

**Core Gameplay Upgrade:** GTA Third-Person Character Control + Native Unreal Engine Interaction AI & Standalone Roleplay

**Primary Workflow:** Real-Time Split-Screen Development (**Antigravity IDE** + **UE 5.8 Live Coding**)

---

## 🧠 Project Context Block

> [!IMPORTANT] Hardware Profile Note (GTX 1650 4 GB VRAM)
> This development machine runs an **NVIDIA GeForce GTX 1650 (4 GB VRAM, no hardware ray-tracing cores)**. `Config/DefaultEngine.ini` uses a **GTX 1650-safe DX11 low-load profile** (`r.ScreenPercentage=80`, half-res SSS, 1K CSM shadows, fast SSR, `t.MaxFPS=60`, `r.VSync=1`; Lumen/VSM/Nanite/Hardware RT disabled to prevent out-of-VRAM crashes and blown-out legacy UE4 interiors).

* **Base Codebase:** *LifePlay v1.0* on Unreal Engine 5.8, featuring procedural **OpenStreetMap (OSM)** city generation, scene scripting, relationship/mood RPG stats, and **Daz / MakeHuman** character rigs driven by morph targets and animations.
* **Engine Target:** Unreal Engine 5.8 (C++ & Blueprints).
* **Runtime GTA Control & UE Interaction AI Architecture:**
  * `ULifePlayGTAControllerSubsystem` (`UTickableWorldSubsystem`): Pressing `G` toggles seamless GTA-style over-the-shoulder camera (`AGtaCameraRigActor`) and camera-relative `W/A/S/D` + `Shift` sprint + `Ctrl` walk movement on `Person_C` (`isPlayer == true`), renders the `LifePlay v1.0` GTA HUD card, strips legacy donate/social buttons/donor walls from `MainMenu`, plays looping `street` ambience, and enables `E`/`T` Roleplay, `F` Call NPC Over, and `M` Mute.
  * `ULifePlayAIRoleplaySubsystem` (`UGameInstanceSubsystem`): Drives native UE `AAIController` (`SetFocus`, `MoveToActor`, `LineOfSightTo`), `UNavigationSystemV1` fleeing & street roaming, 3D `UGameplayStatics` soundplay, and 10 standalone GTA roleplay interactions persisted to `Saved/LifePlayRoleplay/`.

---

## Phase 0: Real-Time Split-Screen Dev Setup (Antigravity ↔ UE 5.8)

*Goal: Connect Antigravity directly to the running Unreal Editor 5.8 viewport so you can watch edits happen live (~30–60 mins).*

* [x] **0.1 Enable Unreal MCP in UE 5.8**
  * Enabled `ModelContextProtocol`, `AllToolsets`, and `PythonScriptPlugin` in `LifePlay.uproject`.
  * Configured `Config/DefaultEditorPerProjectUserSettings.ini` to Auto-Start MCP on `http://127.0.0.1:8000/mcp`.
* [ ] **0.2 Connect Antigravity IDE to Unreal MCP**
  * Open `LifePlay.uproject` in Unreal Editor 5.8.
  * In Antigravity's Agent panel, open **`...` > MCP Servers > Manage MCP Servers > View raw config** (`mcp_config.json`) and add:
    ```json
    {
      "mcpServers": {
        "unreal-mcp": {
          "serverUrl": "http://127.0.0.1:8000/mcp"
        }
      }
    }
    ```
  * Test live connection by prompting Antigravity to spawn or adjust a light in the open level and verifying immediate viewport updates.
* [x] **0.3 Configure UE 5.8 Live Coding & Antigravity IntelliSense**
  * Configured `ULiveCodingSettings` (`bEnabled=True`, `Startup=AutomaticButHidden`, `bEnableReinstancing=True`) in `Config/DefaultEditorPerProjectUserSettings.ini`.
  * Generated `.vscode` / C++ IntelliSense project files for Antigravity IDE.
  * Toggle **Realtime (`Ctrl + R`)** ON in the UE 5.8 3D Viewport while working live.

---

## Phase 1: Core Engine & Rendering Pipeline (Config & C++)

*Goal: Strip legacy UE 4.2x rendering defaults and stabilize UE 5.8 C++ modules (~2–4 hours).*

* [x] **1.1 C++ & Build System Migration**
  * `LifePlay.Target.cs` & `LifePlayEditor.Target.cs` use `DefaultBuildSettings = BuildSettingsVersion.V7` and `IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8`.
  * Fixed all 18 UE 5.8 `LogClass` uninitialized property startup errors in `Plugins/StreetMap`.
  * Added `Slate`, `SlateCore`, `HTTP`, `Json`, `JsonUtilities`, `AIModule`, and `NavigationSystem` dependencies in `LifePlay.Build.cs`.
* [x] **1.2 `Config/DefaultEngine.ini` Rendering Overhaul (GTX 1650 DX11 Realism Profile)**
  * Applied full-res SSS skin shading, GTAO, lightweight half-res SSGI, 2K CSM + Contact Shadows, SSR Quality 3, Bloom, Filmic ACES Tonemapper, TAA Sharpening, and 16x Anisotropy in `Config/DefaultEngine.ini` and `Config/DefaultScalability.ini`.
  * *(Optional 8 GB+ RTX Upgrade Profile only)*: Enable Lumen (`r.DynamicGlobalIlluminationMethod=1`, `r.ReflectionMethod=1`), Virtual Shadow Maps (`r.Shadow.Virtual.Enable=1`), and Nanite (`r.Nanite.ProjectEnabled=True`).
* [ ] **1.3 Live Lighting & Atmosphere Setup (via Unreal MCP)**
  * Inspect and tune **Sky Atmosphere**, **Directional Light**, **Sky Light**, **Exponential Height Fog**, and **Post Process Volume** live via Unreal MCP.

---

## Phase 2: OpenStreetMap (OSM) City & Environment Realism

*Goal: Transform LifePlay's flat procedural OSM block cities into dense, detailed, GTA 6-style urban environments (~1–2 weeks).*

* [ ] **2.1 Asset Acquisition (Fab / Quixel Megascans)**
  * Download modular modern urban building kits, weathered asphalt/concrete PBR materials, and street props into `Content/UE5_Upgrades/Environment/`.
* [ ] **2.2 Live Batch Mesh Optimization / Nanite Conversion**
  * Run a UE 5.8 Editor Python script via MCP to optimize/tag imported static architectural and street prop meshes.
* [ ] **2.3 Procedural Building Facades & Interior Mapping**
  * Drive a **PCG** graph from LifePlay's OSM building footprints/splines to add facades, window trims, and rooftop props + Parallax Interior Mapping window materials.
* [ ] **2.4 Realistic Roads, Puddles & Street Dressing**
  * Build a Master Road Material Instance with live roughness/wetness parameters and scatter sidewalks along OSM road edges.

---

## Phase 3: Lifelike Characters, Skin, Hair & Clothing

*Goal: Eliminate the "plastic UE4 mannequin" look while preserving 100% compatibility with LifePlay's morph targets and mod ecosystem (~3–5 days).*

* [ ] **3.1 Subsurface Profile Skin Shader (Live Look-Dev)**
  * Upgrade master skin materials to **Subsurface Profile** with exposed pore normal and roughness Material Instance parameters.
* [ ] **3.2 Realistic Eyes & Wet Meniscus**
  * Convert character eye materials to UE5's **Eye Shading Model**.
* [ ] **3.3 Strand-Based Groom Hair**
  * Bind UE5 Groom strand hair assets to the head bone/socket while preserving head morph compatibility.
* [ ] **3.4 Fabric Shading & Micro-Normals**
  * Switch outfit materials to the **Cloth** shading model with weave micro-normals.

---

## Phase 4: GTA Third-Person Control & Standalone NPC Role-Play

*Goal: Direct GTA-style character movement & camera control (`G`) plus 100% standalone offline NPC role-play (`E`/`T`) with persistent memory, LifePlay relationship updates, and physical actions (optional external AI/TTS only if added later).*

* [x] **4.0 GTA Third-Person Character Controller (`ULifePlayGTAControllerSubsystem`)**
  * Over-the-shoulder collision-tested `AGtaCameraRigActor` toggled with `G`.
  * Camera-relative `W/A/S/D` movement, `LeftShift` sprint, `LeftControl` slow walk, mouse look, mouse wheel zoom, and `LeftAlt` temporary cursor unlock.
* [x] **4.1 Standalone Offline Role-Play Dialogue Subsystem (`ULifePlayAIRoleplaySubsystem` + `SLifePlayRoleplayChatWidget`)**
  * Zero external AI/API/internet required: instant offline role-play panel (`E` or `T`) with **Greet**, **Ask About Life**, **Flirt**, **Follow Me**, **Stay Here**, **Provoke**, and free-text role-play input.
* [x] **4.2 LifePlay Context & Persistent Memory Injection**
  * Reads live NPC attributes (`Name_First`, `Name_Last`, `age`, `job`, `CurrentMood`, `rapportwithplayer`, `attractiontoplayer`) via reflection and persists rolling per-NPC JSON history offline in `Saved/LifePlayRoleplay/`.
* [x] **4.3 Structured Action & Relationship Triggers**
  * Updates live `rapportwithplayer` / `attractiontoplayer` stats and executes physical NPC actions (`follow`, `stop_follow`, `face_player`).
* [ ] **4.4 Voice Synthesis / Lip-Sync (Optional Future Upgrade)**
  * Optional offline TTS/lip-sync driving LifePlay facial morph targets during dialogue.

---

## Phase 5: Grounded Locomotion, Vehicles & Crowd Simulation

*Goal: Make movement, driving, and city crowds feel heavy, responsive, and alive (~1–2 weeks).*

* [ ] **5.1 Procedural Control Rig & Foot IK**
* [ ] **5.2 Motion Matching (Optional Advanced Locomotion)**
* [ ] **5.3 Chaos Vehicles Upgrade**
* [ ] **5.4 MassEntity Crowds & Traffic**

---

## Phase 6: Live Antigravity Prompt Templates (Copy & Paste)

Use these prompts inside **Antigravity IDE** while Unreal Editor 5.8 is open and connected via MCP:

1. **Live Lighting & Atmosphere Calibration:**
   > *"Using the Unreal MCP connection, inspect the currently open level (`CentralLondon`). Tune the Directional Light, Sky Light, Exponential Height Fog, and Post Process Volume live via MCP while keeping GTX 1650 DX11 performance smooth."*
2. **Add TTS Voice & Facial Lip-Sync (Phase 4.4):**
   > *"Extend `ULifePlayAIRoleplaySubsystem` to send the NPC's `reply` text to a local Piper/Kokoro TTS endpoint, play the audio at the NPC's location, and drive LifePlay's facial mouth/emotion morph targets on `Person_C`."*
3. **Live Material Instance Look-Dev (Phase 3.1):**
   > *"Inspect the character skin Material Instance currently applied to the test character in the viewport. Adjust the roughness multiplier and subsurface scattering intensity live via MCP until skin highlights look soft and natural."*
