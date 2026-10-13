# Versions and toolchain

_Generated from `data/versions.json` by `tools/render_research.py`; do not edit by hand. Research run of 2026-10-13. Claims are the researcher's; verdicts are from independent skeptics told to refute them. Wording is kept verbatim (English)._

## Summary

As of 2026-10-13 the owner's pin is one game patch behind. SML 3.12.0 (2026-06-06, tag 1a7d2ca3) is still the newest release on GitHub and ficsit.app (GameVersion >=491125); no 3.12.1/3.13 exists, and SML.uplugin is identical on master and dev. Toolchain is unchanged: custom UE 5.6.1 (latest release 5.6.1-css-83), Visual Studio 2022 only (not 2026) with MSVC v14.38-17.8, .NET 8 runtime plus .NET Framework 4.8.1 SDK, clang v25 (clang-18.1.0) for LinuxServer.

The Documentation repo (default branch master, not main) splits on Wwise: master says 2023.1.14.8770; Dev (changed 2026-10-07) says 2025.1.6.9117. The SML master HEAD d2162c99 is the owner's pin: CL502094 (v1.2.4.0, 11 Aug) with Wwise 2023.1.14. The SML dev HEAD 8eebc9e6 is CL513885 (v1.2.4.1, 12 Oct) with Wwise 2025.1.6. Docs tell newcomers to clone whichever of dev/master was pushed most recently, which is dev.

Game: v1.2.4.1 (Build 513885, wiki = secondary) shipped 2026-10-12. A Steam mirror shows public and experimental share one build ID per app (client 25782416, server 25782485), so CL502094 is no longer newest and the owner's server build 24656085 is stale.

Flags for a new mod:
1. GameVersion trap: Alpakit's "update Game Version" writes >=currentVersion.txt, and SML refuses the mod when the running CL is lower. A dev-built mod auto-set to >=513885 will not load on a server still at 502094.
2. Content mods need a Game Feature Data Asset, a binary .uasset made in the editor.
3. Release zips need Windows/WindowsServer/LinuxServer folders with byte-identical .uplugin files. LinuxServer is mandatory for the owner's server.
4. Use TObjectPtr and full include paths (BuildSettingsVersion V5).
5. ficsit.app requires AI-usage and network disclosures before any upload.
6. Server installs of Game Feature mods need ficsit-cli >=0.7.0.

