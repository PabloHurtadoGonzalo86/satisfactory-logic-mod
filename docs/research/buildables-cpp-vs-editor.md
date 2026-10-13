# Buildables: C++ versus editor

_Generated from `data/buildables-cpp-vs-editor.json` by `tools/render_research.py`; do not edit by hand. Research run of 2026-10-13. Claims are the researcher's; verdicts are from independent skeptics told to refute them. Wording is kept verbatim (English)._

## Summary

Verdict: all buildable logic and data can be native C++ (text, reviewable on Linux). The Unreal editor is needed only as build/package host and for binary assets (meshes, textures, materials), which can be script-generated or partly avoided.

FactoryGame headers are public at the owner's SDK pin d2162c99, which equals master HEAD today. UFGBuildingDescriptor, UFGRecipe, UFGSchematic, UFGUnlockRecipe, UFGBuildCategory/SubCategory, AFGBuildable and AFGBuildableHologram are ordinary UCLASSes whose EditDefaultsOnly fields a subclass constructor can set. SML 3.12.0 accepts native classes: it discovers native UGameWorldModule subclasses (bRootModule), resolves /Script/ owners, and RegisterSchematic auto-registers recipes and their products. There is no register-buildable API.

Existence proof: Teleport Logistics (MIT, SML 3.12.0, CL502094, approved on SMR, 1,519 downloads) does all of it in C++, including Slate UI and RPCs. Its only assets are meshes/textures/materials imported by an editor Python script; no Blueprints, no GameFeatureData. Its repo lists retail checks as pending.

Costs: AFGBuildable::mIsUseable and mInteractWidgetSoftClass are private, so an AccessTransformers Friend entry is needed (SDK-wide rebuild). Native unlocks do not render in reward/MAM UI; two mods swap in stock BP unlock classes at runtime. Avoid asset loads in constructors. Docs demand FGColoredInstancedMeshProxy but the shipped mod uses UStaticMeshComponent (unresolved).

Editor workload for a custom mesh: build Development Editor once, run one Python import script, Alpakit package. No Blueprint or UMG authoring. The SDK uproject enables PythonScriptPlugin and EditorScriptingUtilities; whether the access-controlled CSS engine binary ships them is unverified.

Use a plain plugin (Alpakit Library template), not GameFeature: GameFeatureData is an editor-made asset needed only for icon libraries, user settings, ADA messages and input contexts.

On the Linux VPS only an engine-independent logic core can be compiled and tested (g++ -std=c++20 with sanitizers, as Teleport does); UObject glue cannot.

SMR requires AI-usage disclosure. Prior art exists: Circuitry (FicsitWiremod), 195k downloads, no licence.

## Claims at a glance

| Id | Confidence | Decision-driving | Skeptic verdicts |
| --- | --- | --- | --- |
| c1 | verified-primary | no | not verified (not decision-driving) |
| c2 | verified-primary | no | not verified (not decision-driving) |
| c3 | verified-primary | yes | partially-correct, confirmed |
| c4 | verified-primary | yes | partially-correct, partially-correct |
| c5 | verified-primary | yes | partially-correct, partially-correct |
| c6 | verified-primary | yes | confirmed, confirmed |
| c6b | unverified | no | not verified (not decision-driving) |
| c7 | verified-primary | yes | confirmed, partially-correct |
| c8 | verified-primary | no | not verified (not decision-driving) |
| c9 | verified-primary | yes | confirmed, confirmed |
| c10 | verified-primary | yes | confirmed, confirmed |
| c11 | verified-primary | no | not verified (not decision-driving) |
| c12 | secondary | yes | partially-correct, partially-correct |
| c13 | secondary | no | not verified (not decision-driving) |
| c14 | verified-primary | no | not verified (not decision-driving) |
| c15 | verified-primary | no | not verified (not decision-driving) |
| c16 | secondary | no | not verified (not decision-driving) |
| c17 | verified-primary | no | not verified (not decision-driving) |
| c18 | secondary | no | not verified (not decision-driving) |

## Claims in detail

### c1 (verified-primary)

FactoryGame headers ARE publicly browsable: 2,032 files live under Source/FactoryGame in the SDK repo at the owner's pinned commit d2162c99 (commit message 'Update headers to CL502094', 2026-08-11). master HEAD resolves to the same SHA as of 2026-10-13, so the 'compare with main' step is identical for every file I read. The Private/*.cpp files are generated skeletons, not game source, so behaviour must be inferred from headers/docs/other mods. The repo is GPL-3.0 at repo level and each header carries a Coffee Stain copyright line.

Evidence:

- <https://api.github.com/repos/satisfactorymodding/SatisfactoryModLoader/commits/d2162c999bbaa9650594ee9ad6e6e06190306009>

  ```text
  Update headers to CL502094
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildable.h>

  ```text
  // Copyright Coffee Stain Studios. All Rights Reserved.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Cpp/index.html>

  ```text
  CSS has been kind enough to provide us with the Satisfactory C++ headers.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/StarterProjectStructure.html>

  ```text
  but the C++ files, similar to the Content placeholders, are automatically generated skeletons of the files in the correct locations.
  ```

### c2 (verified-primary)

Every content type in the buildable chain is a plain native UCLASS whose EditDefaultsOnly members a derived C++ constructor (CDO) can set: UFGBuildingDescriptor::mBuildableClass; UFGItemDescriptor::mDisplayName/mDescription/mSmallIcon/mPersistentBigIcon/mCategory/mSubCategories/mMenuPriority; UFGRecipe::mIngredients/mProduct/mProducedIn (soft class list); UFGSchematic::mType/mTechTier/mCost/mUnlocks (Instanced); UFGUnlockRecipe::mRecipes (class is abstract, so a concrete native subclass is required); UFGBuildCategory and UFGBuildSubCategory (concrete UCLASS, fields mDisplayName/mMenuPriority/mCategoryIcon); AFGBuildable::mHologramClass/mDisplayName/mDescription (public); AFGBuildableHologram and AFGFactoryHologram (concrete UCLASS, usable as-is). The official tutorial builds them as Blueprints, but nothing in the headers requires that.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Resources/FGBuildingDescriptor.h>

  ```text
  UPROPERTY( EditDefaultsOnly, Category = "Building" ) TSubclassOf< class AFGBuildable > mBuildableClass;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Resources/FGItemDescriptor.h>

  ```text
  TObjectPtr<UTexture2D> mSmallIcon;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGRecipe.h>

  ```text
  TArray< TSoftClassPtr< UObject > > mProducedIn;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGSchematic.h>

  ```text
  TArray< TObjectPtr<class UFGUnlock> > mUnlocks;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Unlocks/FGUnlockRecipe.h>

  ```text
  UCLASS( Blueprintable, EditInlineNew, abstract, DefaultToInstanced )
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGBuildSubCategory.h>

  ```text
  class FACTORYGAME_API UFGBuildSubCategory : public UFGBuildCategory
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildable.h>

  ```text
  UPROPERTY( EditDefaultsOnly, Category = "Buildable" ) TSubclassOf< class AFGHologram > mHologramClass;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Hologram/FGBuildableHologram.h>

  ```text
  class FACTORYGAME_API AFGBuildableHologram : public AFGHologram
  ```

### c3 (verified-primary, decision-driving)

SML 3.12.0 content registration accepts native classes. (a) FPluginModuleLoader::FindRootModulesOfType discovers native UGameWorldModule/UGameInstanceModule subclasses (not only Blueprints) and reads bRootModule from the CDO, so a C++ class with bRootModule=true in its constructor is a valid root module (one per mod per module type, otherwise a Fatal log). (b) Content owner resolution explicitly handles the /Script/ mount point (native classes) by mapping module to plugin. (c) UGameWorldModule::mSchematics (TArray<TSubclassOf<UFGSchematic>>) is registered in the INITIALIZATION phase; RegisterSchematic auto-registers unlocked recipes and their products/ingredients as items. (d) There is no RegisterBuildable: a building becomes known because its descriptor is a recipe product. (e) Registration freezes at BeginPlay. Docs call UModContentRegistry a game-instance subsystem but the header derives from UWorldSubsystem (header wins).

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/ModLoading/PluginModuleLoader.cpp>

  ```text
  //Retrieve all loaded classes parenting from module class and check them
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/Kismet/BlueprintAssetHelperLibrary.cpp>

  ```text
  Script mount point is used for accessing native class/CDO objects, package name will equal Module Name,
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Public/Module/GameWorldModule.h>

  ```text
  List of schematics that will be automatically registered
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Public/Registry/ModContentRegistry.h>

  ```text
  All recipes referenced by schematic are registered automatically
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Public/Registry/ModContentRegistry.h>

  ```text
  Content can only be registered before save is loaded,
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/Module/WorldModuleManager.cpp>

  ```text
  Only one root module is allowed
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Public/Registry/ModContentRegistry.h>

  ```text
  class SML_API UModContentRegistry : public UWorldSubsystem
  ```

