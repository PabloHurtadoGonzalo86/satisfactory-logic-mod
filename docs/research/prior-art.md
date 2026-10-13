# Prior art and name collisions

_Generated from `data/prior-art.json` by `tools/render_research.py`; do not edit by hand. Research run of 2026-10-13. Claims are the researcher's; verdicts are from independent skeptics told to refute them. Wording is kept verbatim (English)._

## Summary

Scope: all 1,894 SMR entries (1,608 visible + 286 hidden) pulled from the public GraphQL API, 28 relevant ones catalogued, plus source reading of FicsIt-Networks (FIN) and Circuitry (FicsitWiremod) with commit-pinned URLs.

Main finding: the premise "Satisfactory has no Redstone" is only partly true. Circuitry (195k downloads, v14.1.63 of 2026-09-26, built for game >=502094, Windows and LinuxServer) already ships logic gates, SR latch, flip-flops, timers, edge detectors, bool/int/hex conversion, multiplexer, radio, sensors and machine control. FIN (209k) offers Lua computers and push-event signals over network cables, but its newest SMR release 1.2.0 is flagged Broken by the UE 5.6.1 update; the development branch 1.3.0 targets >=502094 yet has no release. No SMR mod mentions "redstone".

Gap, phrased as "not found in Circuitry/FIN": Circuitry gates are separate actors ticked per frame that pull values by reflection, with no tick groups or prerequisites in its C++ (Blueprints not inspected). A deterministic, frame-rate-independent discrete-tick model with propagation delay is the most defensible differentiator, but this is an inference to confirm in-game.

Licensing: Circuitry and FRM have no LICENSE (all rights reserved by default); FIN is GPL-3.0. Plan a clean-room design, copy nothing, and do not link FIN.

Interop without linking: FIN's netFunc_/netProp_/netSig_ and FINConnector convention, Circuitry's CircuitryAPI JSON tables, and FRM setEnabled/setSwitches for vanilla power switches (dashboard-friendly).

Editor: both reference mods are C++ logic plus roughly 1,100 to 1,400 .uasset files; a pure-C++ solver core is testable on this VPS, but placeable buildables need the Unreal editor.