Unresolved: what CL503514 is, and whether any 1.2.4.1 breakage exists beyond one open session-settings bug (#429).

## Claims at a glance

| Id | Confidence | Decision-driving | Skeptic verdicts |
| --- | --- | --- | --- |
| C1 | verified-primary | yes | confirmed, confirmed |
| C2 | verified-primary | no | not verified (not decision-driving) |
| C3 | verified-primary | yes | confirmed, confirmed |
| C4 | verified-primary | no | not verified (not decision-driving) |
| C5 | secondary | yes | confirmed, confirmed |
| C6 | secondary | no | not verified (not decision-driving) |
| C7 | verified-primary | yes | partially-correct, partially-correct |
| C8 | verified-primary | yes | partially-correct, confirmed |
| C9 | verified-primary | yes | confirmed, confirmed |
| C10 | verified-primary | no | not verified (not decision-driving) |
| C11 | verified-primary | yes | confirmed, confirmed |
| C12 | verified-primary | yes | partially-correct, partially-correct |
| C13 | secondary | no | not verified (not decision-driving) |
| C14 | verified-primary | no | not verified (not decision-driving) |
| C15 | verified-primary | no | not verified (not decision-driving) |
| C16 | verified-primary | no | not verified (not decision-driving) |
| C17 | verified-primary | no | not verified (not decision-driving) |
| C18 | verified-primary | no | not verified (not decision-driving) |
| C19 | verified-primary | no | not verified (not decision-driving) |
| C20 | unverified | no | not verified (not decision-driving) |
| C21 | secondary | no | not verified (not decision-driving) |

## Claims in detail

### C1 (verified-primary, decision-driving)

The Documentation repo's default branch is master (not main). Its software-versions.adoc contains only the UE version (5.6.1), the clang cross-compile toolchain (v25 clang-18.1.0-based) and Wwise versions; MSVC, .NET and SML versions are not in that file (they are in dependencies.adoc and the per-version Updating guides). master (d35cc911, 2026-08-05) pins Wwise 2023.1.14.8770 and docs.ficsit.app/latest renders that; Dev (8175a04f, 2026-10-07, commit da2eb842 'Update wwise versions') pins Wwise 2025.1.6.9117 and docs-dev.ficsit.app renders that.

Evidence:

- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/_includes/software-versions.adoc>

  ```text
  :engine-version: 5.6.1
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/_includes/software-versions.adoc>

  ```text
  :wwise-version-full: 2023.1.14.8770
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/8175a04f8c194160b3a145d118a7fbc8f243fa99/modules/ROOT/pages/Development/_includes/software-versions.adoc>

  ```text
  :wwise-version-full: 2025.1.6.9117
  ```
- <https://github.com/satisfactorymodding/Documentation/commit/da2eb842bb6ff1573cc8a8c37ba95259e12dddc6>

  ```text
  Update wwise versions
  ```

**Skeptic (source-accuracy): confirmed**

Note: I re-opened both commits. master d35cc911 (2026-08-05) has engine 5.6.1, clang v25 clang-18.1.0-based, Wwise 2023.1.14.8770. Dev 8175a04f (2026-10-07) has Wwise 2025.1.6.9117. Commit da2eb842 'Update wwise versions' touches only that file. GitHub reports default_branch=master. docs.ficsit.app and docs-dev.ficsit.app render those two Wwise values. The working URLs are /satisfactory-modding/latest/...; /satisfactory/latest 404s. The branch is spelled 'Dev' with a capital D. MSVC v14.38 and .NET are hard-coded in dependencies.adoc lines 131-133, but no SML version is in that file; it appears in UpdatingFromSml311.adoc. Side note, unverified: the pin's WwisePatches are CL480605, and SML dev added Wwise-2025 patches for CL503514. Steam's 1.2.4.1 post says the game's Wwise moved to 2025.1.6, but I could not find which game CL first shipped it.

Evidence: <https://docs-dev.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/dependencies.html>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): confirmed**

Note: Re-checked by API: Documentation default_branch is master; the other long-lived branch is spelled 'Dev' (capital D); Dev is 4 commits ahead and 0 behind master. software-versions.adoc at d35cc911 and 8175a04f matches the claim exactly. Rendered docs match: docs.ficsit.app dependencies page says 'Select 2023.1 ... 2023.1.14.8770' and docs-dev.ficsit.app says 'Select version 2025.1.6.9117 from the "Version" dropdown'. New cross-check: SML CI at the owner's pin d2162c99 uses `--sdk-version "2023.1.14.8770"`, but SML dev 8eebc9e6 uses `--sdk-version "2025.1.6.9117"` (build.yml L176) and its WwisePatches switched to '04-CSS-CL503514.patch'. So the Wwise split follows the SML branch: staying on the pin means Wwise 2023.1.14; moving to SML dev means reinstalling Wwise 2025.1.6. The live docs (master) are internally inconsistent: repo-versions.adoc sets ':suggested-branch: dev' yet the live page still tells you to install Wwise 2023.1.14.8770. Follow docs-dev if you adopt dev. Steam 1.2.4.1 notes also say: 'We updated our internal Wwise version to 2025.1.6'.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/8eebc9e659f0b2ff4392d2f8cf6e2cff60c8ea49/.github/workflows/build.yml>

### C2 (verified-primary)

Documented toolchain for the current game: Visual Studio 2022 (not 2026) with MSVC v143 v14.38-17.8 (marked Out of Support but required), .NET 8.0 Runtime, .NET Framework 4.8.1 SDK, custom UE 5.6.1, and clang cross-compile toolchain 'v25 clang-18.1.0-based' for LinuxServer. SML's own CI uses MSVC 17.8 (--msvc-version 17.8). SML 3.12 moved Access Transformers to .NET 8 / Harmony 2.4.2.

Evidence:

- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/dependencies.adoc>

  ```text
  `MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.38-17.8) (Out of Support)`
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/dependencies.adoc>

  ```text
  `.NET 8.0 Runtime (Long Term Support)`
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/dependencies.adoc>

  ```text
  not the latest release version 2026.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/.github/workflows/build.yml>

  ```text
  --msvc-version 17.8
  ```

### C3 (verified-primary, decision-driving)

SML 3.12.0 (published 2026-06-06, tag v3.12.0 = commit 1a7d2ca3) is the latest SML release: GitHub 'latest' and the ficsit.app SML version list both show it, with satisfactory_version 491125 and targets Windows, WindowsServer, LinuxServer. The previous release is 3.11.3 (2025-08-09). There is no pre-release and no 3.12.x/3.13.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/releases/tag/v3.12.0>

  ```text
  Satisfactory 1.2 (CL#491125) support
  ```
- <https://api.ficsit.app/v2/query?query=query%7BgetSMLVersions(filter%3A%7Blimit%3A10%2Corder_by%3Acreated_at%2Corder%3Adesc%7D)%7Bcount%20sml_versions%7Bversion%20satisfactory_version%20date%7D%7D%7D>

  ```text
  "version":"3.12.0","satisfactory_version":491125
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/8eebc9e659f0b2ff4392d2f8cf6e2cff60c8ea49/Mods/SML/SML.uplugin>

  ```text
  "SemVersion": "3.12.0",
  ```

**Skeptic (source-accuracy): confirmed**

Note: Release v3.12.0 was published 2026-06-06T17:09:42Z, is not a prerelease, and is the GitHub 'latest'. Its tag is 1a7d2ca3 'Bump version'. The next release down is v3.11.3 (2025-08-09). The release body says 'Satisfactory 1.2 (CL#491125) support'. The ficsit.app query returns 3.12.0 first, with satisfactory_version 491125, stability 'release' and the Windows, WindowsServer and LinuxServer targets. SML.uplugin has SemVersion 3.12.0 at the pin, at 8eebc9e6 and at the tag. The repo has no tag newer than v3.12.0. Cosmetic: the claim's FRM/API quotes use '>=', but the raw API bytes encode it as >.

Evidence: <https://api.github.com/repos/satisfactorymodding/SatisfactoryModLoader/releases/latest>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): confirmed**

Note: Tried to refute with newer data and found none. GitHub releases list: v3.12.0 (2026-06-06T17:09:42Z, target master, prerelease=false, draft=false) is first, then v3.11.3 (2025-08-09). The tag v3.12.0 resolves to 1a7d2ca3. Master d2162c99 and dev 8eebc9e6 both still carry `"SemVersion": "3.12.0",` and `"GameVersion": ">=491125",` in SML.uplugin, so there is no unreleased version bump on either branch, even after the CL513885 header commit. ficsit.app getSMLVersions (ordered by created_at desc) returns 3.12.0 first with satisfactory_version 491125, stability 'release', and targets Windows, WindowsServer and LinuxServer; the next entry is 3.11.3. No 3.12.x or 3.13 exists. Side note: docs UpdatingFromSml311.adoc still carries a stale banner, 'SML 3.12 and 1.2 support have not been released yet!', which contradicts the release. Do not read it as evidence that SML is unreleased.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/8eebc9e659f0b2ff4392d2f8cf6e2cff60c8ea49/Mods/SML/SML.uplugin>

### C4 (verified-primary)

Since the 3.12.0 release there are no new SML API changes. master has 4 extra commits (docs process, 228 material placeholder stubs, a spatial-loading fix for level-placed resource nodes, and 'Update headers to CL502094' on 2026-08-11). dev has 5 more (Wwise 2025.1.6 patches for 'CL503514', bulk-data Wwise assets, a Linux-server Wwise packaging fix, and 'Update headers to CL513885' on 2026-10-12). SML.uplugin on master and dev still says SemVersion 3.12.0 and GameVersion >=491125. The master docs page UpdatingFromSml311 still carries a stale banner that 3.12/1.2 are unreleased. The documented breaking changes for new mods are those of 3.12 itself (see C8, C16).

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/commit/8eebc9e659f0b2ff4392d2f8cf6e2cff60c8ea49>

  ```text
  Update headers to CL513885
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/commit/77c1847bb8e51ce37856107642c6d736c0706443>

  ```text
  Update wwise patches for CL503514 (wwise 2025.1.6.9117.4248)
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/8eebc9e659f0b2ff4392d2f8cf6e2cff60c8ea49/Mods/SML/SML.uplugin>

  ```text
  "GameVersion": ">=491125",
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/UpdatingFromSml311.adoc>

  ```text
  SML 3.12 and 1.2 support have not been released yet!
  ```

### C5 (secondary, decision-driving)

The newest public game version is v1.2.4.1, released 2026-10-12 (Steam community announcement 14:39 UTC); the Satisfactory wiki maps it to Build 513885 and v1.2.4.0 (released 2026-08-11) to Build 502094. The SML team's own dev branch updated headers to CL513885 the same day. So CL502094 is no longer the newest public CL. Version-to-build mapping rests on the wiki (secondary) plus the SML commit; the Steam post itself gives only the version string.

Evidence:

- <https://steamstore-a.akamaihd.net/news/externalpost/steam_community_announcements/1846018067941164>

  ```text
  1.2 Update Fixes v1.2.4.1
  ```
- <https://satisfactory.wiki.gg/wiki/Patch_1.2.4.1>

  ```text
  Build 513885
  ```
- <https://satisfactory.wiki.gg/wiki/Patch_1.2.4.0>

  ```text
  Build 502094
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/8eebc9e659f0b2ff4392d2f8cf6e2cff60c8ea49/Source/FactoryGame/currentVersion.txt>

  ```text
  513885
  ```

**Skeptic (source-accuracy): confirmed**

Note: The Steam news API (primary) lists 'Update Fixes v1.2.4.1' at 2026-10-12T14:39:03Z and 'v1.2.4.0' at 2026-08-11T15:01Z. The post body gives only the version string, no build number. The wiki wikitext says stable=1, build=513885, date=2026-10-12 for 1.2.4.1, and build=502094, 2026-08-11 for 1.2.4.0. No newer patch page exists on the wiki. SML dev 8eebc9e6 'Update headers to CL513885' is dated 2026-10-12T17:44:42Z, and currentVersion.txt there is 513885. The version-to-build mapping is still secondary: wiki plus the SML commit.

Evidence: <https://api.steampowered.com/ISteamNews/GetNewsForApp/v0002/?appid=526870&count=8&maxlength=600&format=json>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): confirmed**

Note: Confirmed, still secondary for the CL mapping. The akamai URL only 302-redirects to steamcommunity, so cite the Steam RSS feed: item '1.2 Update Fixes v1.2.4.1' has pubDate 'Mon, 12 Oct 2026 14:39:03 +0000'. The v1.2.4.0 item is 'Tue, 11 Aug 2026 15:01:13 +0000', and no newer item exists. Wiki quote: 'Patch Notes: v1.2.4.1 – Build 513885. This patch was released on 12 October 2026.' SML dev commit 8eebc9e6 'Update headers to CL513885' is dated 2026-10-12T17:44:42Z. KEY FRESHNESS FINDING (third-party mirror api.steamcmd.net/v1/info/<app>, so secondary): both public branches moved at 2026-10-12T14:36 UTC, three minutes before the announcement. Dedicated server app 1690800 public buildid is 25782485 (client 526870 is 25782416), and experimental equals public. The owner's recorded server build 24656085 is therefore already stale, and CL502094 is a transient state. Nothing newer than 513885 turned up. The 1.2.4.1 notes also say 'Re-enabled Audio Routing so Metasound will work by default again' and mention the Wwise 2025.1.6 update.

Evidence: <https://store.steampowered.com/feeds/news/app/526870/?count=10&feed=steam_community_announcements>

### C6 (secondary)

Steam branch data (third-party mirror of Steam appinfo, fetched 2026-10-13): for the game client app 526870 and the Dedicated Server app 1690800 the 'public' and 'experimental' branches carry the same build ID, both set live on 2026-10-12 around 14:35-14:36 UTC (client 25782416, server 25782485). The modding docs also say stable and experimental are the same game version. The owner's build 24656085 matches neither current ID, so it is an older build. I did not map 24656085 to either app or to a CL.

Evidence:

- <https://api.steamcmd.net/v1/info/526870>

  ```text
  "public": {"buildid": "25782416", "timebuildupdated": "1791389698", "timeupdated": "1791815788"}
  ```
- <https://api.steamcmd.net/v1/info/1690800>

  ```text
  "public": {"buildid": "25782485", "timebuildupdated": "1791389899", "timeupdated": "1791815784"}
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/_includes/current-branch-header.adoc>

  ```text
  Stable (Release) and Experimental are currently both the same game version.
  ```

### C7 (verified-primary, decision-driving)

GameVersion trap. SML compares the running game's changelist (FEngineVersion::Current().GetChangelist()) against the plugin's GameVersion range and rejects the plugin if it does not match. Alpakit reads Source/FactoryGame/currentVersion.txt and offers a button that rewrites GameVersion to >= that number. A mod built on SML dev (currentVersion 513885) and 'fixed' with that button gets >=513885 and will not load on a server still at CL 502094. A mod built on master gets >=502094, which loads on 513885, but see C14 on recompiling. Existing practice (SML, ExampleMod, FRM 1.5.3) keeps the floor at >=491125.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/ModLoading/ModLoadingLibrary.cpp#L265>

  ```text
  if (CurrentChangelist != 0 && !PluginDescriptorMetadata.GameVersion.Matches(FVersion(CurrentChangelist, 0, 0))) {
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Source/Alpakit/Private/Alpakit.cpp#L164>

  ```text
  FString CurrentVersionFile = FPaths::ProjectDir() / TEXT("Source") / TEXT("FactoryGame") / TEXT("currentVersion.txt");
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Source/Alpakit/Private/AlpakitEditModDialog.cpp#L96>

  ```text
  Click to update Game Version to {0}
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/ReleaseMod.adoc>

  ```text
  SML checks this field at runtime and will prevent the game from launching if the game version does not match this range.
  ```
- <https://api.ficsit.app/v2/query?query=query%7BgetModByReference(modReference%3A%22FicsitRemoteMonitoring%22)%7Bversions(filter%3A%7Blimit%3A1%7D)%7Bversion%20game_version%20sml_version%7D%7D%7D>

  ```text
  "version":"1.5.3","game_version":">=491125","sml_version":"^3.12.0"
  ```

**Skeptic (source-accuracy): partially-correct**

Corrected statement: SML compares FEngineVersion::Current().GetChangelist() against the plugin's GameVersion range. ENFORCE_PLUGIN_DEPENDENCY_VERSIONS is defined as !WITH_EDITOR (ModLoadingLibrary.cpp line 15). Outside the editor, a mismatch is not a per-plugin rejection: it triggers UE_LOG Fatal 'Found mismatched dependencies versions in the environment. Loading cannot continue', which aborts the whole client or server. Alpakit reads currentVersion.txt, and its button merges a >= currentVersion comparator into the range (so >=491125 becomes >=513885), which on a CL502094 server aborts startup and takes the other mods down with it.

Note: Every code quote is verbatim at the pin and on dev: ModLoadingLibrary.cpp L265, Alpakit.cpp L164, AlpakitEditModDialog.cpp L96 and the ReleaseMod.adoc line 168. The ReleaseMod.adoc line says 'prevent the game from launching', which fits the Fatal. SemVersion.cpp ToString merges and-ranges to their lower and upper bounds, so the button's added comparator makes the effective floor 513885. SML.uplugin, ExampleMod.uplugin (Mods/GameFeatures/ExampleMod) and FRM 1.5.3 on the API all have floor >=491125. Only the 'rejects the plugin' wording was wrong. Cross-claim: the pin (master, CL502094) matches the owner's server. Following 'most recently pushed' leads to dev, CL513885 and Wwise 2025.1.6.9117.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/ModLoading/ModLoadingLibrary.cpp>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): partially-correct**

Corrected statement: SML compares FEngineVersion::Current().GetChangelist() with the plugin's GameVersion range. In non-editor builds a mismatch is FATAL for the whole game or server, not just that plugin: it logs 'Loading cannot continue', so one mod with a too-high floor stops a dedicated server from starting. Alpakit's 'Click to update Game Version' button ANDs a '>= currentVersion.txt' comparator onto the existing range, so an SDK on CL513885 raises the floor to 513885. The claim that existing practice keeps the floor at >=491125 is not representative. Of 854 ficsit.app versions uploaded since 2026-08-11, 605 use >=502094, 238 use >=491125 (SML, ExampleMod, FRM 1.5.3), and at least one (Cable Choices Plus 1.1.9, 2026-10-12) already uses >=513885. The premise of a server still at 502094 is transient, because the server public branch moved to a newer build on 2026-10-12.

Note: The check quote at L265 is correct at the pin. Contradicting detail from the same file: L15 `#define ENFORCE_PLUGIN_DEPENDENCY_VERSIONS !WITH_EDITOR` and L212-215 `UE_LOG(LogSatisfactoryModLoader, Fatal, TEXT("Found mismatched dependencies versions in the environment. Loading cannot continue: \n%s"), *ErrorList);`. So the failure is a crash, not a skipped mod. Alpakit at the pin (AlpakitEditModDialog.cpp L183-185) uses `ComparatorCollection.Comparators.Add(FVersionComparator(EVersionComparisonOp::GREATER_EQUALS, GameVersion));` and falls back to a min-version range only if the result does not match. Freshness data comes from ficsit.app api getVersions (12 pages x100, 1200 rows, 2026-07-10..2026-10-13; a measurement I made). Since 2026-08-11 15:00Z the floors are: >=502094 605, >=491125 238, >=495413 7, >=365306 2, >=410840 1, >=513885 1. Real example: api.ficsit.app getModByReference AB_CableMod gives `"version":"1.1.9","game_version":">=513885","created_at":"2026-10-12T20:11:39Z"` ('Cable Choices Plus'); it is 1.1.8 at >=502094 and 1.1.7 at >=491125. If the owner's server lists that mod, SMM should pick a compatible version. A manual install of 1.1.9 on a server at CL502094 would crash it. The trap itself is real, and the sound policy is to keep the floor at >=491125 or >=502094, never the SDK's current CL, unless 513885 is truly required.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/ModLoading/ModLoadingLibrary.cpp>

### C8 (verified-primary, decision-driving)

Since SML 3.12 / game 1.2, '99% of mods' must include a Game Feature Data Asset (class FGGameFeatureData, named exactly as the mod reference, in the mod's content root, with the plugin under Mods/GameFeatures); only mods with no content or assets (pure scripting libraries) are exempt. Icon libraries, input contexts, messages and user settings are no longer auto-detected and need scan rules. Both default Alpakit templates ('Blueprint Only', 'C++ & Blueprint') set game_feature true; only the '(Advanced)' library templates do not. The asset is a binary .uasset created in the Unreal editor, so it cannot be authored on a text-only Linux box. Mod Modules (C++) remain an alternative registration route for schematics and similar.

Evidence:

- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/UpdatingFromSml311.adoc>

  ```text
  99% of mods should now include a "Game Feature Data Asset",
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/UpdatingFromSml311.adoc>

  ```text
  with the exception being mods that introduce no new content nor assets, just scripting libraries.
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/UpdatingFromSml311.adoc>

  ```text
  Create a new `Data Asset` asset of class `FGGameFeatureData` in the mod's content root folder.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Templates/templates.json>

  ```text
  "game_feature": true,
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/Satisfactory/GameFeatureDataAsset.adoc>

  ```text
  Use SML Mod Modules if...
  ```

**Skeptic (source-accuracy): partially-correct**

Corrected statement: Since SML 3.12 / game 1.2, the upgrade guide says '99% of mods should now include' an FGGameFeatureData asset in the mod's content root, normally named exactly the mod reference ('GameFeatureData' is also supported but discouraged). Mods that introduce no new content or assets, just scripting libraries, are the exception. The guide labels icon libraries, input contexts, messages and user settings as types that 'must now be registered via the Game Features system'. Mod Modules remain a documented alternative for registration. The guide is draft: its banner still says 'SML 3.12 and 1.2 support have not been released yet!', and the GFDA page says 'still a work in progress'.

Note: The claim wrote 'must include'; the source says 'should', and 'must' appears only for registering the four asset types. All quotes are verbatim in UpdatingFromSml311.adoc, which is identical on master and Dev. templates.json at the pin and on dev has game_feature true on exactly the two default templates, and the two '(Advanced)' library templates have no such field. The plugin is moved to Mods/GameFeatures (step 3). The ExampleMod GFDA exists as the binary Mods/GameFeatures/ExampleMod/Content/ExampleMod.uasset. That 'cannot be authored on a text-only Linux box' is the claim's own inference; no source states it.

Evidence: <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/UpdatingFromSml311.adoc>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): confirmed**

Note: master and Dev have identical text. Two wording nits that do not change the decision. First, the source says 'should', not 'must': `99% of mods should now include a "Game Feature Data Asset",`. The hard requirement is only for the four formerly auto-detected types (FGUserSetting, FGMessage, FGIconLibrary, FGChildInputMappingContext), which 'must now be registered via the Game Features system'. Second, naming: `It should be named exactly the mod's mod reference.` and `Using the name GameFeatureData is also supported but discouraged`. The templates claim holds: templates.json at the pin and at dev both have 'game_feature': true for Blueprint Only and C++ & Blueprint, and none for the two '(Advanced)' library templates. GameFeatureDataAsset.adoc (same on master and Dev) lists the Mod Modules alternative: 'Use SML Mod Modules if... You already need a mod module to utilize other SML-specific features like mod subsystems or chat commands'. It also says 'This page is still a work in progress' and flags the Add Components action as 'not working as of Satisfactory 1.2'. A WARNING block documents a bug where the editor shows an inaccurate 'Current State' and the feature is not actually loaded in-game. Its fix is to check that Initial State is Active. The ExampleMod ships a binary ExampleMod.uasset in Content, so the asset cannot be hand-written as text.

Evidence: <https://github.com/satisfactorymodding/Documentation/blob/8175a04f8c194160b3a145d118a7fbc8f243fa99/modules/ROOT/pages/Development/UpdatingFromSml311.adoc>

### C9 (verified-primary, decision-driving)

SMR upload and target rules. SMR's validator allows exactly three targets (Windows, WindowsServer, LinuxServer). A multi-target zip must have one top-level folder per target, each containing <ModReference>.uplugin; files outside target folders are rejected, and the per-target .uplugin files must be byte-identical. The SMR uplugin schema requires FileVersion (const 3), Version and GameVersion. The docs warn that omitting a target makes the mod not work on that platform, and Linux dedicated servers use LinuxServer, so LinuxServer is mandatory for the owner's server. C++ mods go through an automated approval wait.

Evidence:

- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/validation/validation.go#L38>

  ```text
  var AllowedTargets = []string{"Windows", "WindowsServer", "LinuxServer"}
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/validation/validation.go>

  ```text
  multi-target plugin contains different .uplugin files
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/validation/validation.go>

  ```text
  multi-target plugin contains file outside of target directories: 
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/static/uplugin-json-schema.json>

  ```text
  "required": [
      "FileVersion",
      "Version",
      "GameVersion"
    ],
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/ReleaseMod.adoc>

  ```text
  If you leave out one of these targets, the mod will not work on that platform!
  ```

**Skeptic (source-accuracy): confirmed**

Note: smr-api HEAD at f1f39d09 (default branch is 'staging') has AllowedTargets = Windows, WindowsServer, LinuxServer (validation.go line 38). validateMultiTargetPlugin takes targets from each <Dir>/<ModRef>.uplugin and rejects invalid targets. It rejects files outside target directories and different .uplugin bytes (bytes.Equal). The version-upload workflow calls ExtractModInfo with withValidation=true. The uplugin schema requires FileVersion (const 3), Version and GameVersion. Extra rule: SemVersion's major must equal Version. ReleaseMod.adoc line 431 says 'If you leave out one of these targets, the mod will not work on that platform!'. UploadToSMR.adoc line 165 documents the 'automated approval process' wait for C++ mods, about 1 to 3 minutes. 'LinuxServer mandatory' is a sound inference from the Linux Server description in the docs.

Evidence: <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/validation/validation.go>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): confirmed**