**Skeptic (source-accuracy): partially-correct**

Corrected statement: SML 3.12.0 accepts native content registration. FPluginModuleLoader::FindRootModulesOfType discovers native UGameWorldModule/UGameInstanceModule subclasses and reads bRootModule from the CDO. Setting bRootModule = true in the constructor is documented. One root module per plugin per module type, otherwise a Fatal log. The docs prefer a Blueprint child. Script-mount owner resolution maps /Script/<Module> to the plugin through the plugin descriptor's Modules list. mSchematics is registered in the INITIALIZATION phase. Schematic registration auto-registers the unlocked recipes and their products and ingredients as items, so there is no RegisterBuildable. Only the schematic and research-tree registries freeze (at BeginPlay), and the item registry never freezes. UModContentRegistry derives from UWorldSubsystem, although the docs call it a game-instance subsystem.

Note: Re-read at d2162c99 (which is also master HEAD, committed 2026-08-11 'Update headers to CL502094', so the main comparison is trivially identical). (a) CONFIRMED: PluginModuleLoader.cpp calls FindNativeClassesByType plus FindBlueprintAssetsByTag and then checks 'if (ModModuleCDO->bRootModule)'. WorldModuleManager.cpp logs Fatal 'Only one root module is allowed' per plugin and module type. docs ModModules page: 'To make a C++-defined module a root module, set bRootModule = true; in its constructor', but it also says 'it is recommenced to make a blueprint child of it root instead'. (b) CONFIRMED: BlueprintAssetHelperLibrary.cpp has 'Script mount point is used for accessing native class/CDO objects' and maps module name to plugin via the plugin descriptor Modules list. (c) CONFIRMED: GameWorldModule.cpp calls RegisterDefaultContent() when Phase == INITIALIZATION; AutoRegisterSchematicReferences -> UFGUnlockRecipe recipes -> AutoRegisterRecipeReferences registers products and ingredients as items. (d) CONFIRMED: no RegisterBuildable in header or cpp; only RegisterSchematic, RegisterResearchTree, RegisterRecipe and RegisterResourceSinkItemPointTable. (e) OVERBROAD: FreezeRegistry() is called only for the schematic (cpp line 377) and research-tree (line 432) registries, at BeginPlay. The cpp says the item registry is 'never frozen'. The docs say 'Schematic and Research Tree registrations are frozen on BeginPlay.' UWorldSubsystem vs docs 'game instance subsystem': header wins, CONFIRMED.

Evidence: <https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/Registry.html>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): confirmed**

Note: I read every cited file at the pinned SDK d2162c99. Master HEAD is still d2162c99 (2026-08-11) and v3.12.0 is the latest release, so the pin and main agree. (a) FindRootModulesOfType (lines 32-70) merges native classes (GetDerivedClasses) with BP classes and reads bRootModule from the CDO. WorldModuleManager.cpp:78-82 and GameInstanceModuleManager.cpp:53 log Fatal 'Only one root module is allowed'. Docs ModModules.adoc: 'set `bRootModule = true;` in its constructor' (docs recommend a BP child, but pure C++ works; Teleport Logistics does it). (b) BlueprintAssetHelperLibrary.cpp:121-135 handles the Script mount point. Scope: it maps to the mod plugin only when IsPluginAMod is true (EPluginType::Mod, default bTreatNonModPluginsAsGame=true). Otherwise the class is attributed to FactoryGame, so the plugin must live under Mods/. (c) GameWorldModule.cpp registers mSchematics in INITIALIZATION. AutoRegisterUnlockReferences and AutoRegisterRecipeReferences register unlocked recipes and their products and ingredients as items. (d) No RegisterBuildable exists anywhere under SML Mods. (e) Scope: FreezeRegistry is called only for the schematic and research-tree states (ModContentRegistry.cpp:377, 432), via ModifySchematicList at BeginPlay. Docs Registry.adoc:100: 'Schematic and Research Tree registrations are frozen on BeginPlay.' A late registration only logs Error plus ensure. The header derives from UWorldSubsystem, and the docs say 'game instance subsystem' (Registry.adoc:74). No contradicting source found.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/ModLoading/PluginModuleLoader.cpp>

### c4 (verified-primary, decision-driving)

Existence proof that the full chain can be native C++: Teleport Logistics (josh-tf, MIT, v0.5.12, built for CL502094 / SML 3.12.0 / UE 5.6.1) defines build category, sub-category, six building descriptors, recipes, UFGUnlockRecipe subclasses, two UFGSchematic milestones, a UGameWorldModule and UGameInstanceModule, buildable actors (hologram AFGFactoryHologram), power/belt/pipe connection components and an interaction widget, all in C++. Its only editor-authored content is ~35 mesh/material/texture .uasset files; it has no Blueprint assets and no GameFeatureData. It is live on SMR (4 approved versions seen, 0.5.12 approved, targets Windows + WindowsServer, 1,519 downloads at query time). Caveat: the repo is about a month old and its own validation.md lists the retail-game acceptance checks as pending, and Linux dedicated server is 'not certified'; this proves feasibility and SMR acceptance, not production quality.

Evidence:

- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Public/TeleportLogisticsContent.h>

  ```text
  class TELEPORTLOGISTICS_API UTeleportLogisticsDescriptor : public UFGBuildingDescriptor
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Private/TeleportLogisticsContent.cpp>

  ```text
  mUnlocks.Add(CreateDefaultSubobject<UTeleportLogisticsUnlock>(TEXT("TeleporterRecipes")));
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Private/TeleportLogisticsContent.cpp>

  ```text
  mSchematics.Add(UTeleportLogisticsMilestone::StaticClass());
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Private/TeleportLogisticsBuilding.cpp>

  ```text
  mHologramClass = AFGFactoryHologram::StaticClass();
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/README.md>

  ```text
  Built for Satisfactory **CL502094**, SML **3.12.0** and CSS Unreal **5.6.1**.
  ```