Names: "Circuitry" is taken (hidden placeholder reference and Wiremod's brand). The other 11 candidates are free as SMR references; avoid FicsitRedstone (Mojang naming rules), and Relay/Gatekeeper collide heavily on GitHub. References are permanent, and an SMR AI-use disclosure cannot be cleared once set.

## Claims at a glance

| Id | Confidence | Decision-driving | Skeptic verdicts |
| --- | --- | --- | --- |
| circuitry-exists | verified-primary | yes | confirmed |
| license-reuse | verified-primary | yes | partially-correct |
| fin-status | verified-primary | yes | confirmed |
| fin-interop-no-dependency | verified-primary | yes | confirmed |
| circuitry-tick-model | verified-primary | yes | confirmed |
| circuitry-order-inference | secondary | no | not verified (not decision-driving) |
| smr-naming-process | verified-primary | yes | confirmed |
| editor-split | verified-primary | yes | confirmed |
| fin-architecture | verified-primary | no | not verified (not decision-driving) |
| frm-interop | verified-primary | no | not verified (not decision-driving) |
| circuitry-interop | verified-primary | no | not verified (not decision-driving) |
| no-redstone-on-smr | verified-primary | no | not verified (not decision-driving) |
| fin-ecosystem | verified-primary | no | not verified (not decision-driving) |
| smr-ai-disclosure | verified-primary | no | not verified (not decision-driving) |
| redstone-name-and-semantics | verified-primary | no | not verified (not decision-driving) |

## Claims in detail

### circuitry-exists (verified-primary, decision-driving)

Circuitry (mod_reference FicsitWiremod, 195,478 downloads) already is a Redstone-like in-game logic mod: gates (AND/OR/NOT/XOR/NAND/NOR/XNOR), SR latch, D and T flip-flops, timer, edge detectors, bool-array<->int and hex conversion, multiplexer/transistor, sensors, and control of vanilla machines. Its newest SMR release 14.1.63 (2026-09-26) targets game >=502094 and SML ^3.12.0 (satisfied by the owner's build 502094), is flagged Works on EA and EXP, ships Windows/LinuxServer/WindowsServer targets and is required_on_remote. So binary logic, latches, clocks and sensors are NOT a gap. The repo head (uplugin 14.1.62) trails the SMR release by one patch.

Evidence:

- <https://ficsit.app/mod/FicsitWiremod>

  ```text
  Allows you to create circuits to control your factory in any way you want
  ```
- <https://ficsit.app/mod/FicsitWiremod>

  ```text
  "required_on_remote":true
  ```
- <https://ficsit.app/mod/FicsitWiremod>

  ```text
  "state":"Works"
  ```
- <https://ficsit.app/mod/FicsitWiremod>

  ```text
  **Logic gates** (AND, OR, NOT, XOR etc.)
  ```
- <https://ficsit.app/mod/FicsitWiremod>

  ```text
  **Interaction with manufacturers** (Put machine into standby mode, set the recipe or overclock)
  ```
- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/FicsitWiremod.uplugin>

  ```text
  "GameVersion": ">=502094"
  ```
- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/Source/FicsitWiremod/Public/Behaviour/Gates/Logic/SRLatch.h>

  ```text
  class FICSITWIREMOD_API ASRLatch : public AFGWiremodBuildable
  ```

**Skeptic (source-accuracy): confirmed**

Note: Facts verified via the SMR GraphQL API (getModByReference FicsitWiremod: compatibility{EA,EXP}, versions{version,game_version,required_on_remote,targets,dependencies}). Result: 14.1.63 created 2026-09-26T07:25:47Z, game_version >=502094, SML ^3.12.0, targets LinuxServer/Windows/WindowsServer, required_on_remote true, EA and EXP both Works, short_description is the quoted sentence. Citation fix: ficsit.app/mod/FicsitWiremod is a client-rendered shell (1,218 bytes) that does not contain the JSON-style quotes, so cite the API instead. Downloads now 195,480 (live counter). Repo: single branch master = 4e9fff3, uplugin 14.1.62 (so one patch behind), no LICENSE. Headers present for AND/OR/NOT/XOR/NAND/NOR/XNOR, SRLatch, DFlipFlop, TFlipFlop, Timer, Rising/FallingEdgeDetector, IntToBoolArray, BoolArrayToInt, Multiplexer, Transistor, SignalSelector (one-hot decoder). Hex support is only NumberToHexString and ColorToHex/HexToColor.

Evidence: <https://api.ficsit.app/v2/query>

### license-reuse (verified-primary, decision-driving)

Reuse constraints: Circuitry's repository has no LICENSE/COPYING/NOTICE file (README grants nothing), so default copyright applies and its code/assets cannot be reproduced or adapted. FicsIt-Networks (FIN) is GPL-3.0; linking a module against GPL code makes the combination GPL, and copying FIN code requires GPL for the whole work. FRM has no LICENSE either; its README only grants 'create your own web UI or serial apps'. No permissively licensed (MIT/Apache) in-game logic mod was found. This is research, not legal advice: the safe route is a clean-room reimplementation from public docs, not copying or porting their code or assets.

Evidence:

- <https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository>

  ```text
  without a license, the default copyright laws apply, meaning that you retain all rights to your source code and no one may reproduce, distribute, or create derivative works from your work.
  ```
- <https://github.com/Scordium/FicsitWiremod/tree/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162>

  ```text
  This repository contains the source code for Circuitry (FicsitWiremod) mod
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/LICENSE>

  ```text
  c) You must license the entire work, as a whole, under this
  ```
- <https://www.gnu.org/licenses/gpl-faq.html#IfLibraryIsGPL>

  ```text
  Yes, because the program actually links to the library. As such, the terms of the GPL apply to the entire combination.
  ```
- <https://github.com/porisius/FicsitRemoteMonitoring/blob/32fe64e0c22389a944c27222ef6c881f5e207072/README.md>

  ```text
  Feel free to create your own web UI or serial apps.
  ```

**Skeptic (source-accuracy): partially-correct**

Corrected statement: Circuitry's repo (2,380-entry tree at 4e9fff3) has no LICENSE/COPYING/NOTICE and the GitHub license field is null, so default copyright applies. FIN is stock GPL-3.0 (674-line LICENSE, no exception, section 5(c) quoted correctly); the FSF FAQ states that linking makes the whole combination GPL. FRM has no license file; its README section 'Permissions Granted' says only 'Feel free to create your own web UI or serial apps.' Omitted point: the SDK/SML repo at the owner's pinned commit d2162c99 also has a stock GPL-3.0 root LICENSE.txt and no exception file, so the FSF linking argument is not specific to FIN. Whether it binds mods is a legal question these sources do not settle (not legal advice). Clean-room remains the safe route for Circuitry and for FIN code; the owner should choose the mod's own license deliberately.

Note: All quotes re-checked: GitHub docs sentence, GPL FAQ IfLibraryIsGPL, FIN LICENSE 5(c), FRM README. FRM repo tree at 32fe64e has no license file. The absence claim ('no permissive in-game logic mod') is not refuted by my gh and SMR searches (logic/redstone/gate/circuit/wire/signal surfaced only Circuitry and FIN) but cannot be proven. GitHub metadata shows MIT-licensed Satisfactory mods exist (kcking/justfly, Basement-Science/SatisfactoryMod_Speedo); I did not check whether they link SML.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/LICENSE.txt>

### fin-status (verified-primary, decision-driving)

FIN's newest SMR release 1.2.0 (2025-09-09, game >=415558, SML ^3.11.1) is flagged Broken on both EA and EXP by the Unreal Engine 5.6.1 update, i.e. broken on the owner's build. The development branch is version 1.3.0 with GameVersion >=502094 and SML ^3.12.0 and was still receiving commits on 2026-10-08, but no release exists; port PR #534 was closed without being merged. FIN cannot be assumed installable today (its 'Broken' flag is the evidence; the caret SML range alone would allow 3.12).

Evidence:

- <https://ficsit.app/mod/FicsItNetworks>

  ```text
  "note":"Broken by the update to Unreal Engine 5.6.1"
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/789c32fd/FicsItNetworks.uplugin>

  ```text
  "GameVersion": ">=415558"
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/FicsItNetworks.uplugin>

  ```text
  "GameVersion": ">=502094",
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/pull/534>

  ```text
  Port to Satisfactory 1.2 / Unreal Engine 5.6.1 (port only)
  ```

**Skeptic (source-accuracy): confirmed**

Note: SMR API (getModByReference FicsItNetworks): latest version 1.2.0, created 2025-09-09, game_version >=415558, SML ^3.11.1; EA and EXP both state Broken with note 'Broken by the update to Unreal Engine 5.6.1'. As with Circuitry, the ficsit.app page is a JS shell; cite the API. GitHub: master = 789c32fd (1.2.0, >=415558); development = e588fab (2026-10-08), uplugin 1.3.0, >=502094, SML ^3.12.0. The latest release and tag is v1.2.0. PR #534 is closed, merged=false, and the maintainer commented 'Similar to #533 and for same reasons, close.'

Evidence: <https://api.ficsit.app/v2/query>

### fin-interop-no-dependency (verified-primary, decision-driving)

A new mod can expose its buildables to FIN Lua computers without depending on FIN: FIN's reflection picks up netFunc_/netProp_/netSig_ prefixed members and a scene component named FINConnector (or any power connection component). Only varargs functions need FIN as a dependency. Linking FIN's own C++ modules is the route the GPL FAQ treats as a combined work (not legal advice), and building FIN itself needs SML_Patch.patch applied to the SDK headers. FIN's own mod-integration convention is therefore a link-free interop point.

Evidence:

- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/docs/modules/ROOT/pages/ModIntegration.adoc>

  ```text
  even without the need of having FIN as a dependency.
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/docs/modules/ROOT/pages/ModIntegration.adoc>

  ```text
  You can declare a custom function by simply calling it in any way but with the `netFunc_` Prefix
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/docs/modules/ROOT/pages/ModIntegration.adoc>

  ```text
  Alternatively you can have a simple Scene Component with the name `FINConnector`
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/docs/modules/ROOT/pages/ModIntegration.adoc>

  ```text
  but then you sadly need to have FIN as dependency
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/Source/FicsItNetworksCircuit/Private/FINNetworkAdapter.cpp>

  ```text
  GetName().EndsWith("FINConnector")
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/README.md>

  ```text
  Apply the `SML_Patch.patch` file using `git apply` to the Satisfactory Modloader Repository folder.
  ```

**Skeptic (source-accuracy): confirmed**

Note: All six quotes found verbatim. Code confirms the doc: FIRSourceUObject.cpp matches netProp_, netPropReadOnly_, netPropGet_, netFunc_ and netSig_; FINNetworkAdapter.cpp:62 has GetName().EndsWith("FINConnector"). Those files, and the doc sentence, also exist at v1.2.0 (789c32fd). README step 3 says SML_Patch.patch adds header changes needed to build FIN; the patch touches FactoryGame headers. Caveats: the doc names varargs as needing FIN but never says 'only' (FFIRAnyValue is a FIN type). The doc warns that replicated netProp_ variables are not allowed and might crash, which matters for multiplayer. Live example: Circuitry creates a "FINConnector" component (FGWiremodBuildable.h:366) and its uplugin depends only on SML. The hook does nothing for players until FIN publishes a release working on CL 502094.

Evidence: <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/docs/modules/ROOT/pages/ModIntegration.adoc>

### circuitry-tick-model (verified-primary, decision-driving)

Circuitry's evaluation model (code facts): every gate is its own AFGBuildable actor with PrimaryActorTick enabled; Tick calls ServerProcess(DeltaTime) on the server. Inputs are FConnectionData = UObject* + FunctionName (+FromProperty), read by name through reflection each time a gate runs (pull model). State elements are frame-based (D flip-flop uses HasClockedLastFrame, Timer accumulates DeltaTime). A grep of all 489 C++ files in Source/ finds no TickGroup/AddTickPrerequisite (Blueprint content under Content/ was not inspected). Connections are explicit point-to-point wires drawn with a tool (FConnectionData.WirePositions), with typed values (Boolean, Number, String, Vector, Inventory, ...).

Evidence:

- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/Source/FicsitWiremod/Private/Behaviour/FGWiremodBuildable.cpp>

  ```text
  if(HasAuthority()) ServerProcess(DeltaTime);
  ```
- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/Source/FicsitWiremod/Public/Behaviour/FGWiremodBuildable.h>

  ```text
  PrimaryActorTick.bStartWithTickEnabled = true;
  ```
- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/Source/FicsitWiremod/Public/WiremodReflection.h>

  ```text
  type get_name() const { return UReflectionUtilities::GetUnmanaged<type>(ToPointer()); }
  ```
- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/Source/FicsitWiremod/Public/Behaviour/Gates/Logic/DFlipFlop.h>

  ```text
  if(HasClockedLastFrame) return;
  ```
- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/Source/FicsitWiremod/Public/Behaviour/Gates/Logic/Timer.h>

  ```text
  CurrentTime += DeltaTime;
  ```

**Skeptic (source-accuracy): confirmed**

Note: Quotes verified. Tick runs `if(HasAuthority()) ServerProcess(DeltaTime); ClientProcess(DeltaTime);`, so ClientProcess runs on every machine. The header sets bCanEverTick = true and bStartWithTickEnabled = true and disables the vanilla mFactoryTickFunction. FConnectionData holds Object, FunctionName, FromProperty, WirePositions and ConnectionType. Reads go through FindFunction/FindPropertyByName/ProcessEvent each call. DFlipFlop uses HasClockedLastFrame and Timer does CurrentTime += DeltaTime. Source/ has 489 files: 488 .cpp/.h plus FicsitWiremod.Build.cs. My grep for TickGroup, AddTickPrerequisite and SetTickGroup found nothing; Blueprint content was not inspected. Correction: not every actor polls per frame, because WiremodVanillaConnections.h sets TickInterval = 0.5. EConnectionType has about 43 values beyond the 5 listed.

Evidence: <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/Source/FicsitWiremod/Private/Behaviour/FGWiremodBuildable.cpp>

### circuitry-order-inference (secondary)

INFERENCE (not observed at runtime): because each gate is a separate actor ticked individually and reads its source's current value on demand, a chain of gates can see values from the same or the previous frame depending on actor tick order, and timing scales with frame rate. Epic states that tick order matters to avoid off-by-one-frame issues and documents tick prerequisites, which Circuitry's C++ does not set. So a deterministic, frame-rate-independent discrete-tick model (like Redstone's fixed tick with repeater delays) was NOT found in Circuitry or FIN; that is the most defensible differentiator, but it must be tested in-game before it is advertised.

Evidence:

- <https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-ticking-in-unreal-engine>

  ```text
  is essential to avoid off-by-one frame issues, as well as to ensure consistency in the way your game runs.
  ```
- <https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-ticking-in-unreal-engine>

  ```text
  will set the actor or component on which the function is called to wait to tick until the specified other actor or component has finished ticking.
  ```

### smr-naming-process (verified-primary, decision-driving)

SMR facts that constrain naming and release: the mod reference is the unique name, cannot contain special characters or spaces, and cannot be changed after the mod is added; C++ mods go through an automated approval of about 1 to 3 minutes. createMod rejects an exact-match existing reference (and a reference equal to an existing mod ID) and a lowercase-compared blocklist (satisfactory, factorygame, sml, satisfactorymodloader, examplemod, docmod, gamefeatures). The reference 'Circuitry' is already taken by a hidden, approved placeholder mod named 'Circuitry 2' (0 downloads), and 'Circuitry' is also the display brand of FicsitWiremod. Deleted references cannot be seen through the API.

Evidence:

- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/UploadToSMR.adoc>

  ```text
  **You can't change your mod reference after you have added your mod to SMR!**
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/UploadToSMR.adoc>

  ```text
  Your mod reference cannot contain special characters or spaces.
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/UploadToSMR.adoc>

  ```text
  The approval process is generally quite quick, about 1 to 3 minutes,
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_mods.go>

  ```text
  return nil, errors.New("mod with this mod reference already exists")
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_mods.go>

  ```text
  "examplemod":            true,
  ```
- <https://ficsit.app/mod/Circuitry>

  ```text
  Mod reference hoarding goes BRRRRRR
  ```
- <https://ficsit.app/mod/FicsitWiremod>

  ```text
  "name":"Circuitry - Logic gates and More"
  ```

**Skeptic (source-accuracy): confirmed**

Note: Docs quotes found at d35cc911 and identical on dev 8175a04. smr-api f1f39d09 is current HEAD. CreateMod rejects a blocklisted ToLower(ref) (the 7 listed names), an exact existing mod_reference ('mod with this mod reference already exists'), and a ref equal to an existing mod ID. UpdateMod blocks changing a reference once set. API: reference 'Circuitry' = mod 'Circuitry 2', hidden true, approved true, 0 downloads, short_description 'Mod reference hoarding goes BRRRRRR'; FicsitWiremod's name is 'Circuitry - Logic gates and More'. Quotes come from the API, not the page shell. Extra constraints: mod_reference MaxLen(32), and docs say the plugin and C++ source folder names must equal the reference. Deleted references: Mod uses SoftDeleteMixin (queries hide deleted rows) and migration 000011 creates a plain unique index with no WHERE clause. So a deleted reference stays invisible to the API but is probably still blocked by a DB constraint error rather than the friendly message (inferred from code, not tested). The only real availability test is attempting to create.

Evidence: <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_mods.go>

### editor-split (verified-primary, decision-driving)

Both reference mods are hybrid C++ + Unreal assets, which answers what needs the editor. Logic classes are plain C++ (Circuitry: header-only AAND, ASRLatch, ATimer; FIN: AFINNetworkCable, AFINNetworkCircuit, AFINSignalSubsystem), but each buildable also ships Build_/Desc_/Recipe_ (and hologram/mesh/UI) .uasset files. FIN development branch: 219 .cpp and 220 .h under Source/ vs 1,090 .uasset under Content/. Circuitry: 489 files under Source/ vs 1,439 .uasset (214 Build_ and 218 Recipe_ assets). Unlike these, the owner's CompanionPresence has no .uasset, so a logic mod with placeable buildables needs the Unreal editor for assets while the signal/solver core can be plain, unit-testable C++.

Evidence:

- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/Source/FicsItNetworksCircuit/Public/FINNetworkCable.h>

  ```text
  class FICSITNETWORKSCIRCUIT_API AFINNetworkCable : public AFGBuildable {
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/Content/Buildings/Network/NetworkCable/Build_NetworkCable.uasset>

  ```text
  Content/Buildings/Network/NetworkCable/Build_NetworkCable.uasset
  ```
- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/Source/FicsitWiremod/Public/Behaviour/Gates/Logic/AND.h>

  ```text
  class FICSITWIREMOD_API AAND : public AMultistateWiremodBuildable
  ```
- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/Content/Buildings/Breakers/BreakItemDescriptor/Recipe_BreakItemDescriptor.uasset>

  ```text
  Content/Buildings/Breakers/BreakItemDescriptor/Recipe_BreakItemDescriptor.uasset
  ```

**Skeptic (source-accuracy): confirmed**

Corrected statement: Counts verified from git ls-files: FIN dev e588fab has 219 .cpp, 220 .h and 1,090 .uasset (Content/Buildings/Network/NetworkCable has Build_, Desc_, Recipe_, a hologram BP and meshes). Circuitry 4e9fff3 has 489 files under Source/ (488 .cpp/.h plus Build.cs) and 1,439 .uasset, of which 214 are Build_, 218 Recipe_ and 219 Desc_. Wording fix: AAND, ASRLatch and ATimer keep their logic inline in the headers, but each has a one-line .cpp that only includes the header, so 'header-only' is not literal. 'Core can be plain unit-testable C++' is a design inference, not shown by these sources: Circuitry's gates depend on UE reflection (UObject*, FindFunction, ProcessEvent).

Note: The two Content paths exist at the pinned commits. The 'CompanionPresence has no .uasset' claim is owner-supplied. The packaged LinuxServer zip is consistent with it (pak 832 B, ucas 64 B, 0 loose .uasset) but does not prove the source Content/ is empty.

Evidence: <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/Source/FicsItNetworksCircuit/Public/FINNetworkCable.h>

### fin-architecture (verified-primary)

FIN architecture: a network is a graph of nodes. Cables are C++ buildable actors joining network connectors; AFINNetworkCircuit (an AActor) caches and merges/splits node sets when cables are built or dismantled. Components are addressed by GUID and nicks. Signals are push events (sender -> listener traces) stored in AFINSignalSubsystem, so computers do not poll. Users program in Lua 5.4 (custom Eris fork for persistence) running on in-game computers with a Lua CPU, RAM and EEPROM; modular I/O panels give buttons/levers/LEDs; the design is inspired by OpenComputers. Typical use is event.listen(switch) then event.pull() in a loop.

Evidence:

- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/Source/FicsItNetworksCircuit/Public/FINNetworkCircuit.h>

  ```text
  Manages and caches a computer network circuit.
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/Source/FicsItNetworksCircuit/Public/Signals/FINSignalSubsystem.h>

  ```text
  Map of sender objects to array of receiver traces, traces point from sender to receiver
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/docs/modules/ROOT/pages/BasicConcept.adoc>

  ```text
  You can build a network by simply connecting a *network cable* to each of the components you wish to include.
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/README.md>

  ```text
  FicsIt-Networks relies on a slightly custom version of Eris for Lua 5.4 to run Lua code and persistency.
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/README.md>

  ```text
  It is inspired by [OpenComputers]
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/e588fab48f0b2af6d57cc2be2416c2a3f9cf4d7a/docs/modules/ROOT/pages/lua/examples/lightSwitch.adoc>

  ```text
  event.listen(switch)
  ```

### frm-interop (verified-primary)

FRM is a data/telemetry bridge, not an in-game logic system, but it can act on vanilla buildings: setEnabled turns constructors, assemblers, manufacturers, power generators and (priority) power switches on/off, and setSwitches edits power switches, via POST with a token. Auth header inconsistency: the code checks X-FRM-Authorization (authentication.adoc agrees) while the setEnabled example uses 'Authorization'; do not rely on the example. So if the new mod's actuators are vanilla power switches or machines, the owner's dashboard sees and drives them through FRM with no code dependency. I found no third-party endpoint-registration API in FRM (not found, not proven absent).

Evidence:

- <https://github.com/porisius/FicsitRemoteMonitoring/blob/32fe64e0c22389a944c27222ef6c881f5e207072/docs/modules/ROOT/pages/json/Write/setEnabled.adoc>

  ```text
  Allows to enable or disable a Constructor, Assembler, Manufacturer, Power Generators, (Priority) Power Switch.
  ```
- <https://github.com/porisius/FicsitRemoteMonitoring/blob/32fe64e0c22389a944c27222ef6c881f5e207072/Source/FicsitRemoteMonitoringServer/Private/FRM_Controller.cpp>

  ```text
  if (RequestHeaders.Contains("X-FRM-Authorization"))
  ```
- <https://github.com/porisius/FicsitRemoteMonitoring/blob/32fe64e0c22389a944c27222ef6c881f5e207072/docs/modules/ROOT/pages/json/authentication.adoc>

  ```text
  `X-FRM-Authorization: <token goes here>`
  ```
- <https://github.com/porisius/FicsitRemoteMonitoring/blob/32fe64e0c22389a944c27222ef6c881f5e207072/docs/modules/ROOT/pages/json/Write/setEnabled.adoc>

  ```text
  -H 'Authorization: <your access token>'
  ```

### circuitry-interop (verified-primary)

Circuitry accepts third-party buildings without linking: JSON data tables dropped in a CircuitryAPI folder, with the file named after the mod reference, map function/property names and connection types onto a building; Circuitry also fetches compatibility packages over HTTP at launch (disclosed on SMR). So a new mod can become Circuitry-compatible by shipping a JSON table.

Evidence:

- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/Source/FicsitWiremod/Public/WiremodAPI.h>

  ```text
  The file name must match the mod reference!
  ```
- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/Source/FicsitWiremod/Public/WiremodAPI.h>

  ```text
  Drop your JSON table in this folder and circuitry will attempt to add connections to buildings.
  ```
- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/Source/FicsitWiremod/Public/Behaviour/CircuitryConnectionsProvider.h>

  ```text
  Function/property name that circuitry will look for to get/set the value
  ```

### no-redstone-on-smr (verified-primary)

No mod among the 1,894 SMR entries (1,608 visible + 286 hidden, enumerated with getMods hidden:true) contains the word 'redstone' in its name or full description. Keyword searches (logic, gate, signal, circuit, wire, sensor, relay, switch, timer, network, clock, latch, lua, computer, controller, programmable, plus a local regex over all names and descriptions) turned up Circuitry and FIN as the only dedicated in-game logic/programming systems I found; PowerControlRoom (133,867 downloads) adds an 'Auto Switch: AND' and power-channel switching, and ArduinoKit/FRM/LightControl are external-bridge mods. Absence claim limited to SMR text fields and to the search method used.

Evidence:

- <https://ficsit.app/mod/PowerControlRoom>

  ```text
  From wireless transmission to automated logic gates, this mod provides the high-tech tools needed to manage complex power networks.
  ```
- <https://ficsit.app/mod/PowerControlRoom>

  ```text
  Auto Switch: AND
  ```

### fin-ecosystem (verified-primary)

FIN is the de-facto programmable layer other mods integrate with (EfficiencyCheckerMod 471,892 downloads, Dispenser, Hopper, Inserter, InserterPlus and TrueNukes mention FIN support), and Circuitry is named as supported by MkPlus/MkPlusLite. Interoperating with either is therefore a realistic distribution advantage, subject to FIN's current Broken status.

Evidence:

- <https://ficsit.app/mod/Dispenser>

  ```text
  Item Dispensers have
  ```
- <https://ficsit.app/mod/EfficiencyCheckerMod>

  ```text
  FicsIt-Networks integration
  ```
- <https://ficsit.app/mod/MkPlus>

  ```text
  Supported by [Circuitry](https://ficsit.app/mod/FicsitWiremod) since v0.10.70
  ```

### smr-ai-disclosure (verified-primary)

SMR has per-mod AI-use and network-use disclosure fields; an AI-use disclosure of type ai_usage or runtime_ai_usage requires a message, and once set it cannot be cleared. Because the project is AI-assisted, the disclosure decision is permanent and should be made deliberately before the first publish.

Evidence:

- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_mods.go>

  ```text
  you need to input a disclosure message when disclosing AI usage
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_mods.go>

  ```text
  this mod already has an AI use disclosure, and thus it cannot be cleared
  ```

### redstone-name-and-semantics (verified-primary)

Naming risk and semantic baseline. Mojang defines 'our name' to include game features and confusingly similar names, forbids using the Minecraft name as the primary or dominant name, and asks for a 'NOT AN OFFICIAL MINECRAFT ...' disclaimer when any part of the name is used. Whether the word 'Redstone' itself is covered is not stated on the page (unverified), so FicsitRedstone is free on SMR/GitHub but not recommended. Semantic baseline: Redstone's basic time unit is the 'redstone tick', two game ticks (a community term on Java; Bedrock's redstone system runs on its own deterministic loop).

Evidence:

- <https://www.minecraft.net/en-us/usage-guidelines>

  ```text
  We also mean any names which are confusingly similar to our name
  ```
- <https://www.minecraft.net/en-us/usage-guidelines>

  ```text
  You may not use the Minecraft name as the primary or dominant name or title.
  ```
- <https://www.minecraft.net/en-us/usage-guidelines>

  ```text
  NOT AN OFFICIAL MINECRAFT [PRODUCT/SERVICE/EVENT/etc.]. NOT APPROVED BY OR ASSOCIATED WITH MOJANG OR MICROSOFT
  ```
- <https://minecraft.wiki/w/Tick>

  ```text
  The term " redstone tick " is a community-made term describing a basic unit of time in redstone mechanics: a delay of two game ticks
  ```
- <https://minecraft.wiki/w/Tick>

  ```text
  The redstone system is calculated independently of the main game tick loop.
  ```

## Catalog

| Name | Reference | Licence | Last updated | Relevance |
| --- | --- | --- | --- | --- |
| [FicsIt-Networks](https://ficsit.app/mod/FicsItNetworks) | FicsItNetworks | GPL-3.0 (LICENSE at repo root, GNU GPL v3, 29 June 2007) | 2025-09-09 | Programmable computers + Lua 5.4 + network cables, signals/events, reflection API over game objects, modular I/O panels. Closest to 'programming'. Latest SMR release 1.2.0 flagged Broken by UE 5.6.1; dev branch 1.3.0 targets game >=502094 but is unreleased. |
| [Circuitry - Logic gates and More](https://ficsit.app/mod/FicsitWiremod) | FicsitWiremod | No LICENSE file in repo (tree at 4e9fff3e has none; GitHub API reports none); default copyright applies, no reuse right | 2026-09-26 | DIRECT PRIOR ART: logic gates (AND/OR/NOT/XOR/NAND/NOR/XNOR), SR latch, D/T flip-flops, timer, edge detectors, bool<->int array and hex conversion, multiplexer, transistor, radio, sensors, actuators on vanilla machines, HTTP module. Works on current game CL 502094, LinuxServer/WindowsServer targets, required_on_remote=true. |
| [FicsIt Remote Monitoring](https://ficsit.app/mod/FicsitRemoteMonitoring) | FicsitRemoteMonitoring | No LICENSE file in repo root (GitHub API reports none); default copyright applies; README grants only: 'Feel free to create your own web UI or serial apps.' | 2026-08-12 | Read API (getPower, getSwitches, ...) plus write API setEnabled/setSwitches over HTTP with token auth; webhooks; serial. Bridge to the owner's dashboard; not an in-game logic system. Works on EA/EXP. |
| [Power Control Room](https://ficsit.app/mod/PowerControlRoom) | PowerControlRoom | No LICENSE file in repo root (GitHub API reports none); default copyright applies | 2026-06-10 | Power-network switching/logic: 'Auto Switch: AND', 12x12/1x12 switches, wireless power channels, potentiometer, timer/time-of-day switches. Logic carried by power, not by dedicated signals. |
| [ArduinoKit](https://ficsit.app/mod/ArduinoKit) | ArduinoKit | No LICENSE file in repo root (GitHub API reports none); default copyright applies | 2026-06-09 | RS232 serial interface to external microcontrollers; external programmability path. |
| [Satisfactory Control Panel](https://ficsit.app/mod/SFControlPanel) | SFControlPanel | No LICENSE file in repo root (GitHub API reports none); default copyright applies | 2026-06-09 | 'Control Panel for Satisfactory' (Windows target only); small, 505 downloads. |
| [Event Utilities](https://ficsit.app/mod/GameShow) | GameShow | No LICENSE file in repo root (GitHub API reports none); default copyright applies | 2025-10-16 | Sends webhook messages when players interact with in-game buildings; flagged Broken by UE 5.6.1. |
| [Power Switch Auto Turn On](https://ficsit.app/mod/PowerSwitchAutoTurnOn) | PowerSwitchAutoTurnOn | No LICENSE file in repo root (GitHub API reports none); default copyright applies | 2026-06-10 | Automation of vanilla power switches by capacity/priority rules. |
| [Wireless Power Switch](https://ficsit.app/mod/WirelessPowerSwitch) | WirelessPowerSwitch | No public source listed on SMR | 2026-08-13 | Wireless power switch (channel-style remote switching). |
| [Powered Splitter and Merger](https://ficsit.app/mod/PoweredSplitterAndMerger) | PoweredSplitterAndMerger | No LICENSE file in repo root (GitHub API reports none); default copyright applies | 2023-04-14 | Splitters/mergers whose outputs are controlled by power switches (power as control signal); last version 2023-04-14. |
| [Throughput Counter and Limiter](https://ficsit.app/mod/CounterLimiter) | CounterLimiter | GPL-3.0 (LICENSE.txt in mklierman/SatisfactoryMods) | 2026-08-17 | Throughput counter and optional limiter belt attachment; sensor/actuator on belts, no external signal input. |
| [Remote Factory](https://ficsit.app/mod/Cellphone) | Cellphone | No LICENSE file in repo root (GitHub API reports none); default copyright applies | 2026-08-16 | Factory remote control and stats UI (ContentLib based). |
| [Dynamic Flow Splitter](https://ficsit.app/mod/LinkedSplitters) | LinkedSplitters | No public source listed on SMR | 2026-06-11 | Demand-driven automatic routing of resources ('global objectives'); automation, not signals. |
| [Item Teleporter Redux V2](https://ficsit.app/mod/StorageTeleporterReduxV2) | StorageTeleporterReduxV2 | No LICENSE file in repo root (GitHub API reports none); default copyright applies | 2026-10-10 | Item teleport via configurable channels; channel concept similar to wireless signals but for items. |
| [Smart Wireless Lighting](https://ficsit.app/mod/SmartWirelessLighting) | SmartWirelessLighting | GPL-3.0 | 2025-06-11 | Wireless light connections for lighting panels; flagged Broken by UE 5.6.1. |
| [Light Control](https://ficsit.app/mod/LightControl) | LightControl | MIT | 2025-08-15 | Controls built-in lights from external Art-Net/DMX software; external control precedent. |
| [SpiffCo Bridge](https://ficsit.app/mod/SpiffCoBridge) | SpiffCoBridge | MIT (repo SpiffCo_Command_Center) | 2026-07-25 | Streams live game data to a web app; telemetry precedent like FRM. |
| [Energy Grid Exporter](https://ficsit.app/mod/EnergyGridExporter) | EnergyGridExporter | No LICENSE file in repo root (GitHub API reports none); default copyright applies | 2026-07-27 | Exports the energy grid through a web server/socket; telemetry. |
| [DI Power Indicators](https://ficsit.app/mod/DI_Power_Indicators) | DI_Power_Indicators | No public source listed on SMR | 2026-06-07 | Power-grid status indicators (monitoring only). |
| [Programmable Elevators](https://ficsit.app/mod/ProgrammableElevatorMod) | ProgrammableElevatorMod | No public source listed on SMR | 2020-09-19 | Programmable multi-floor elevators; last version 2020-09-19. |
| [Vertical Logistics](https://ficsit.app/mod/VerticalLogistics) | VerticalLogistics | No public source listed on SMR | 2026-10-01 | Adds vertical mergers/splitters and programmable splitters (item-filter routing, no external signal input). |
| [ContentLib](https://ficsit.app/mod/ContentLib) | ContentLib | No LICENSE file in repo root (GitHub API reports none); default copyright applies | 2026-06-21 | JSON-defined items/recipes/schematics for any mod; relevant as the data-only route to recipes/unlocks without editor assets. |
| [[Discontinued] Wiremod: RSS](https://ficsit.app/mod/WiremodRSS) | WiremodRSS | No LICENSE file in repo root (GitHub API reports none); default copyright applies | 2023-01-05 | Discontinued Wiremod addon (RSS sign integration); shows the addon-ecosystem pattern. |
| [Circuitry 2](https://ficsit.app/mod/Circuitry) | Circuitry | Not applicable (hidden placeholder, no source URL) | no released version | Hidden mod 'Circuitry 2' (0 downloads, no versions) squatting mod reference 'Circuitry'; description says 'Mod reference hoarding goes BRRRRRR'. |
| [Communication Signals](https://ficsit.app/mod/ComSignal) | ComSignal | No public source listed on SMR | no released version | Hidden, never released: 'A mod to add various methods of long range communications within Satisfactory'. |
| [JSCup - An Unreal.js wrapper](https://ficsit.app/mod/JSCup) | JSCup | No public source listed on SMR | no released version | Hidden, never released: 'Allows you to create Satisfactory mods using JavaScript'. |
| [Satisfactory Power Webserver](https://ficsit.app/mod/Alpha) | Alpha | No public source listed on SMR | no released version | Hidden, never released ('Satisfactory Power Webserver'): remote power monitoring and control. |
| [Statisfactory](https://ficsit.app/mod/Statisfactory) | Statisfactory | MIT | no released version | Hidden, never released: 'HTTP API stats for Satisfactory'. |

## Name checks (SMR references, 2026-10-13)

| Candidate | Exists in SMR | Similar existing | Note |
| --- | --- | --- | --- |
| FicsitStone | no | none | getModByReference 'FicsitStone' and 'ficsitstone' both return 'mod not found'; no normalized name/reference match among 1,894 SMR mods; 0 GitHub repos with this name. 'Stone' evokes Redstone, see the Mojang naming claim. |
| FicsitLogic | no | FicsitWiremod (Circuitry - Logic gates and More) | Not found by exact or lowercase reference; 0 GitHub repos. Semantically overlaps Circuitry's tagline 'Logic gates and More', which may confuse users. |
| Ficsitron | no | none | Not found by exact or lowercase reference; 0 GitHub repos and 0 GitHub users. Follows the common Ficsit prefix pattern (FicsItNetworks, FicsitRemoteMonitoring, FicsitWiremod). |
| LogicGrid | no | TrueGrid (power-grid overhaul mod, different purpose) | Not found on SMR. GitHub has 25 repos whose name contains it, e.g. nik251021/LogicGrid (3 stars) and logicgrid-dev/LogicGrid (2 stars); names are owner-scoped so this is not a block, only search/SEO noise. |
| SignalWorks | no | TrainSignalBridge, TunnelSignals (rail signal mods) | Not found on SMR. GitHub has 26 repos with a matching name, e.g. TimeViewers/signalworks (4 stars), SUAISW/SignalWorks. 'Signal' is already used on SMR for rail signals, so users may expect train signals. |
| Relay | no | none on SMR | Not found as reference or display name on SMR, but 'Relay' is generic. GitHub: 45,777 repos with the name, led by facebook/relay (18,961 stars) and hyvor/relay; very poor discoverability. |
| PulseCircuits | no | Circuitry (brand proximity only) | Not found on SMR; 0 GitHub repos. The word 'Circuits' sits close to Circuitry's brand. |
| SatisLogic | no | none | Not found by exact or lowercase reference; 0 GitHub repos. |
| Circuitry | yes | FicsitWiremod (Circuitry - Logic gates and More) | Mod reference 'Circuitry' exists: hidden, approved placeholder 'Circuitry 2' (id 7fMx4XAi9wZnph, 0 downloads, no versions, description 'Mod reference hoarding goes BRRRRRR'), whose page redirects users to FicsitWiremod. FicsitWiremod itself is branded 'Circuitry - Logic gates and More', so the name collides with the direct competitor. GitHub has 332 repos named circuitry (kapost/circuitry, 134 stars). Do not use. |
| FicsitRedstone | no | none | Free on SMR and GitHub (0 repos), but not recommended: Mojang's usage guidelines define its name to include features and confusingly similar names and require a not-official disclaimer; whether the word 'Redstone' itself is covered is unverified. Describing the mod as 'redstone-inspired' in text is a separate, lower-risk choice. |
| LogicSit | no | none | Not found by exact or lowercase reference; no exact GitHub repo (the 5 hits are 'LogicSite', a different word). Reads as a typo of 'LogicSite' in search. |
| Gatekeeper | no | none on SMR | Not found on SMR, but generic: GitHub has 4,371 repos with the name, led by open-policy-agent/gatekeeper (4,297 stars, Kubernetes) and AltraMayor/gatekeeper, so searches would be dominated by unrelated security projects. |

## Open questions

- Does Circuitry actually show evaluation-order or frame-rate dependence in-game? The claim is inferred from C++ only; Blueprint logic under Content/ was not inspected and nothing was run. Needs a test on the owner's PC.
- Circuitry's wiki (cc.gotpa.ws/wiki) loads its content dynamically and could not be read, so documented features beyond the SMR page and source (and performance at scale) are unconfirmed.
- When will FIN ship a build for UE 5.6.1 (development branch 1.3.0 exists, no release)? Does the owner's server already run FIN? Only FRM 1.5.3 plus about 15 unnamed mods were stated.
- No Coffee Stain policy on using 'Ficsit/FICSIT' in mod names or on distributing mods was found, although the Ficsit prefix is common on SMR.
- Is SMR reference uniqueness case-insensitive in the database? The Go code compares exactly and 'circuitry' (lowercase) returned not found, but database collation was not checked. Deleted references are invisible to the API, so final availability must be rechecked right before upload.
- Is the word 'Redstone' itself covered by Mojang's naming rules? The guidelines page does not say; legal advice would be needed before using it in a name.
- FRM docs conflict on the auth header (code and authentication.adoc use X-FRM-Authorization, one example uses Authorization), and no FRM extension API for third-party endpoints was found; maintainers could confirm.
- The SMR AI-use disclosure (permanent once set) and the owner's own license choice for the new mod still need an explicit decision; a GPL choice would allow linking FIN, a permissive one would not.
- Search limits: SMR rejected the one-letter-short term 'if' (minimum search length); multi-word GitHub searches with spaces returned 0, so '+' queries were used; GitHub code search, Discord, Reddit and the Unreal Marketplace were not searched. SDK pin d2162c99 was not needed for this topic (third-party mod code was read, not the SML source).
- Interop paths (FIN netFunc_ naming, Circuitry CircuitryAPI JSON) are documented but untested against the owner's build, and 'required_on_remote=true' means every player must install the new mod, as with FIN and Circuitry.

## Recommendations

- Treat Circuitry as the incumbent: do not pitch 'logic gates for Satisfactory' as new. Position the mod on a deterministic, frame-rate-independent discrete-tick signal model with propagation delay, but first confirm in-game that Circuitry really lacks it.
- Design clean-room: copy nothing from Circuitry (no license) or FIN (GPL-3.0), do not link FIN, and write the signal/solver core as plain C++ with no Unreal types so it can be built and unit-tested on this VPS with g++, keeping UE wrappers thin.
- Interoperate without linking: FIN netFunc_/netProp_/netSig_ plus a FINConnector component, a Circuitry JSON table named after our mod reference in CircuitryAPI/, and vanilla power switches or machines as actuators so FRM setEnabled/setSwitches can drive them from the owner's dashboard. Keep every integration optional.
- Do not depend on FIN until a release built for UE 5.6.1 is on SMR; Circuitry is the only current logic mod on the owner's build.
- Pick the name from the free, low-collision set (Ficsitron, SatisLogic, PulseCircuits, FicsitLogic) and re-run getModByReference immediately before the first upload; avoid Circuitry, FicsitRedstone, Relay and Gatekeeper. The reference is permanent and the AI-use disclosure cannot be cleared once set.
- Plan for the Unreal editor on the owner's PC for Build_/Desc_/Recipe_/hologram/mesh assets (both reference mods ship about 1,000 to 1,400 .uasset files), and consider ContentLib JSON only as an optional route for recipes and schematics.