Note: f1f39d09 is the HEAD of both smr-api main and staging, so it is not stale. Latest release is v3.1.0 (2026-06-02). Upload calls ExtractModInfo with withValidation=true (workflows/versionupload/extract_mod_info.go L39). Claim details verified in code: AllowedTargets (L38), 'multi-target plugin contains invalid target', 'multi-target plugin contains file outside of target directories: ' and `!bytes.Equal(lastData, data)` -> 'multi-target plugin contains different .uplugin files'. Schema required is [FileVersion, Version, GameVersion]. Nuances the claim missed: (1) any non-empty subset of the three targets passes validation, so the 'LinuxServer mandatory' rule comes from the docs ('If you leave out one of these targets, the mod will not work on that platform!'), not from the validator; (2) validation.go L433 `errors.New("SemVer major version should match Version")` requires the integer Version to equal the SemVer major; (3) the archive must be under 1GB. Approval: .dll and .so files get type 'sml_mod', so shouldAutoApprove (workflow.go L167 `if obj.Type != "pak" {`) is false and a VirusTotal scan runs. Only pak/utoc/ucas auto-approve. If the scan fails, the version is never published. UploadToSMR.adoc says 'you'll have to wait for it to be approved by the automated approval process', about 1 to 3 minutes.

Evidence: <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/validation/validation.go>