- <https://api.ficsit.app/v2/query?query=%7BgetModByReference(modReference%3A%22TeleportLogistics%22)%7Bname%20downloads%20last_version_date%20versions(filter%3A%7Blimit%3A1%7D)%7Bversion%20approved%20targets%7BtargetName%7D%7D%7D%7D>

  ```text
  "version":"0.5.12","approved":true,"targets":[{"targetName":"Windows"},{"targetName":"WindowsServer"}]
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/docs/validation.md>

  ```text
  These retail checks are **pending**, not silently inferred from SDK tests.
  ```

**Skeptic (source-accuracy): partially-correct**

Corrected statement: Teleport Logistics (josh-tf, MIT, v0.5.12, CL502094 / SML 3.12.0 / UE 5.6.1) implements its content chain in C++ (category, sub-category, six building descriptors, recipes, two unlock classes, two milestones, root world and game-instance modules, factory/belt/pipe/power actors, a Slate interaction widget). Only 42 script-generated mesh/material/texture .uasset files are editor content, with no Blueprints and no GameFeatureData. It is on SMR with 3 approved versions (0.5.10-0.5.12, Windows + WindowsServer only, 1,519 downloads). The milestone reward presentation depends on a vanilla BP_UnlockRecipe plus a reflection hack, its own validation.md lists the retail checks as pending, and Linux dedicated server is not certified.

Note: CONFIRMED against the tree and files at 47343693 (also main HEAD, 2026-10-02, MIT, repo created 2026-09-16): UFGBuildCategory/SubCategory, six UFGBuildingDescriptor subclasses (ItemInput, ItemOutput, FluidInput, FluidOutput, Hub, TravelHub), recipes, two UFGUnlockRecipe subclasses, two UFGSchematic milestones, a UGameWorldModule and a UGameInstanceModule both with 'bRootModule = true;', AFGFactoryHologram, UFGPowerConnectionComponent, UFGFactoryConnectionComponent, UFGPipeConnectionFactory, and UTeleportLogisticsWidget : UFGInteractWidget. No GameFeatureData and no Blueprint assets in the tree. The uplugin has no GameFeature field. README quote 'Built for Satisfactory **CL502094**, SML **3.12.0** and CSS Unreal **5.6.1**.' matches. validation.md says 'These retail checks are **pending**'. WRONG DETAILS: (1) the tree has 42 .uasset files (Icons, Models, Textures, UI), not about 35, and they are generated by scripts from OBJ/PNG under assets/. (2) api.ficsit.app returns 3 versions (0.5.12, 0.5.11, 0.5.10, all approved, targets Windows + WindowsServer, SML ^3.12.0, GameVersion >=502094), not 4, even with limit 100. downloads=1519 matches. CAVEATS the claim omits: the milestone reward UI relies on a runtime reflection hack (Content.cpp ~255-295: FindFProperty on mUnlocks/mRecipes, swapping in vanilla /Game/FactoryGame/Unlocks/BP_UnlockRecipe.BP_UnlockRecipe_C and pinning GC roots), and the widget loads the vanilla font by path. It has no LinuxServer target, so it is no evidence for the owner's Linux dedicated server.

Evidence: <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Private/TeleportLogisticsContent.cpp>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): partially-correct**

Corrected statement: Teleport Logistics (josh-tf, MIT, repo created 2026-09-16, v0.5.12, CL502094/SML 3.12.0/UE 5.6.1) proves the full native-C++ chain: build category and sub-category, 6 building descriptors, recipes, UFGUnlockRecipe subclasses, 2 UFGSchematic milestones, UGameWorldModule and UGameInstanceModule with bRootModule=true, buildable actors, AFGFactoryHologram, power/belt/pipe components and a UFGInteractWidget. Content is 42 .uasset files (meshes, materials, textures, UI), generated headlessly by scripts/import-assets.py from OBJ/PNG. 'No Blueprints' is inferred from asset prefixes and from import-assets.py never using BlueprintFactory; I did not open the assets. It has no GameFeatureData. SMR currently shows 3 approved versions (0.5.10, 0.5.11, 0.5.12), 1,519 downloads, targets Windows and WindowsServer only, with a live ai_use_disclosure of type ai_usage. Caveat: no LinuxServer target, and the owner's dedicated server is Linux. LinuxServer existence proofs for native-C++ mods on SML 3.12 are FicsitWiremod (Circuitry, 489 source files, targets LinuxServer/Windows/WindowsServer) and Bidirectional Freight Platforms (targets LinuxServer/Windows/WindowsServer). Retail evidence is thin: validation.md says retail checks are 'pending' but also 'Startup was confirmed after the 0.5.5 fix'.

Note: Core claim verified in the cloned repo at 47343693 (every quoted line exists, including mUnlocks.Add, mSchematics.Add, mHologramClass and mInteractWidgetSoftClass). Two numbers were wrong. The tree has 42 .uasset files, not ~35. api.ficsit.app returns 3 versions, not 4 (limit 50). 1,519 downloads and the 0.5.12 targets match. Freshness: the repo is about a month old with 0 issues and 0 stars, so it is feasibility evidence, not a production-quality reference.

Evidence: <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Public/TeleportLogisticsContent.h>

### c5 (verified-primary, decision-driving)

Pure-C++ buildables need Access Transformers: AFGBuildable::mInteractWidgetSoftClass and the bitfield mIsUseable sit under 'private:' (line 1023 onward of FGBuildable.h), so a C++ subclass cannot set them directly. Teleport Logistics solves this with 'Friend' transformers on AFGBuildable/AFGBuildableFactory in Config/AccessTransformers.ini, then assigns mIsUseable/mInteractWidgetSoftClass/mHologramClass in its constructor. Friend and Accessor transformers need a C++ rebuild, and that repo warns changing them can force an SDK-wide rebuild, so list every needed private member up front. Whether the Accessor form works on a bitfield is untested (Friend is the proven route).

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildable.h>

  ```text
  TSoftClassPtr< class UFGInteractWidget > mInteractWidgetSoftClass;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildable.h>

  ```text
  uint8 mIsUseable:1;
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/AccessTransformers.html>

  ```text
  Friend and Accessor transformers require rebuilding C++ to take effect.
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Config/AccessTransformers.ini>

  ```text
  Friend=(Class="AFGBuildable", FriendClass="ATeleportLogisticsBuilding")
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Private/TeleportLogisticsBuilding.cpp>

  ```text
  mInteractWidgetSoftClass = UTeleportLogisticsWidget::StaticClass();
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/docs/development.md>

  ```text
  Do not add access transformers without understanding their SDK-wide rebuild cost.
  ```

**Skeptic (source-accuracy): partially-correct**

Corrected statement: A C++ AFGBuildable subclass cannot assign the private mInteractWidgetSoftClass and mIsUseable directly. The proven route is Friend access transformers (Teleport does this), which need a C++ rebuild and an SDK-wide rebuild when changed, so list every needed private member up front. mHologramClass is public and needs none. Runtime reflection (FindFProperty) is a plausible but untested alternative for these two UPROPERTYs. Accessor on a bitfield is untested.

Note: CONFIRMED: FGBuildable.h at d2162c99 has 'private:' at line 1023; mInteractWidgetSoftClass (line 1039) and 'uint8 mIsUseable:1;' (line 1050) are both under it, both UPROPERTY. The docs say 'Friend and Accessor transformers require rebuilding C++ to take effect.' The Teleport ini has Friend=(Class="AFGBuildable", FriendClass="ATeleportLogisticsBuilding"), and the constructor sets 'mInteractWidgetSoftClass = UTeleportLogisticsWidget::StaticClass();' and mIsUseable = true. development.md says 'Do not add access transformers without understanding their SDK-wide rebuild cost.' OVERSTATED: 'need' Access Transformers. The same repo reaches UFGSchematic::mUnlocks and UFGUnlockRecipe::mRecipes through FindFProperty/ContainerPtrToValuePtr with no transformer (Content.cpp ~265-266). Reflection could in principle reach the two private UPROPERTYs, but that is untested and Friend is the only route proven for these members. mHologramClass (FGBuildable.h line 820) sits in the 'public:' block starting at line 813, so it needs no transformer. The Accessor form on a bitfield is untested.

Evidence: <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Config/AccessTransformers.ini>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): partially-correct**

Corrected statement: Pure-C++ buildables need access transformers for the private AFGBuildable members mInteractWidgetSoftClass (FGBuildable.h:1039) and the bitfield mIsUseable (:1050, under 'private:' from :1023). mHologramClass is public (after 'public:' at :813, set at :820) and needs no transformer. Three routes are in use by published mods: (1) Friend on AFGBuildable, used by Teleport Logistics and by Th3SMBuilder (Friend=(FriendClass="ATh3BuildableSM", Class="AFGBuildable"), then 'mIsUseable = true;' in the constructor, 2025-07 commit). (2) Accessor. Bidirectional Freight Platforms (approved on SMR, LinuxServer included) has `Accessor=(Class="AFGBuildable", Property="mInteractWidgetSoftClass")` and Accessors on the bitfields `uint8 mIsFullUnload : 1` and `uint8 mIsInLoadMode : 1`, and calls `self->SetmIsInLoadMode( false );` and `self->SetmInteractWidgetSoftClass( CustomWidget );` in BFPHooks.cpp. So Accessor on bitfields is demonstrably used; it is untested only for mIsUseable itself. (3) BlueprintReadWrite on both properties (mklierman HypertubeDestinations), which is BP-side and needs no C++ rebuild.

Note: Core verified at the pinned SDK: the private members, the Friend INI lines, and docs 'Friend and Accessor transformers require rebuilding C++ to take effect.' Corrections: (i) the 'untested' hedge on bitfield Accessor is superseded by the BFP evidence, which I pinned at BFP commit 09dce635 (2026-06-18). (ii) 'SDK-wide rebuild' is Teleport Logistics' own wording in docs/development.md. The official docs only say the header generator decides to rebuild 'based on the last changed date of the AccessTransformers.ini file', so touch the .ini after importing another mod's code. (iii) Open SML issue #226 notes the Accessor 'm' prefix quirk (GetmX/SetmX). No source contradicts the need for transformers.

Evidence: <https://github.com/Loupimo/BidirectionalFreightPlatforms/blob/09dce63599d40aad844df76a70b3f19bce6e3471/Config/AccessTransformers.ini>