### C10 (verified-primary)

Alpakit mechanics. Alpakit launches RunUAT 'PackagePlugin' (a BuildCookRun subclass in Mods/Alpakit/Source/Alpakit.Automation/PackagePlugin.cs). Client platforms go to -platform=Win64 (EGS and Steam client targets), server platforms to '-server -serverplatform=Win64+Linux'. With -merge it zips every target into ArchivedPlugins/<Mod>/<Mod>.zip. Per-mod targets live in <Mod>/Config/Alpakit.ini ([ModTargets] Targets=...; empty means all three). It rewrites .modules BuildId to 'SML' so mods load on other game builds. For plugins under Mods/GameFeatures it injects "GameFeature": true into the staged .uplugin (do not put it in source). A mod gets compiled only if its uplugin has Modules. The SML CI command line is the reference invocation.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/.github/workflows/build.yml#L192>

  ```text
  PackagePlugin -Project="/tmp/work/SML/FactoryGame.uproject" -dlcname=SML -merge -build -server -clientconfig=Shipping -serverconfig=Shipping -platform=Win64 -serverplatform=Win64+Linux -nocompileeditor -installed
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Source/Alpakit/Private/AlpakitProfile.cpp>

  ```text
  ServerCommand = TEXT(" -server -serverplatform=") + ServerPlatforms.RightChop(1);
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Source/Alpakit/Private/ModTargetsConfig.cpp>

  ```text
  bLinuxServer = Targets.Contains(TEXT("LinuxServer")) || Targets.Num() == 0;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Source/Alpakit.Automation/PackagePlugin.cs>

  ```text
  moduleManifestFile.BuildId = "SML";
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Source/Alpakit.Automation/PackagePlugin.cs>

  ```text
  PluginDescriptor.AddOrSetFieldValue("GameFeature", true);
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/ReleaseMod.adoc>

  ```text
  `<starter project folder>/Saved/ArchivedPlugins/YourModReference/YourModReference-TargetPlatformName.zip`
  ```

### C11 (verified-primary, decision-driving)

ficsit.app content policy requires two disclosures before a mod can be published: 'Network Activity Transparency' (any external network connectivity) and 'AI Usage Transparency', which explicitly includes whether portions of the mod's source code were directly created by generative AI models. The 3.12 docs say uploading additional versions is blocked until the disclosure questions are answered. A mod co-developed with Claude must disclose this. The live page ficsit.app/content-policy is client-rendered and unreadable via fetch, so the policy text was read from the smr-frontend source.

Evidence:

- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/content-policy/%2Bpage.svelte>

  ```text
  If portions of the mod's source code were directly created by generative AI models
  ```
- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/content-policy/%2Bpage.svelte>

  ```text
  Mods are required to clearly and explicitly state, in the mod's "AI Usage Transparency" section on Ficsit.app,
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/UpdatingFromSml311.adoc>

  ```text
  Note that uploading additional versions of your mod is blocked until disclosure questions are answered.
  ```