### c6 (verified-primary, decision-driving)

Editor-side automation is available in principle: the SDK's FactoryGame.uproject (at the pinned commit) enables PythonScriptPlugin and EditorScriptingUtilities, and DefaultEngine.ini carries a PythonScriptPlugin settings section. The Unreal Python API can import meshes/textures (AssetImportTask with FbxFactory), create assets via AssetTools.create_asset, and create Blueprints with BlueprintFactory.parent_class; scripts run from the command line with -ExecutePythonScript (full editor) or -run=pythonscript (commandlet). Teleport Logistics' scripts/import-assets.py does exactly this inside the CSS editor to produce all its cookable assets from OBJ/PNG sources. Python is documented as Experimental in 5.6.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/FactoryGame.uproject>

  ```text
  "Name": "PythonScriptPlugin", "Enabled": true
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/FactoryGame.uproject>

  ```text
  "Name": "EditorScriptingUtilities", "Enabled": true
  ```
- <https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python>

  ```text
  Add the ExecutePythonScript argument to the command line
  ```
- <https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python>

  ```text
  -run=pythonscript -script=<script_file_or_code>
  ```
- <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/AssetTools?application_version=5.6>

  ```text
  Creates an asset with the specified name, path, and factory
  ```
- <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/BlueprintFactory?application_version=5.6>

  ```text
  The parent class of the created blueprint
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/scripts/import-assets.py>

  ```text
  """Run inside CSS Unreal Editor to build TeleportLogistics's cookable content.
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/scripts/import-assets.py>

  ```text
  tools.import_asset_tasks(tasks)
  ```

**Skeptic (source-accuracy): confirmed**

Note: FactoryGame.uproject at d2162c99 lists PythonScriptPlugin and EditorScriptingUtilities as Enabled true (each Name/Enabled pair is split across lines, not on one line as quoted). DefaultEngine.ini has [/Script/PythonScriptPlugin.PythonScriptPluginSettings] bDeveloperMode=True. The Epic 5.6 page says 'Learn to use this Experimental feature, but use caution when shipping with it.' It documents -ExecutePythonScript (full editor, which needs Editor Scripting Utilities) and -run=pythonscript -script=<script_file_or_code> (commandlet, headless, 'does not automatically load levels'). AssetTools create_asset reads 'Creates an asset with the specified name, path, and factory'. BlueprintFactory parent_class reads '[Read-Write] The parent class of the created blueprint'. Teleport import-assets.py docstring is 'Run inside CSS Unreal Editor to build TeleportLogistics's cookable content.' and calls AssetImportTask, FbxFactory, tools.import_asset_tasks(tasks) and create_asset (materials). Caveats: Teleport's script never uses BlueprintFactory, so Blueprint creation by Python is documented but not shown in that repo, and the script alone is not the whole pipeline (build-*-material.py and import-ui-symbols.py also contribute).

Evidence: <https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python?application_version=5.6>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): confirmed**

Note: The pinned FactoryGame.uproject has PythonScriptPlugin and EditorScriptingUtilities Enabled:true (lines 270-277), and DefaultEngine.ini:825 has the [/Script/PythonScriptPlugin.PythonScriptPluginSettings] section. The 5.6 page says 'Add the ExecutePythonScript argument to the command line' and `-run=pythonscript -script=<script_file_or_code>`. The 5.6 Python API pages show 'Creates an asset with the specified name, path, and factory' and 'The parent class of the created blueprint', and carry the header 'Unreal Python 5.6 (Experimental) documentation'. Freshness addition: Teleport Logistics' build-linux.sh and build.ps1 actually run `UnrealEditor-Cmd ... -run=pythonscript -script=import-assets.py -unattended -nosplash -nullrhi`, with AssetImportTask, FbxFactory and tools.create_asset in import-assets.py. That shows the headless commandlet route works on the CSS engine, not just in principle. No contradicting source found.

Evidence: <https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python?application_version=5.6>

### c6b (unverified)

Unverified: that the access-controlled custom engine release 5.6.1-css-83 actually ships the Python plugin binaries and Python 3.11. The SDK project enables the plugins and a third-party mod runs Python 'inside CSS Unreal Editor', which makes it very likely, but I could not inspect the engine (repo is access-controlled) and found no CSS statement either way.

Evidence:

- <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/dependencies.html>

  ```text
  Currently, Satisfactory uses Unreal Engine 5.6.1 with custom changes provided by Coffee Stain Studios.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Config/DefaultEngine.ini>

  ```text
  [/Script/PythonScriptPlugin.PythonScriptPluginSettings] bDeveloperMode=True
  ```

### c7 (verified-primary, decision-driving)

Plugin mod vs GameFeature mod: the Starter Project and docs now default to GameFeature mods since SML 3.12/game 1.2 ('99% of mods'), but a GameFeature mod needs one editor-authored FGGameFeatureData asset (created by the Alpakit wizard through AssetTools.CreateAsset; named exactly the mod reference) and must live under Mods/GameFeatures. Alpakit also ships non-game-feature 'Library (Advanced)' templates, and Alpakit marks 'GameFeature': true in the packaged uplugin only when the plugin sits under Mods/GameFeatures. A GFDA is required only to register FGUserSetting, FGMessage, FGIconLibrary and FGChildInputMappingContext (schematics/research trees/recipes can instead go through SML modules). Teleport Logistics and Circuitry repos contain no GameFeatureData asset at their content root (absence read from the git trees, not quotable) and are live on SMR. So a pure-C++ buildable mod can use a plain plugin; the GFDA/editor step appears only if sign icon libraries or ADA messages are added. SML 3.12.0 (2026-06-06, GameVersion >=491125) is the latest release.

Evidence:

- <https://docs.ficsit.app/satisfactory-modding/latest/Development/UpdatingFromSml311.html>

  ```text
  99% of mods should now include a "Game Feature Data Asset", with the exception being mods that introduce no new content nor assets, just scripting libraries.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/UpdatingFromSml311.html>

  ```text
  Create a new Data Asset asset of class FGGameFeatureData in the mod’s content root folder.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/UpdatingFromSml311.html>

  ```text
  FGUserSetting FGMessage FGIconLibrary FGChildInputMappingContext
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Source/Alpakit/Private/GameFeatureModTemplateDescription.cpp>

  ```text
  // Create a new game feature data asset
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Templates/templates.json>

  ```text
  "description": "Create a library mod that does not contain a game feature. Can contain both C++ and Blueprint code."
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Source/Alpakit.Automation/PackagePlugin.cs>

  ```text
  // Game features have to stay under Mods/GameFeatures to be picked up by the game as such
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/SML.uplugin>

  ```text
  "GameVersion": ">=491125"
  ```

**Skeptic (source-accuracy): confirmed**

Note: All quotes verified. docs: '99% of mods should now include a "Game Feature Data Asset", with the exception being mods that introduce no new content nor assets, just scripting libraries.' and 'It should be named exactly the mod's mod reference' (GameFeatureData is also supported but discouraged). The list FGUserSetting / FGMessage / FGIconLibrary / FGChildInputMappingContext is the previously auto-detected set; the GameFeatureDataAsset page also lists FGSchematic, FGResearchTree and FGRemoteCallObject as scannable, with SML modules as the alternative. Alpakit: GameFeatureModTemplateDescription.cpp '// Create a new game feature data asset'; templates.json 'Library (Advanced)' entries have no game_feature flag; PackagePlugin.cs MarkGameFeature sets "GameFeature": true only if IsGameFeatureDLC, which accepts Mods/GameFeatures or Plugins/GameFeatures. SML.uplugin has VersionName 3.12.0 and GameVersion >=491125; GitHub v3.12.0 was published 2026-06-06 and is the latest release. Nuances: the docs default is to include a GFDA, so skipping it is a deviation backed by empirical evidence only. The Circuitry repo is Scordium/FicsitWiremod, whose SMR name is 'Circuitry - Logic gates and More' (195,480 downloads, 14.1.63 approved, SML ^3.12.0, GameVersion >=502094, targets LinuxServer + Windows + WindowsServer). It has no GameFeatureData in its tree but is NOT pure C++ (about 1,439 .uasset, a .umap, 193 .cpp). Separately, SMR lists an unrelated 'Circuitry 2' (mod_reference Circuitry) with 0 downloads and no versions. FicsitWiremod is already a Redstone-like logic-gate mod, a scope fact for the owner, and it has no license file so its code cannot be reused.

Evidence: <https://docs.ficsit.app/satisfactory-modding/latest/Development/UpdatingFromSml311.html>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): partially-correct**

Corrected statement: Starter Project and docs default to GameFeature mods since SML 3.12 / game 1.2: docs say '99% of mods should now include a "Game Feature Data Asset"', with the exception of mods that 'introduce no new content nor assets'. A GFDA is an FGGameFeatureData asset in the content root named exactly the mod reference (the ExampleMod GFDA is Content/ExampleMod.uasset). Alpakit's IsGameFeatureDLC matches plugins under Plugins/GameFeatures OR Mods/GameFeatures (not only Mods/GameFeatures) and then sets 'GameFeature': true in the packaged uplugin. Alpakit also ships non-game-feature 'Library (Advanced)' templates. The GFDA is mandatory only for FGUserSetting, FGMessage, FGIconLibrary and FGChildInputMappingContext (the docs also list FGSchematic, FGResearchTree and FGRemoteCallObject as scannable alternatives to SML modules). Plain plugins with content are live on SMR for CL502094 / SML 3.12: Teleport Logistics (no Content/<ModRef>.uasset; its Content root has no .uasset files at all) and Circuitry. Circuitry's SMR reference is FicsitWiremod (source: Scordium/FicsitWiremod; 'Circuitry' is a placeholder entry named 'Circuitry 2'). It has 195,480 downloads and v14.1.63 targets LinuxServer/Windows/WindowsServer, and its Content root has only CircuitryMap.uasset and HoldDelayed.uasset. SML 3.12.0 (2026-06-06, GameVersion >=491125) is the latest release.

Note: Quotes verified in the docs source and live docs. Basis for 'plain plugin works': the repo trees, the Alpakit Library templates, and the migration page's warning against having both a 'standard' and a 'game features' copy installed. I did not inspect the packaged uplugin's GameFeature flag, because that would mean downloading mod zips, which is outside the allowed actions. Absence of a GFDA in git is therefore not proof about the shipped package. Freshness: SML `dev` is 5 commits ahead of master (2026-10-12 'Update headers to CL513885', Wwise 2025.1.6.9117.4248). The docs `Dev` branch (2026-10-07) bumps Wwise from 2023.1.14 to 2025.1.6 and does not change the engine tag. So the CL502094 / Wwise 2023.1.14 pin will drift at the next game build. The Documentation master has no commits since 2026-08-05. Because the docs say 99%, defaulting to the GameFeature template is the safer option.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Source/Alpakit.Automation/PackagePlugin.cs>

### c8 (verified-primary)

Vanilla assets can be used from C++ by path with no shipped copyright content. Official docs: reusing base-game icons/meshes by referencing their paths does not include the game's files in the mod; redistributing extracted assets needs Coffee Stain permission. Teleport Logistics references vanilla ingredient descriptors with LoadClass on /Game/FactoryGame/... paths and sets recipe producer to the vanilla BuildGun by soft path (BP_BuildGun_C); the official tutorial says the recipe's ProducedIn must be FGBuildGun. I found NO Coffee Stain published policy and no clause in the SMR Content Policy about asset reuse, so beyond the docs' wording the licence position is unverified.

Evidence:

- <https://docs.ficsit.app/satisfactory-modding/latest/Development/ReuseGameFiles.html>

  ```text
  you do not need to be concerned about shipping copyrighted files with your mod, since this process does not actually include the game’s files in your own.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/ExtractGameFiles.html>

  ```text
  redistributing these assets should not be done without permission, as they are copyrighted content of Coffee Stain Studios.
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Private/TeleportLogisticsContent.cpp>

  ```text
  return LoadClass<UFGItemDescriptor>(nullptr, Path);
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Private/TeleportLogisticsContent.cpp>

  ```text
  mProducedIn.Add(TSoftClassPtr<UObject>( FSoftObjectPath(TEXT("/Game/FactoryGame/Equipment/BuildGun/BP_BuildGun.BP_BuildGun_C"))));
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/SimpleMod/buildable.html>

  ```text
  Create a new FGRecipe asset and set ProducedIn to FGBuildGun
  ```

### c9 (verified-primary, decision-driving)

An interaction UI needs no hand-made UMG: a C++ subclass of UFGInteractWidget overrides RebuildWidget() and returns a pure Slate tree (SButton, SEditableTextBox, SComboButton, SScrollBox, FSlateColorBrush, FCoreStyle brushes, the vanilla font by path). The official tutorial instead uses a Blueprint widget (Widget_UseableBase) and the optional Widget_Window_DarkMode. Client-to-server changes (e.g. a threshold) must go through a player-owned Remote Call Object; docs say native RCOs inherit FGRemoteCallObject and are registered through UGameInstanceModule::RemoteCallObjects (works from a native module, as Teleport does). Guard widget code with IsRunningDedicatedServer(). A zero-UI alternative exists: vanilla AFGBuildableCircuitSwitch exposes SetSwitchOn(bool) as a BlueprintCallable UFUNCTION (usable as an actuator). Not verified: pixel-level look of Slate UI in retail game.

Evidence:

- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Public/TeleportLogisticsWidget.h>

  ```text
  class TELEPORTLOGISTICS_API UTeleportLogisticsWidget : public UFGInteractWidget
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Public/TeleportLogisticsWidget.h>

  ```text
  virtual TSharedRef<SWidget> RebuildWidget() override;
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Private/TeleportLogisticsWidget.cpp>

  ```text
  if (Closing || IsRunningDedicatedServer() || !IsValid(Building))
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/Multiplayer.html>

  ```text
  Your remote call objects should inherit from FGRemoteCallObject.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Public/Module/GameInstanceModule.h>

  ```text
  Mod Remote Call Objects to be registered automatically during construction phase
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/SimpleMod/machines/SimpleInteraction.html>

  ```text
  Create a new Blueprint Class asset of type Widget_UseableBase.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableCircuitSwitch.h>

  ```text
  void SetSwitchOn( bool isSwitchOn );
  ```

**Skeptic (source-accuracy): confirmed**

Note: Verified: 'class TELEPORTLOGISTICS_API UTeleportLogisticsWidget : public UFGInteractWidget', 'virtual TSharedRef<SWidget> RebuildWidget() override;', and the guard 'if (Closing || IsRunningDedicatedServer() || !IsValid(Building))'. The widget uses SButton, SEditableTextBox, SComboButton, SScrollBox, FSlateColorBrush, FCoreStyle brushes and the vanilla font at /Game/FactoryGame/Interface/Font/DescriptionText. The docs tutorial uses 'Create a new Blueprint Class asset of type Widget_UseableBase.' GameInstanceModule.h has 'Mod Remote Call Objects to be registered automatically during construction phase' (TArray<TSubclassOf<UFGRemoteCallObject>> RemoteCallObjects). The docs say 'RCOs can be registered via listing their classes in your Game Instance Module's Remote Call Objects array.' AFGBuildableCircuitSwitch is FACTORYGAME_API and SetSwitchOn(bool) is public and BlueprintCallable. Gotchas the claim omits: the documented C++ RCO needs a replicated dummy UPROPERTY plus GetLifetimeReplicatedProps to work, and RCOs must not use multicast RPCs. In-game appearance of the Slate UI remains unverified (Teleport's retail checks are pending).

Evidence: <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Public/TeleportLogisticsWidget.h>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): confirmed**

Note: Verified at 47343693: `class TELEPORTLOGISTICS_API UTeleportLogisticsWidget : public UFGInteractWidget`, `virtual TSharedRef<SWidget> RebuildWidget() override;`, and `IsRunningDedicatedServer()` in the widget. The Slate includes (SButton, SEditableTextBox, SComboButton, SScrollBox, FSlateColorBrush, FCoreStyle) are present. RemoteCallObjects.Add(...) appears in the native GameInstanceModule. Docs Multiplayer.adoc says 'Your remote call objects should inherit from `FGRemoteCallObject`.' GameInstanceModule.h has `TArray<TSubclassOf<class UFGRemoteCallObject>> RemoteCallObjects`. SimpleInteraction.adoc uses Widget_UseableBase (BP). FGBuildableCircuitSwitch.h:46-47 shows `UFUNCTION( BlueprintCallable ...) void SetSwitchOn( bool isSwitchOn );`. Omissions to add: a C++ RCO also needs a dummy replicated UPROPERTY plus a GetLifetimeReplicatedProps override, or 'it still won't work' (docs, 'C++ Example'). In the SDK, SetSwitchOn's body is an empty stub, so server authority is not verifiable from the SDK; the header only shows a replicated mIsSwitchOn with OnRep_IsSwitchOn. The docs list FGRemoteCallObject as scannable via a GFDA, but the GameInstanceModule route works too. Pixel-level look in retail remains unverified.