**Skeptic (source-accuracy): confirmed**

Note: The three quotes are verbatim in smr-frontend 0b129362 content-policy/+page.svelte (lines 51, 80, 96). The same file on main differs only by a typo fix. ficsit.app/content-policy returns only a SvelteKit bootstrap, so it is client-rendered. The API enforces more than the docs: smr-api gql/resolver_versions.go CreateVersion (before the upload ID is created) returns 'you must provide an AI use disclosure before uploading a version', demands a message unless the type is no_ai_usage, and returns 'you must provide a network use disclosure before uploading a version' if that is nil. That covers any version upload including the first, not only 'additional versions' as the 3.12 guide words it. The guide's stale 'not released yet' banner applies to this page as well.

Evidence: <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_versions.go>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): confirmed**

Note: The content-policy text in smr-frontend 0b129362 is identical to staging HEAD (compare status 'identical'), and ficsit.app/content-policy fetches empty, as the claim says. Strong new confirmation that this is enforced in the API, not just policy: gql/resolver_versions.go L55 'you must provide an AI use disclosure before uploading a version', L59 'you must provide a description for your AI use disclosure before uploading a version' (required whenever disclosure_type is not no_ai_usage), and L63 'you must provide a network use disclosure before uploading a version'. This blocks every upload, including the first. The enum in schemas/mod.graphql is `no_ai_usage | ai_usage | runtime_ai_usage`. The policy lists 'If portions of the mod's source code were directly created by generative AI models' and also covers ficsit.app page text and patch notes written with AI assistance. The exemptions, such as 'core Unreal Engine libraries, IDE intellisense suggestion ranking', do not cover code co-written with Claude, so the disclosure applies. 'Network Activity Transparency' names 'Multiplayer interaction and game-to-game synchronization' as the only exceptions.