Evidence: <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Public/TeleportLogisticsWidget.h>

### c10 (verified-primary, decision-driving)

Publishing on SMR carries a mandatory disclosure that applies directly to an AI-written mod: the Content Policy requires an 'AI Usage Transparency' section stating the purposes of AI use in the mod or its content, explicitly including source code created by generative models; it also requires a 'Network Activity Transparency' section for any external network use. The live ficsit.app page is a JS shell, so the text was read from the smr-frontend source (commit 0b129362). C++ mods wait for automated approval (about 1 to 3 minutes per docs).

Evidence:

- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/content-policy/+page.svelte>

  ```text
  the purposes for which any AI tools have been used in the creation of the mod or its content
  ```
- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/content-policy/+page.svelte>

  ```text
  If portions of the mod's source code were directly created by generative AI models
  ```

**Skeptic (source-accuracy): confirmed**

Note: The text is in src/routes/content-policy/+page.svelte at 0b129362 (a 'fix: typo' commit on staging, ahead of main by 1) and also on main from 3429ced9 'feat: revised content policy with disclosure info' (2026-06-02). It says 'Mods are required to clearly and explicitly state, in the mod's "AI Usage Transparency" section on Ficsit.app, the purposes for which any AI tools have been used in the creation of the mod or its content.' The list includes 'If portions of the mod's source code were directly created by generative AI models'. The Network Activity Transparency section is also required for external connectivity. The page heading is 'Generative AI Transparency'. That the live site currently renders this text is not verified (the page is a JS shell; curl found no text). docs.ficsit.app/satisfactory-modding/latest/UploadToSMR.html says 'If you've uploaded a C++ mod, you'll have to wait for it to be approved by the automated approval process' and 'about 1 to 3 minutes'.

Evidence: <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/content-policy/+page.svelte>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): confirmed**

Note: Policy text verified at 0b129362, which is still the smr-frontend HEAD (2026-06-03). It states that mods must say 'the purposes for which any AI tools have been used in the creation of the mod or its content', lists 'If portions of the mod's source code were directly created by generative AI models', and has a separate Network Activity Transparency section. Live corroboration beyond the source: the api.ficsit.app Mod type exposes ai_use_disclosure{disclosure_type message} and network_use_disclosure. Teleport Logistics' live entry is type ai_usage ('Generative AI (Claude Opus 5, Sonnet 5 and GPT-6 Astra) were used to create the models and source code.') and is approved. FicsitWiremod's is no_ai_usage. So an AI-written native mod passes SMR with the disclosure. The docs UploadToSMR.adoc says C++ mods wait for automated approval: 'about 1 to 3 minutes'. The policy exempts only ADA voice lines made via the documented WaveNet approach, and uncontrollable dependencies and dev tools.

Evidence: <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/content-policy/+page.svelte>

### c11 (verified-primary)

Prior art in exactly the owner's idea: 'Circuitry - Logic gates and More' (SMR ref FicsitWiremod, Scordium/FicsitWiremod, v14.1.63 built for >=502094) offers logic gates, sensors and wiring, has 195,478 downloads, and ships Windows, WindowsServer and LinuxServer targets (so native logic mods do run on Linux dedicated servers). It is hybrid, not pure C++: about 193 .cpp, 295 .h and 1,439 .uasset files including 214 Build_* Blueprint assets, with gate logic in C++ classes that Blueprints reparent (e.g. AAND). The repo has no LICENSE file and GitHub reports licence null, so its code cannot be reused. This is a scope/novelty risk for a 'Redstone' mod, not a technical blocker.

Evidence:

- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/FicsitWiremod.uplugin>

  ```text
  "Description": "Create cool control circuits and contraptions using logic gates, sensors, and other stuff!"
  ```
- <https://github.com/Scordium/FicsitWiremod/blob/4e9fff3e3e1254c1b418bdf4d9148994f3ff0162/Source/FicsitWiremod/Public/Behaviour/Gates/Logic/AND.h>

  ```text
  class FICSITWIREMOD_API AAND : public AMultistateWiremodBuildable
  ```
- <https://api.ficsit.app/v2/query?query=%7BgetModByReference(modReference%3A%22FicsitWiremod%22)%7Bname%20downloads%20last_version_date%20versions(filter%3A%7Blimit%3A1%7D)%7Bversion%20approved%20targets%7BtargetName%7D%7D%7D%7D>

  ```text
  "downloads":195478
  ```
- <https://api.ficsit.app/v2/query?query=%7BgetModByReference(modReference%3A%22FicsitWiremod%22)%7Bname%20downloads%20last_version_date%20versions(filter%3A%7Blimit%3A1%7D)%7Bversion%20approved%20targets%7BtargetName%7D%7D%7D%7D>

  ```text
  "targets":[{"targetName":"LinuxServer"},{"targetName":"Windows"},{"targetName":"WindowsServer"}]
  ```

### c12 (secondary, decision-driving)

What can be verified on the Linux VPS: only code with no Unreal includes. Teleport Logistics keeps an 'engine-independent' core header shared with standalone tests and compiles it with plain g++ -std=c++20 -Wall -Wextra -Werror -pedantic plus ASan/UBSan, alongside Python validators for assets. UObject/AFGBuildable glue needs UHT and the engine headers (engine repo access-controlled) so it cannot be compiled or syntax-checked here; the same repo states native tests exercise SDK code and do not establish retail acceptance. Docs add that new C++ files should be created through the editor or carefully by hand because UBT must see them.

Evidence:

- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/scripts/test.sh>

  ```text
  "${CXX:-g++}" -std=c++20 -O1 -g -Wall -Wextra -Werror -pedantic
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/docs/architecture.md>

  ```text
  Engine-independent rate budgets and fair source/receiver selection; shared with standalone tests.
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/docs/development.md>

  ```text
  Native tests exercise SDK code and reconstructed game APIs; a passing report does not establish retail-game acceptance.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Cpp/setup.html>

  ```text
  you should not create new files from Visual Studio directly
  ```

**Skeptic (source-accuracy): partially-correct**

Corrected statement: On this VPS (only g++, python3, node, git; no engine, SDK, Wine or MSVC) only code with no Unreal includes can be compiled and tested, as Teleport does for its scheduler header with g++ -std=c++20 -Werror plus ASan/UBSan and Python validators. UObject/AFGBuildable glue needs UHT and the engine headers. The engine repo is private (reachable with the right GitHub grant), and Linux builds are possible in principle through the documented Wine + msvc-wine setup, but that needs a provisioned toolchain, so for now Unreal builds happen on the owner's PC.

Note: Quotes verified: test.sh has '"${CXX:-g++}" -std=c++20 -O1 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined' compiling tests/transport_test.cpp with only the Public include dir. Core/TeleportLogisticsScheduler.h includes only std headers (algorithm, cmath, cstddef, cstdint, vector). architecture.md: 'Engine-independent rate budgets and fair source/receiver selection; shared with standalone tests.' development.md: 'Native tests exercise SDK code and reconstructed game APIs; a passing report does not establish retail-game acceptance.' docs Cpp/setup: 'you should not create new files from Visual Studio directly'. Manual creation is allowed but 'not recommended as it's very easy to make mistakes', followed by regenerating project files. FGBuildable.h includes FactoryGame.h and Engine/*, so it cannot compile without the engine. OVERSTATED: the 'cannot be built here' reason. satisfactorymodding/UnrealEngine is private (GitHub API private:true, default branch 5.6.1-CSS) and this session's token has pull permission, so access control is not the blocker. The official docs have a 'Linux Modding Environment Setup' page ('mostly stable Linux development environment', wine + msvc-wine), and Teleport's development.md runs its full pipeline from a Linux host via build-linux.sh with UE_WINE_MSVC. So the limit is this VPS's provisioning, not a platform or access impossibility.

Evidence: <https://docs.ficsit.app/satisfactory-modding/latest/Development/Linux/LinuxSetup.html>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): partially-correct**

Corrected statement: Engine-independent code (no Unreal includes) can be compiled and tested on this VPS with plain g++, as Teleport Logistics does (scripts/test.sh: `-std=c++20 -O1 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined` on Core/TeleportLogisticsScheduler.h and tests/transport_test.cpp, plus Python validators). UObject/AFGBuildable glue cannot be compiled here as the VPS is currently provisioned. That is a provisioning limit, not a fundamental one. Official docs (Development/Linux/LinuxSetup) and SML's own CI (LinuxCi: 'As of SML 3.12, we build binaries for SML and the custom Unreal Engine from Linux CI runners') document a full Linux modding environment (msvc-wine, wine, the UnrealEngine-CSS-Editor-Linux.tar.zst.* archive, Wwise). Teleport Logistics' scripts/build-linux.sh runs FactoryEditor build, the Python import commandlet, and Windows/LinuxServer packaging headlessly on Linux. The 5.6.1-css-83 release's Linux editor archive is about 8.3 GB compressed (4 parts), and this VPS's gh identity can list those release assets. The real blockers are the task's no-install rule and capacity (15 GB RAM total with about 2 GB available, 300 GB free disk, 8 cores). Manual C++ file creation is allowed but 'not recommended'; the docs warning is specifically about Visual Studio creating files in a temp directory that UBT cannot see.

Note: The Teleport Logistics quotes (-Werror -pedantic, 'Engine-independent rate budgets...', 'a passing report does not establish retail-game acceptance') are verified, which keeps the repo-derived parts at 'secondary'. The docs.ficsit.app quote 'you should not create new files from Visual Studio directly' is verified in Cpp/setup.adoc:229, but the 'must be created through the editor or carefully by hand' gloss is not what the docs say. They offer the editor wizard or manual creation ('very easy to make mistakes') followed by regenerating the VS project files. The UE engine repo is access-controlled via the Epic-GitHub link flow (dependencies.adoc). Also, TL's native automation tests run only in the editor ('Automation RunTests'), so functional verification of the glue needs an engine.

Evidence: <https://docs.ficsit.app/satisfactory-modding/latest/Development/Linux/LinuxSetup.html>

### c13 (secondary)

Gotcha: native unlock classes do not render in the game's unlock UI. Two independent mods work around it by instancing stock Blueprint unlock classes at runtime: Teleport Logistics replaces its native UFGUnlockRecipe subobjects with stock BP_UnlockRecipe (loaded at INITIALIZATION, pinned with TStrongObjectPtr because native CDOs may sit in the disregard-for-GC set), and the LaserRifle mod states a custom C++ UFGUnlockInfoOnly subclass 'draws nothing'. The recipes themselves unlock fine. The exact failure mode on CL502094 for a bare native UFGUnlockRecipe was not verified (only the authors' comments).

Evidence:

- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/docs/architecture.md>

  ```text
  game-instance initialization replaces the presentation with stock `BP_UnlockRecipe` objects.
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Private/TeleportLogisticsContent.cpp>

  ```text
  Native CDOs can live in the disregard-for-GC set.
  ```
- <https://github.com/mellox/SatisfactoryLaserRifleMod/blob/a541b4e5a871d1710f589e47ecdaa000f82b6ef6/Source/LaserRifleMod/Public/LaserRifleSchematics.h>

  ```text
  the MAM card only renders rows for the game's KNOWN
  ```

### c14 (verified-primary)

Docs and shipped practice disagree on the building mesh component: the official tutorial says a placed building needs an FGColoredInstancedMeshProxy with a mesh, while Teleport Logistics (approved on SMR) builds every part with a plain UStaticMeshComponent and sets customization flags (mAllowColoring, mAllowPatterning) on the buildable. Not resolved here; test painting and abstract-instance behaviour in game before committing to one.

Evidence:

- <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/SimpleMod/buildable.html>

  ```text
  Make sure that your building has an FGColoredInstancedMeshProxy component and that a mesh is assigned to it.
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Private/TeleportLogisticsBuilding.cpp>

  ```text
  auto *Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
  ```

### c15 (verified-primary)

Icons do not strictly need editor-imported textures: SML ships FImageLoadingUtil::LoadImageFromFile/LoadImageFromByteArray, which decodes PNG/JPG into a transient, root-set UTexture2D at runtime, and the Alpakit C++ template stages a raw Resources directory (+AdditionalNonUSFDirectories=Resources). Whether such a transient texture is acceptable for UFGItemDescriptor::mSmallIcon / build-menu icons on the client (and harmless on a dedicated server) is untested. Teleport Logistics instead imports its icons as Texture2D assets.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Public/Util/ImageLoadingUtil.h>

  ```text
  /** Loads image from file at the given path */
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Templates/CPPAndBlueprintBlank/Config/PluginSettings.ini>

  ```text
  +AdditionalNonUSFDirectories=Resources
  ```

### c16 (secondary)

Constructor asset-loading hazard: because the engine runs a constructor for each spawned building (not only the CDO), a blocking LoadObject in the constructor can stall the game thread; Teleport Logistics measured a 2.3 s single-frame flush and now caches each path once (TStrongObjectPtr) so only the CDO's construction loads. Treat 'set defaults in the constructor' as safe for plain data, but cache any asset resolution.

Evidence:

- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Public/TeleportLogisticsAsset.h>

  ```text
  FactoryGame.log caught one such flush holding a single
  ```

### c17 (verified-primary)

Hologram: the stock native AFGBuildableHologram (docs) or AFGFactoryHologram (Teleport) is enough for ordinary placement; a custom hologram needs C++ (docs state so) but not Blueprint assets. Forgetting to set mHologramClass on the buildable crashes the build gun on selection, so it must be set in the constructor.

Evidence:

- <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/SimpleMod/buildable.html>

  ```text
  Set M Hologram Class to FGBuildableHologram, which will cause the FGBuildable to automatically find the hologram by itself.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/SimpleMod/buildable.html>

  ```text
  You forgot to assign a hologram class in the FGBuildable - the default is None, which leads to this crash.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/BuildableHolograms.html>

  ```text
  Writing your own Hologram logic requires writing some C++ code.
  ```

### c18 (secondary)

Assets a custom-mesh buildable cannot avoid (as authored data): a UStaticMesh, and for mesh texturing either the vanilla factory material MI_Factory_Base_01 referenced by path (the mesh's UVs must map to its atlas; the docs say you need to do that UV work yourself for your own models) or a custom material, plus optional icon textures. Teleport Logistics generates its meshes with Blender scripts (OBJ), binds native factory materials and falls back to /Engine/BasicShapes primitives before import. Whether engine primitives are cooked into a packaged client/LinuxServer when no mesh is imported is unverified, as is the retail look of such a fallback.

Evidence:

- <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/SimpleMod/buildable.html>

  ```text
  The provided example mesh already provides a proper UV map for said material, so you will not need to edit the UVs of the provided mesh.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/SimpleMod/buildable.html>

  ```text
  You will need to do so for your own models.
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/assets/PROVENANCE.md>

  ```text
  pre-import editor startup falls back to engine basic shapes
  ```
- <https://github.com/josh-tf/teleport-logistics/blob/47343693b5175d3684c709ce3838789e92916047/TeleportLogistics/Source/TeleportLogistics/Private/TeleportLogisticsBuilding.cpp>

  ```text
  Part(TEXT("MainMesh"), TEXT("/Engine/BasicShapes/Cube.Cube")
  ```

## Catalog

| Name | Reference | Licence | Last updated | Relevance |
| --- | --- | --- | --- | --- |
| [SatisfactoryModLoader (SDK / Starter Project) at owner's pin](https://github.com/satisfactorymodding/SatisfactoryModLoader/tree/d2162c999bbaa9650594ee9ad6e6e06190306009) | d2162c999bbaa9650594ee9ad6e6e06190306009 | GPL-3.0 | 2026-08-11 | Primary source for every class/member name in this report; read-only reference. |
| [Teleport Logistics (josh-tf/teleport-logistics)](https://github.com/josh-tf/teleport-logistics) | 47343693b5175d3684c709ce3838789e92916047 (v0.5.12) | MIT | 2026-10-02 | Closest existence proof for a native-C++ buildable mod with minimal editor assets; MIT so code patterns can be studied/adapted with attribution. Retail validation pending per its own docs. |
| [Circuitry - Logic gates and More (Scordium/FicsitWiremod)](https://github.com/Scordium/FicsitWiremod) | 4e9fff3e3e1254c1b418bdf4d9148994f3ff0162 | none (GitHub licence null, no LICENSE file in tree) | 2026-09-26 | Direct prior art for a Redstone-like mod and evidence that logic mods run on LinuxServer; no licence means do not copy code. |
| [SatisfactoryLaserRifleMod (mellox)](https://github.com/mellox/SatisfactoryLaserRifleMod) | a541b4e5a871d1710f589e47ecdaa000f82b6ef6 | none (GitHub licence null) | 2026-07-14 | Second independent source for native schematics and the stock-BP unlock UI workaround. |
| [ContentLib (Nogg-aholic)](https://github.com/Nogg-aholic/ContentLib) | 3dbf359258626c7568097b4ba3a48f20427c7a5c (v1.10.0) | none (GitHub licence null) | 2026-06-21 | Alternative for data-only content; cannot create new building actors, so not a substitute for native buildable classes. |
| [AutoSupport (kalbert312/satisfactory-mods)](https://github.com/kalbert312/satisfactory-mods/tree/main/AutoSupport) | default branch main (repo pushed 2026-08-16) | GPL-3.0 | 2026-08-16 | Counter-example: same buildable chain done with Blueprint assets and UMG. |
| [Satisfactory Modding documentation (latest)](https://docs.ficsit.app/satisfactory-modding/latest/) | latest | not stated | 2026-10-13 (read date; site shows latest = SML 3.12) | Primary documentation; some pages lag the headers (e.g. registry described as game-instance subsystem). |
| [smr-frontend content policy](https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/content-policy/+page.svelte) | 0b1293626be45e871136c83f3c055567d0833a93 | not stated | 2026-10-13 (read date) | Publishing requirements for an AI-assisted mod. |
| [Unreal Python / Editor scripting docs (5.6)](https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python) | application_version=5.6 | Epic documentation | 2026-10-13 (read date) | Official basis for scripting asset import/creation on the owner's PC. |

## Name checks (SMR references, 2026-10-13)

| Candidate | Exists in SMR | Similar existing | Note |
| --- | --- | --- | --- |
| Redstone | no | FicsitWiremod (Circuitry - Logic gates and More) | Exact modReference lookup on api.ficsit.app returned 'mod not found' on 2026-10-13. The word is Minecraft/Mojang branding; no legal assessment was made. |
| FicsitRedstone | no |  | Exact modReference lookup returned 'mod not found'. |
| Ficsitstone | no |  | Exact modReference lookup returned 'mod not found'. |
| FicsitLogic | no |  | Exact modReference lookup returned 'mod not found'. |
| LogicGates | no |  | Exact modReference lookup returned 'mod not found'. Fuzzy search lists many unrelated mods, so descriptive names may still collide on the page title. |
| SignalLogic | no |  | Exact modReference lookup returned 'mod not found'. |
| Wiremod | no | FicsitWiremod | Exact modReference lookup returned 'mod not found', but the active mod with the same idea uses reference FicsitWiremod. |
| Circuitry | yes | FicsitWiremod | Reference exists as 'Circuitry 2' with 0 downloads, no versions and description 'Mod reference hoarding'; the real product is FicsitWiremod. |
| FicsitWiremod | yes | FicsitWiremod | Active mod 'Circuitry - Logic gates and More', 195,478 downloads, last version 14.1.63 (2026-09-26). |

## Open questions

- Does the access-controlled CSS engine release 5.6.1-css-83 ship PythonScriptPlugin and Python 3.11 binaries? (SDK uproject enables them; no direct check possible.)
- Do native UFGSchematic/UFGUnlockRecipe classes show a correct HUB/MAM reward UI on CL502094 without swapping in stock BP_UnlockRecipe? Only two mods' comments were found, no CSS/SML statement.
- Is a plain UStaticMeshComponent enough for Customizer painting and abstract-instance rendering, or is FGColoredInstancedMeshProxy mandatory (docs vs Teleport Logistics)?
- Are /Engine/BasicShapes meshes available in packaged client and LinuxServer builds when a mod imports no mesh? Teleport only uses them as a pre-import fallback.
- Can SML's runtime FImageLoadingUtil textures be used for UFGItemDescriptor::mSmallIcon / build-menu icons, and is the ImageWrapper module available on a dedicated server?
- Does the Accessor access transformer work on the bitfield AFGBuildable::mIsUseable? (Friend is proven; Accessor is untested.)
- Does a native-only buildable mod package and run cleanly for LinuxServer with the owner's toolchain? Teleport Logistics certifies Windows only; Circuitry ships LinuxServer builds but is hybrid.
- Licensing: the SDK repo is GPL-3.0 with no visible exception, and I found no Coffee Stain policy on asset reuse; whether a mod linking SML/headers must be GPL, and any use of Minecraft's 'Redstone' name, was not assessed.
- My search for native-content mods used gh search code and hit HTTP 403 rate limits partway, so the list is a lower bound. I found no buildable mod with zero .uasset files (Teleport Logistics still ships mesh/texture/material assets).
- Teleport Logistics' retail acceptance checklist is marked pending by its author; how well its pure-C++ approach holds up in long multiplayer sessions is unknown.

## Recommendations

- Architecture: put all signal/gate/latch/clock logic in an engine-independent C++ core (plain headers, no Unreal includes) tested on the Linux VPS with g++ -std=c++20 -Wall -Wextra -Werror -pedantic and ASan/UBSan, as Teleport Logistics does. Keep the UObject/AFGBuildable layer as thin adapters that can only be compiled on the owner's PC.
- Use a plain C++ plugin under Mods/<ModRef> (Alpakit 'C++ & Blueprint Library (Advanced)' template, no game feature) with a native UGameWorldModule subclass (bRootModule=true, mSchematics) and a native UGameInstanceModule; add a GameFeatureData asset only if sign icon libraries, ADA messages, user settings or input contexts become necessary.
- Write the descriptor/recipe/schematic/unlock/category/buildable/hologram classes as native UCLASSes whose constructors set the defaults. Use AFGBuildableHologram or AFGFactoryHologram and set mHologramClass in every buildable constructor. Reference vanilla ingredients and BP_BuildGun by /Game/FactoryGame path (LoadClass / soft path) and cache every asset resolution (constructor hitch).
- Decide all AccessTransformers.ini Friend entries before the first build (at least AFGBuildable for mIsUseable and mInteractWidgetSoftClass, plus any private member your gates touch), because each change forces a long SDK-wide rebuild on the owner's PC.
- Plan the first vertical slice as one native buildable (for example a signal lamp or lever) + one schematic + recipe + Slate interaction widget + one RCO, with zero imported assets (engine primitives or vanilla meshes by path). Have the owner build and test it in game first: it settles open questions on unlock UI, mesh component, primitives and Linux packaging before the full gate set is written.
- Editor-only steps on the owner's PC for a buildable with a custom mesh: (1) one-time Starter Project at the pinned commit (already done); (2) build the Development Editor target whenever headers, UCLASS layout or transformers change; (3) run one Python script (-ExecutePythonScript or commandlet) that imports OBJ/FBX meshes as UStaticMesh via AssetImportTask+FbxFactory and PNGs as Texture2D, with UVs authored to the vanilla factory material atlas (MI_Factory_Base_01); (4) Alpakit package for Windows + WindowsServer + LinuxServer (or UAT PackagePlugin); (5) in-game smoke test. No Blueprint, UMG, data-table or GameFeatureData authoring is required in this route.
- For UI avoid UMG: subclass UFGInteractWidget, override RebuildWidget() with a Slate tree, send changes through a native FGRemoteCallObject, guard with IsRunningDedicatedServer(), and skip the widget path entirely for gates that can be toggled or cycled with E. Consider the vanilla power switch's SetSwitchOn as the first actuator.
- Handle schematic reward UI deliberately: either accept an incomplete reward/MAM card for native unlocks, or copy Teleport Logistics' approach (swap in stock BP_UnlockRecipe at INITIALIZATION and pin it with TStrongObjectPtr) and test it on the owner's save.
- Before publishing: prepare the SMR 'AI Usage Transparency' and 'Network Activity Transparency' text (source code is AI-generated; no network use), and decide the scope relative to Circuitry (195k downloads, no licence, so no code reuse) so the mod has a clear differentiator.
- Do not treat Teleport Logistics as proven quality (its retail checks are pending) but as a structural template under MIT; re-verify every API name against the pinned headers before use and keep a written list of unverified items for the in-game test round.