Evidence: <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_versions.go>

### C12 (verified-primary, decision-driving)

SDK mapping. SML master HEAD d2162c99 (2026-08-11) has currentVersion.txt 502094 and SML.uplugin 3.12.0, and is the owner's pin. The v3.12.0 tag (1a7d2ca3, 'Bump version') has currentVersion.txt 491125. Header-update commits exist only for CL491125 (efc3e830, 2026-05-25), CL502094 (d2162c99) and CL513885 (dev HEAD 8eebc9e6, 2026-10-12); there are none for 493833 or 495413. FactoryGame.uproject at the pin has EngineAssociation 5.6.1-CSS. The Docs branch notice tells newcomers to clone dev or master, whichever was pushed to most recently; today that is dev (2026-10-12 vs master 2026-08-11), and the repo-versions include sets :suggested-branch: dev.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/currentVersion.txt>

  ```text
  502094
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/v3.12.0/Source/FactoryGame/currentVersion.txt>

  ```text
  491125
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/FactoryGame.uproject>

  ```text
  "EngineAssociation": "5.6.1-CSS",
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/_includes/branch-notice.adoc>

  ```text
  Of those two, you probably want whichever branch was pushed to most recently,
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/_includes/repo-versions.adoc>

  ```text
  :suggested-branch: dev
  ```

**Skeptic (source-accuracy): partially-correct**

Corrected statement: In the 1.2 era (CL491125 onward) the only 'Update headers' commits are CL491125 (efc3e830, 2026-05-25), CL502094 (d2162c99) and CL513885 (8eebc9e6). Older refreshes also exist, for example 'Update project to CL480321' (dffa7d20, 2026-04-23, 961 Source/ files). A commit search found nothing for 493833 or 495413. ':suggested-branch: dev' only takes effect in StarterProjectViaZip.adoc's zip URL, because the branch-notice line that uses it is commented out. The live advice is 'whichever branch was pushed most recently', which today is dev.

Note: The 'only' was too absolute. Everything else checks out. currentVersion.txt is 502094 at the pin, 491125 at the v3.12.0 tag and 513885 on dev. SML.uplugin is 3.12.0 and FactoryGame.uproject has EngineAssociation '5.6.1-CSS' at the pin. Master tip is d2162c99 (2026-08-11T22:13Z). Dev tip is 8eebc9e6 (2026-10-12). The tag 1a7d2ca3 is 'Bump version'. The branch-notice and repo-versions quotes are verbatim at docs d35cc911. Caveat: the commit search is weaker evidence than reading the history.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/commits/dev>

**Skeptic (Lens 2 - CONTRADICTION AND FRESHNESS): partially-correct**

Corrected statement: Master HEAD d2162c99 (2026-08-11, currentVersion 502094, SML.uplugin 3.12.0, EngineAssociation 5.6.1-CSS) and the v3.12.0 tag (491125) are as stated, and dev HEAD 8eebc9e6 (2026-10-12) carries 513885. But header-update commits are not 'only' 491125/502094/513885. The currentVersion.txt history on dev also has dffa7d20 (2026-04-23) 'Update project to CL480321' within the 1.2 cycle, plus many earlier ones. What does hold is that there are none for 493833 or 495413.

Note: Verified at the pin: currentVersion.txt '502094', v3.12.0 '491125', `"EngineAssociation": "5.6.1-CSS",` (dev has the same). Commit history of currentVersion.txt on dev: 8eebc9e6 'Update headers to CL513885', d2162c99 'Update headers to CL502094', efc3e830 'Update headers to CL491125', dffa7d20 'Update project to CL480321', 9dd548de 'Update headers to CL455399'. No 493833 or 495413. Contradicting detail: ficsit.app has 17 recent versions whose game_version is '>=495413' or '495413', so there is a public CL for which SML published no headers. A mod floor can name a CL that has no SDK commit. Branch-recency check: dev 2026-10-12T17:44:42Z vs master 2026-08-11T22:13:23Z; the 'dev or master, whichever was pushed to most recently' text is in branch-notice.adoc. repo-versions.adoc does set `:suggested-branch: dev`, but the line that uses it is commented out, so the rendered advice is the 'whichever was pushed most recently' rule. UE fork latest release is 5.6.1-css-83 (2026-06-02), matching the owner. No newer engine release exists. Header diff pin->dev touches FGBuildableConveyorBase.h, FGGameState.h and FGSignSubsystem.h, so C++ mods built at 502094 may need a rebuild for 513885.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/commits/dev/Source/FactoryGame/currentVersion.txt>

### C13 (secondary)

The '>=491125' ranges on SML, ExampleMod and the server mods (FRM 1.5.3 uploaded 2026-08-12) are minimum-build floors on the game changelist, not exact pins: 491125 is v1.2.2.2, the 1.2 launch build of 2026-06-02 (wiki), and it is the CL that SML 3.12.0 was released for. The docs state that only minimum ranges are useful because future build numbers are unknown. The dependency on SML is a separate semver range (^3.12.0).

Evidence:

- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/ReleaseMod.adoc>

  ```text
  Since we don't know what future game build numbers will be, only minimum ranges (ex. `>=something`) are useful for now.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/GameFeatures/ExampleMod/ExampleMod.uplugin>

  ```text
  "SemVersion": "^3.12.0"
  ```
- <https://satisfactory.wiki.gg/wiki/Patch_1.2.2.2>

  ```text
  v1.2.2.2 – Build 491125
  ```

### C14 (verified-primary)

Header delta CL502094 to CL513885 is small (9 files) but not ABI-neutral on its face: AFGGameState gains a delegate member and an OnRep function and its mSignSubsystem becomes ReplicatedUsing; AFGBuildableConveyorBase turns two inline accessors into out-of-line functions and drops ReportInvalidStateAndRequestConveyorRepReset; AFGSplineBuildableSnapHologram adds a virtual override. INFERENCE from the diff (not stated by the SML team): a C++ mod compiled against 502094 headers may see layout or vtable differences on a 513885 game, so rebuilding against the current headers is the safe course.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/compare/d2162c999bbaa9650594ee9ad6e6e06190306009...8eebc9e659f0b2ff4392d2f8cf6e2cff60c8ea49>

  ```text
  FOnSingleSubsystemReplicatedDelegate mOnSignSubsystemReplicated;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/compare/d2162c999bbaa9650594ee9ad6e6e06190306009...8eebc9e659f0b2ff4392d2f8cf6e2cff60c8ea49>

  ```text
  virtual void PostConstructMessageDeserialization() override;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/compare/d2162c999bbaa9650594ee9ad6e6e06190306009...8eebc9e659f0b2ff4392d2f8cf6e2cff60c8ea49>

  ```text
  bool HasAnyThroughputMonitorsAttached() const;
  ```

### C15 (verified-primary)

The custom engine's latest release is 5.6.1-css-83 (2026-06-02), the same as the owner's pin; Docs repo-versions uses 'latest' and lists that tag as its commented example. Both SML branches and both Docs branches say UE 5.6.1, so no engine change is pending for either branch.

Evidence:

- <https://github.com/satisfactorymodding/UnrealEngine/releases/tag/5.6.1-css-83>

  ```text
  5.6.1-css-83
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/_includes/repo-versions.adoc>

  ```text
  // :engine-gh-release: tag/5.6.1-css-83
  ```

### C16 (verified-primary)

3.12 compile-time rules for new C++ code: FactoryGame uses TObjectPtr, and the docs say mods with raw UObject pointer members will fail to compile in the next SML update (force the check now with NonEngineNativePointerMemberBehavior=Disallow); all modules must follow BuildSettingsVersion V5, which means full include paths (e.g. Hologram/FGHologram.h); the Alpakit C++ template uses C++20 and depends on FactoryGame and SML. These are rules that text-authored C++ on the VPS can already follow.

Evidence:

- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/UpdatingFromSml311.adoc>

  ```text
  In the next SML update, mods still using raw pointers will be unable to compile.
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/UpdatingFromSml311.adoc>

  ```text
  A change in CSS build settings means that all modules must follow `BuildSettingsVersion.V5`.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Templates/CPPAndBlueprintBlank/Source/PLUGIN_NAME/PLUGIN_NAME.Build.cs>

  ```text
  CppStandard = CppStandardVersion.Cpp20;
  ```

### C17 (verified-primary)

Installing Game Feature mods needs updated mod tooling: ficsit-cli v0.7.0 (2026-06-02) added GameFeatures support and Satisfactory Mod Manager v3.1.0 (2026-06-06) can manage Game Feature mods. Whether the owner's server installs mods with ficsit-cli, and at which version, is not known.

Evidence:

- <https://github.com/satisfactorymodding/ficsit-cli/releases/tag/v0.7.0>

  ```text
  feat: GameFeatures support (#83)
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModManager/releases/tag/v3.1.0>

  ```text
  Now capable of managing mods that use [Unreal Game Features]
  ```

### C18 (verified-primary)

What can and cannot be done on the Linux VPS. An official Linux modding setup exists, but it needs the access-gated custom engine (standard engine-access steps first), wine, msvc-wine, and an Audiokinetic account to obtain Wwise; SML's own CI uses Wwise credentials as secrets. Authoring text on the VPS is fine (.uplugin, Build.cs, headers/sources, Config/*.ini, Alpakit.ini, AccessTransformers.ini, engine-agnostic logic tested with g++). The Unreal editor/cook/package steps (FGGameFeatureData asset, buildable/recipe/schematic assets, Alpakit packaging, in-game test) need the owner's PC or a prepared Linux dev environment.

Evidence:

- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/Linux/LinuxSetup.adoc>

  ```text
  significant community efforts have resulted in a mostly stable Linux development environment for Satisfactory modding.
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/Linux/LinuxSetup.adoc>

  ```text
  First, follow the standard setup directions to get access to the custom engine.
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/dependencies.adoc>

  ```text
  Create an account if you don't have one yet, or sign in, to download and run the installer.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/.github/workflows/build.yml>

  ```text
  WWISE_EMAIL: ${{ secrets.WWISE_EMAIL }}
  ```

### C19 (verified-primary)

Wwise 2025.1.6 is now the version inside the game and in SML dev CI. SML master CI uses SDK 2023.1.14.8770 with integration 2023.1.14.3555; SML dev CI uses SDK 2025.1.6.9117 with integration 2025.1.6.4248. The v1.2.4.1 notes say Coffee Stain updated their internal Wwise to 2025.1.6, re-enabled audio routing so Metasound works again for mods, and fixed a dedicated-server crash with Conveyor Throughput Monitors. Moving the Starter Project to dev therefore also means re-integrating Wwise, and Wwise is needed even for mods with no sound.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/.github/workflows/build.yml#L176>

  ```text
  --sdk-version "2023.1.14.8770"
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/8eebc9e659f0b2ff4392d2f8cf6e2cff60c8ea49/.github/workflows/build.yml#L178>

  ```text
  --integration-version "2025.1.6.4248"
  ```
- <https://steamstore-a.akamaihd.net/news/externalpost/steam_community_announcements/1846018067941164>

  ```text
  We updated our internal Wwise version to 2025.1.6 and we originally made this change in preparation for future development
  ```
- <https://steamstore-a.akamaihd.net/news/externalpost/steam_community_announcements/1846018067941164>

  ```text
  Fixed crash on Dedicated Server when placing a Conveyor Throughput Monitor on a Conveyor Belt
  ```

### C20 (unverified)

One open bug report against SML 3.12.0 on game build 502094: the 'Mod Savegame Settings' (Session Settings) menu shows only SML's own category, with Array_Sort comparator errors in Widget_SessionSettings. It is a single user report with no maintainer reply seen, so treat it as unconfirmed; it matters only if the mod uses SML Session Settings.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/issues/429>

  ```text
  Array_Sort failed: Failed to find comparator function
  ```

### C21 (secondary)

The first Satisfactory expansion, 'Core Values', was announced on 2026-09-29. No release date is published: the Steam store page lists it as 'To be announced' and the wiki says a date is not announced, citing an interview that mentions 'not in 2026'. No experimental branch for it was found. A future expansion is a likely trigger for another game and SML update cycle, but nothing official says it changes the engine.

Evidence:

- <https://steamstore-a.akamaihd.net/news/externalpost/steam_community_announcements/1844751498235169>

  ```text
  https://store.steampowered.com/app/5276140/Satisfactory__Core_Values_Expansion/
  ```
- <https://store.steampowered.com/app/5276140/Satisfactory__Core_Values_Expansion/>

  ```text
  To be announced.
  ```

## Catalog

| Name | Reference | Licence | Last updated | Relevance |
| --- | --- | --- | --- | --- |
| [SatisfactoryModLoader (SML + Starter Project)](https://github.com/satisfactorymodding/SatisfactoryModLoader) | d2162c999bbaa9650594ee9ad6e6e06190306009 (master), 8eebc9e659f0b2ff4392d2f8cf6e2cff60c8ea49 (dev), tag v3.12.0 = 1a7d2ca3a4281cf589bd842a814fd7a55eac4a99 | GPL-3.0 (per GitHub API) | 2026-10-12 | Source of truth for versions, GameVersion check, Alpakit packaging, CI command line and Wwise pins. |
| [Satisfactory Modding Documentation (source)](https://github.com/satisfactorymodding/Documentation) | master d35cc911d6849cb39740cb884735ec5d9ffb2f67; Dev 8175a04f8c194160b3a145d118a7fbc8f243fa99 | none declared in GitHub API | 2026-10-07 | software-versions.adoc, dependencies.adoc, UpdatingFromSml311.adoc, ReleaseMod.adoc, UploadToSMR.adoc, GameFeatureDataAsset.adoc. |
| [docs.ficsit.app (latest) and docs-dev.ficsit.app](https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/dependencies.html) | https://docs-dev.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/dependencies.html |  | 2026-10-13 | Confirms that the published docs and the dev docs differ on Wwise. |
| [smr-api (ficsit.app backend)](https://github.com/satisfactorymodding/smr-api) | f1f39d09e070938dd48f3862816309ab07f96f79 | GPL-3.0 (per GitHub API) | 2026-06-02 | Authoritative target and .uplugin validation rules; usable to write a local pre-upload validator. |
| [smr-frontend content policy source](https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/content-policy/%2Bpage.svelte) |  | GPL-3.0 (per GitHub API) | 2026-06-03 | Mandatory disclosures before publishing. |
| [ficsit.app GraphQL API (GET queries)](https://api.ficsit.app/v2/query) |  |  | 2026-10-13 | Verifies published SML/FRM game_version and sml_version ranges. |
| [satisfactorymodding/UnrealEngine releases](https://github.com/satisfactorymodding/UnrealEngine/releases) |  | NOASSERTION (per GitHub API) | 2026-06-02 | Engine pin; unchanged for master and dev. |
| [ficsit-cli and Satisfactory Mod Manager releases](https://github.com/satisfactorymodding/ficsit-cli/releases) | https://github.com/satisfactorymodding/SatisfactoryModManager/releases/tag/v3.1.0 | GPL-3.0 (per GitHub API) | 2026-07-09 | Server-side and player-side install path for Game Feature mods. |
| [Steam news feed for Satisfactory (ISteamNews API)](https://api.steampowered.com/ISteamNews/GetNewsForApp/v0002/?appid=526870&count=25&maxlength=0&format=json) |  |  | 2026-10-12 | Primary source for release dates and patch notes; does not state CL numbers. |
| [Satisfactory wiki patch pages](https://satisfactory.wiki.gg/wiki/Patch_1.2.4.1) |  |  | 2026-10-12 | Version-to-CL mapping used for C5 and C13. |
| [steamcmd.net Steam appinfo mirror](https://api.steamcmd.net/v1/info/1690800) |  |  | 2026-10-12 | Shows public and experimental on the same build ID; secondary. |

## Open questions

- Which build is CL 503514 (the CL named in SML dev commit 77c1847b of 2026-08-20), and when did the game first ship Wwise 2025.1.6? The v1.2.4.1 notes talk about a 'previous patch' disabling audio routing, but the v1.2.4.0 notes do not mention it, and no public release maps to 503514.
- Does the owner's Windows PC currently have Wwise 2023.1.14.8770 integrated (implied by the master pin)? Moving to dev requires installing 2025.1.6.9117 and re-integrating it.
- Is the owner's server build 24656085 the Dedicated Server app (1690800) or the client app? Does the wolveix image update on restart? The mirror shows server build 25782485 live since 2026-10-12, so a restart may already move the server to 1.2.4.1, and the existing server mods (FRM 1.5.3 and others) have not been confirmed on it.
- Will the SML team ship a release for 1.2.4.1 (3.12.1 or 3.13)? No milestone, release candidate or open PR indicates it, and 3.12.0 binaries were built from CL491125 headers.
- Can a mod that adds buildables register everything through SML C++ Mod Modules without any GameFeatureData asset? The docs describe both routes, but icon libraries, input contexts, messages and user settings require Game Feature scan rules; this needs an editor test.
- SMR's API reports engine_version "5.2" for SML 3.12.0 while the docs and FactoryGame.uproject say 5.6.1. Likely stale SMR metadata; not verified and not relied on.
- Core Values release date and whether it brings an engine, SML major or Game Feature changes: no official information.
- Whether SMR enforces anything about the Compatibility Info for the Experimental branch when public and experimental are the same build: not researched.

## Recommendations

- Treat the game, SDK branch, Wwise and server as one coordinated step: update the server to 1.2.4.1, move the Starter Project to SML dev (currently 8eebc9e6, CL513885), and install Wwise 2025.1.6.9117 with integration 2025.1.6.4248. Test the existing server mods on 1.2.4.1 first. If the server must stay on 502094, stay on master d2162c99 and Wwise 2023.1.14.8770 for now, and do not mix.
- Keep the mod's GameVersion at >=491125 and its SML dependency at ^3.12.0 (as SML, ExampleMod and FRM 1.5.3 do). Do not click Alpakit's 'update Game Version' warning unless the server is on the same CL, and make this a checklist item before every release.
- Start from Alpakit's 'C++ & Blueprint' game-feature template and plan the editor-only work (FGGameFeatureData named exactly as the mod reference, buildable/recipe/schematic assets, scan rules, packaging, multiplayer and dedicated-server tests) as separate steps on the owner's PC.
- Author on the VPS only text artifacts that the validators describe: .uplugin (FileVersion 3, Version equal to the SemVersion major, SemVersion equal to VersionName, GameVersion, SML ^3.12.0 with Enabled true, no GameFeature field), Build.cs from the template, Config/Alpakit.ini with all three targets, TObjectPtr and full include paths. Add a local pre-upload script that checks the .uplugin against smr-api's uplugin-json-schema.json and the identical-per-target rule.
- Package with all three targets (Windows, WindowsServer, LinuxServer) via Alpakit Release and inspect the merged zip for the three target folders before uploading to ficsit.app.
- Before publishing, prepare the 'AI Usage Transparency' and 'Network Activity Transparency' texts (state that source code was written with generative AI assistance and that the mod makes no external network connections) and set Compatibility Info. Check that the server's mod installer supports Game Feature mods (ficsit-cli >=0.7.0).
- Re-check Steam build, SML dev/master HEADs, the docs Dev branch and the SML issue tracker right before the first build, since the dev branch moved on 2026-10-12 and the Docs Dev branch moved on 2026-10-07. If the mod uses SML Session Settings, test against issue #429.

