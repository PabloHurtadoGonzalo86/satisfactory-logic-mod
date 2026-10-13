# Publishing and policy

_Generated from `data/publishing-and-policy.json` by `tools/render_research.py`; do not edit by hand. Research run of 2026-10-13. Claims are the researcher's; verdicts are from independent skeptics told to refute them. Wording is kept verbatim (English)._

## Summary

SMR publishing is a manual, web-first flow. Sign in with GitHub/Google/Facebook, create the mod page (auto-approved; mod_reference is immutable, 3-32 chars), then edit it to set the mandatory AI-use and network-use disclosures, then upload Alpakit's multi-target YourModReference.zip. Pak-only versions auto-approve; any .dll/.so is scanned on VirusTotal (auto-approved if malicious<=1 and suspicious<=1). Limits: 4 new mods per user and 5 versions per mod per 24 h; a version name can never be reused.

Decision-driving findings:
1. This Claude-built mod must declare ai_usage with a written description. That is normal on SMR (378 of the top 1000 mods disclose AI use, including Smart!).
2. SMR rejects a descriptor whose Version differs from the SemVersion major. The owner's CompanionPresence.uplugin (Version 5, SemVersion 0.4.1, exact pins) must not be the template.
3. RequiredOnRemote defaults to true and SML checks on both sides. Docs say exact versions; code defaults to ">= own version"; the combined effect is lockstep versions. A mismatch disconnects with "Client/Server missing mods".
4. SML is GPL-3.0 with no linking exception. The FSF treats dynamically linked plug-ins as one combined program, so license the mod GPL-compatibly (GPL-3.0-or-later, MIT or Apache-2.0) and publish source.
5. FactoryGame headers are (c) Coffee Stain: never vendor them or game assets.
6. The SMR ToS forbids links to off-site mod binaries and use of the API in closed-source apps.

Install: SMM and ficsit-cli manage servers over SFTP/SMB. Manual copy into FactoryGame/Mods/<ModRef> is unsupported. The wolveix image has no SFTP; SKIPUPDATE=true pins the game build.

Automation: `ficsit smr upload <mod-id>` works with the login session token. How to obtain it is undocumented; api.ficsit.dev is an unofficial staging instance.

Policy: I found no Coffee Stain EULA, fan-content or trademark policy (the Steam EULA page failed to load), only community guidelines. "Satisfactory" and "FICSIT" appear in many mod names; add an unaffiliated disclaimer.

Docs hosting: SMR sanitized Markdown, GitHub Pages, docs.ficsit.app by request.

## Claims at a glance

| Id | Confidence | Decision-driving | Skeptic verdicts |
| --- | --- | --- | --- |
| c1 | verified-primary | yes | confirmed |
| c2 | verified-primary | no | not verified (not decision-driving) |
| c3 | verified-primary | no | not verified (not decision-driving) |
| c4 | verified-primary | no | not verified (not decision-driving) |
| c5 | verified-primary | yes | partially-correct |
| c6 | verified-primary | yes | confirmed |
| c7 | verified-primary | no | not verified (not decision-driving) |
| c8 | verified-primary | yes | partially-correct |
| c9 | verified-primary | no | not verified (not decision-driving) |
| c10 | verified-primary | no | not verified (not decision-driving) |
| c11 | verified-primary | no | not verified (not decision-driving) |
| c12 | verified-primary | no | not verified (not decision-driving) |
| c13 | verified-primary | no | not verified (not decision-driving) |
| c14 | unverified | no | not verified (not decision-driving) |
| c15 | secondary | no | not verified (not decision-driving) |
| c16 | verified-primary | yes | confirmed |
| c17 | verified-primary | yes | partially-correct |
| c18 | verified-primary | no | not verified (not decision-driving) |
| c19 | unverified | no | not verified (not decision-driving) |
| c20 | secondary | no | not verified (not decision-driving) |
| c21 | verified-primary | yes | confirmed |
| c22 | verified-primary | no | not verified (not decision-driving) |
| c23 | verified-primary | no | not verified (not decision-driving) |
| c24 | verified-primary | no | not verified (not decision-driving) |
| c25 | verified-primary | no | not verified (not decision-driving) |

## Claims in detail

### c1 (verified-primary, decision-driving)

SMR refuses every version upload until the mod page carries an AI-use disclosure. The content policy requires disclosure when portions of source code were directly created by generative AI. Because this mod is Claude-built, the disclosure must be type ai_usage with a written description.

Evidence:

- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/content-policy/+page.svelte>

  ```text
  If portions of the mod's source code were directly created by generative AI models
  ```
- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/content-policy/+page.svelte>

  ```text
  Mods are required to clearly and explicitly state, in the mod's "AI Usage Transparency" section on Ficsit.app, the purposes for which any AI tools have been used in the creation of the mod or its content.
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_versions.go>

  ```text
  return "", errors.New("you must provide an AI use disclosure before uploading a version")
  ```

**Skeptic (SOURCE ACCURACY): confirmed**

Note: All 3 quotes verified verbatim at the pinned commits; smr-api pin f1f39d09 is current main HEAD; smr-frontend main differs from the pin only by a typo fix in content-policy. CreateVersion returns 'you must provide an AI use disclosure before uploading a version' when ai_use_disclosure is nil, and a second error if the type is not no_ai_usage and the message is empty. Enum (schemas/mod.graphql): no_ai_usage | ai_usage | runtime_ai_usage. ai_usage is the right type by elimination (runtime_ai_usage is for runtime generative AI, and the policy lists source code created by generative AI as disclosable). Omitted by the claim: CreateVersion ALSO refuses upload without network_use_disclosure (nil check at the next lines). UpdateMod forbids clearing an AI disclosure once set. createMod itself works without a disclosure (tests/ai_use_disclosure_test.go); only version upload is gated. Live ficsit.app pages are SPA shells, so evidence is repo source, not rendered pages.

Evidence: <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_versions.go>

### c2 (verified-primary)

Disclosure mechanics: the new-mod form omits the disclosure fields, so they are set by editing the page after creation. The network-use disclosure is also required before upload, and an empty string appears to mean 'no network use' (stored as null/empty/non-empty). A disclosure, once set, cannot be cleared. Mods must state any external network connectivity, except multiplayer and game-to-game synchronization.

Evidence:

- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/lib/components/mods/ModForm.svelte>

  ```text
  // NewMod does not have compatibility or disclosure fields
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_versions.go>

  ```text
  return "", errors.New("you must provide a network use disclosure before uploading a version")
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_mods.go>

  ```text
  return nil, errors.New("this mod already has an AI use disclosure, and thus it cannot be cleared")
  ```
- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/lib/models/mods.ts>

  ```text
  Since network usage disclosure is stored as a null/empty string/non-empty string,
  ```

### c3 (verified-primary)

AI-assisted mods are common and accepted on SMR. In my 2026-10-13 survey of the top 1000 mods by downloads, 378 disclose ai_usage, 319 no_ai_usage, 1 runtime_ai_usage and 302 have none set. Smart! (about 1.29M downloads) discloses AI-assisted development.

Evidence:

- <https://api.ficsit.app/v2/query?query=%7BgetModByReference%28modReference%3A%22SmartFoundations%22%29%7Bai_use_disclosure%7Bdisclosure_type%20message%7D%20network_use_disclosure%7D%7D>

  ```text
  Smart! uses AI-assisted development. AI tools help with investigation, drafting, refactoring, documentation, and debugging support.
  ```

### c4 (verified-primary)

Account and page creation: sign-in is via GitHub, Google or Facebook. A new mod page is created already approved (no staff review of the page). Name is 3-32 chars and short description 16-128 chars. A user may create at most 4 new mods per 24 h. Hidden mods are excluded from lists and send no Discord #mod-updates message, so unhide before the first version.

Evidence:

- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/UploadToSMR.adoc>

  ```text
  then selecting to sign in with an existing GitHub, Google, or Facebook account.
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_mods.go>

  ```text
  SetApproved(true).
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/util/ratelimits.go>

  ```text
  ModsPer24h   = 4
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/UploadToSMR.adoc>

  ```text
  In addition, updates to Hidden mods will not cause messages to be sent in the `#mod-updates` channel of the modding Discord.
  ```

### c5 (verified-primary, decision-driving)

The mod_reference is a one-way door. It cannot be changed after the SMR page exists. It must match the plugin folder and the C++ Source folder. The SMR form accepts ^[a-zA-Z][a-zA-Z0-9_]*$ with 3-32 chars (docs: no special characters or spaces, underscores allowed, PascalCase suggested). The backend blocks the references satisfactory, factorygame, sml, satisfactorymodloader, examplemod, docmod and gamefeatures, exact match only. Generic candidates Redstone, Signal, Logic, Wiring and FicsitRedstone were free on 2026-10-13.

Evidence:

- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/UploadToSMR.adoc>

  ```text
  You can't change your mod reference after you have added your mod to SMR!
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/SimpleMod/gameworldmodule.adoc>

  ```text
  Mod references can't contain special characters (ex. `+`, `#`, `.`, `ß`, ...) *nor can they contain spaces*. However, they can contain underscores (`_`).
  ```
- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/lib/models/mods.ts>

  ```text
  .regex(/^([a-zA-Z][a-zA-Z0-9_]*)$/)
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_mods.go>

  ```text
  "satisfactorymodloader": true,
  ```

**Skeptic (SOURCE ACCURACY): partially-correct**

Corrected statement: The mod_reference is effectively permanent: docs say it cannot be changed once the SMR page exists, and UpdateMod returns 'this mod already has set a mod reference' unless the mod ID equals its reference. It must match the plugin folder and C++ Source folder. The SMR frontend form enforces ^[a-zA-Z][a-zA-Z0-9_]*$ and 3-32 chars; the backend NewMod has no validate tag on mod_reference and the DB schema only sets MaxLen(32), so the regex and the minimum are frontend-only. The backend blocklist (satisfactory, factorygame, sml, satisfactorymodloader, examplemod, docmod, gamefeatures) is a whole-string match on strings.ToLower(), so case-insensitive. CreateMod also rejects a reference equal to an existing mod ID. On 2026-10-13 a GET to api.ficsit.app getModByReference returned 'mod not found' for Redstone, Signal, Logic, Wiring and FicsitRedstone, but that only shows nothing visible holds them: Mod uses SoftDeleteMixin with a plain unique index on mod_reference, so a soft-deleted mod could still block creation. Availability is proven only when CreateMod succeeds.

Note: Doc quotes (UploadToSMR.adoc, gameworldmodule.adoc), the mods.ts regex and min(3).max(32), and the blocklist map all verified verbatim at the pins (docs pin d35cc911 = master HEAD; mods.ts identical on main). The availability sub-claim has no cited evidence; my own API check is the only support.

Evidence: <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/gql/resolver_mods.go>

### c6 (verified-primary, decision-driving)

Descriptor rules enforced by SMR on upload: Version must equal the SemVersion major, GameVersion must be present and SemVersion must be valid SemVer. Docs advise 1.0.0 for the first public release (avoid major 0), a minimum-range GameVersion (>=N) and ^ dependency ranges. The owner's CompanionPresence.uplugin (Version 5 vs SemVersion 0.4.1, GameVersion 502094.0.0, SML 3.12.0 exact) would be rejected, so do not copy it as the template for the new mod.

Evidence:

- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/validation/validation.go>

  ```text
  return nil, errors.New("SemVer major version should match Version")
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/ReleaseMod.adoc>

  ```text
  As a rule of thumb, version `1.0.0` should be used for the first public release of your mod.
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/ReleaseMod.adoc>

  ```text
  Since we don't know what future game build numbers will be, only minimum ranges (ex. `>=something`) are useful for now.
  ```
- <[private repository of the owner, not published]>

  ```text
  "Version": 5, "VersionName": "0.4.1", "SemVersion": "0.4.1",
  ```

**Skeptic (SOURCE ACCURACY): confirmed**

Note: Verified: 'SemVer major version should match Version' (validateUPluginJSON, upload runs with withValidation=true), static/uplugin-json-schema.json requires FileVersion, Version and GameVersion, and the SemVersion pattern is strict. ReleaseMod.adoc quotes on 1.0.0, avoiding major 0, '>=something' GameVersion and the ^ prefix are verbatim. CompanionPresence.uplugin at ee830e84 (and the local copy) has Version 5 / SemVersion 0.4.1, so split[0]='0' != '5' and SMR rejects it. Nuances: (1) the Version/SemVersion mismatch is the only hard rejection; GameVersion '502094.0.0' and an exact SML '3.12.0' pass the schema and are only fragile or against the docs' advice. (2) SemVersion is not in the schema's required list; if absent SMR synthesizes 'Version.0.0' (docs call it required). (3) The cited CompanionPresence quote is three adjacent lines joined with spaces, not one contiguous line.

Evidence: <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/validation/validation.go>

### c7 (verified-primary)

Version bookkeeping: a version number cannot be reused, even after deletion. At most 5 versions per mod per 24 h. Archive must be under 1 GB. The upload validates the .uplugin against a public JSON schema (static/uplugin-json-schema.json, usable offline on Linux; its Plugins 'items' array is tuple-form, so only the first dependency entry is pattern-checked). Pre-release SemVer suffixes are not auto-downloaded by the Mod Manager.

Evidence:

- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/workflows/versionupload/extract_mod_info.go>

  ```text
  reusing the version name of a deleted mod version is not allowed
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/workflows/versionupload/extract_mod_info.go>

  ```text
  if len(versions) >= 5 {
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/validation/validation.go>

  ```text
  errors.New("mod archive must be < 1GB")
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/ReleaseMod.adoc>

  ```text
  The Mod Manager does not automatically download any versions with a prerelease suffix -
  ```

### c8 (verified-primary, decision-driving)

RequiredOnRemote defaults to true. SML runs the mod-list check on both sides: the server validates clients (IsServer=true) and, since SML 3.9.0, the client validates the server (IsServer=false). A mod with RequiredOnRemote true that the other side lacks, or whose version fails the range, closes the connection with '<Client|Server> missing mods: ...'. Docs say the default RemoteVersionRange is exact-version. Code defaults it to '>= own SemVersion'. Because each side requires the other to be >= itself, the effective result is lockstep versions: a 1.0.0 server rejects a 1.1.0 client. The check can be skipped with the console variable SML.SkipRemoteModListCheck. RequiredOnRemote=false makes a mod server-only or client-only without error.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/Network/SMLNetworkManager.cpp>

  ```text
  const FString Reason = FString::Printf( TEXT("%s missing mods: %s"), IsServer ? TEXT("Client") : TEXT("Server"), *JoinedModList );
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/Network/SMLNetworkManager.cpp>

  ```text
  if ( !ModInfo.bRequiredOnRemote || (ModInfo.Name == TEXT("SML") && !IsServer) )
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/Network/SMLNetworkManager.cpp>

  ```text
  ValidateSMLConnectionData(Connection, false);
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/Network/SMLNetworkManager.cpp>

  ```text
  TEXT("SML.SkipRemoteModListCheck"),
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/ModLoading/ModLoadingLibrary.cpp>

  ```text
  this->RemoteVersionRange = FVersionRange::CreateRangeWithMinVersion(Version);
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/Util/SemVersion.cpp>

  ```text
  const FVersionComparator Comparator(EVersionComparisonOp::GREATER_EQUALS, MinVersion);
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/ReleaseMod.adoc>

  ```text
  This field is optional, and defaults to your `SemVersion` if unspecified, meaning both sides must have the exact same mod version installed.
  ```

**Skeptic (SOURCE ACCURACY): partially-correct**

Corrected statement: Core is right: RequiredOnRemote defaults to true; both sides run ValidateSMLConnectionData (server with IsServer=true, client via HandleWelcomePlayer_Client with false; docs: 'The reverse (client checking host) is implemented as of SML 3.9.0'); the default RemoteVersionRange in code is '>= own SemVersion' while the docs say exact; and SML.SkipRemoteModListCheck skips the check. The effective result is lockstep versions. But the example is backwards: with a 1.0.0 server and a 1.1.0 client, the server accepts (1.1.0 matches >=1.0.0) and the CLIENT closes the connection, because the server's 1.0.0 fails its >=1.1.0 range, showing 'Server missing mods: ... required: >=1.1.0, server: 1.0.0'. Conversely a 1.1.0 server rejects a 1.0.0 client. RequiredOnRemote=false only skips the branch where the remote lacks the mod; if both sides have the mod and the version is out of range, the connection is still rejected. An explicit RemoteVersionRange such as ^1.0.0 would allow minor skew.

Note: All five SML quotes and the docs quote are verbatim. SMLNetworkManager.cpp, ModLoadingLibrary.cpp and SemVersion.cpp are byte-identical at the pin and on master (the pin is master HEAD). ModLoadingLibrary.cpp sets RemoteVersionRange = CreateRangeWithMinVersion(Version) both in SetupDefaults and when SemVersion parses; an explicit RemoteVersionRange field overrides it.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/Network/SMLNetworkManager.cpp>

### c9 (verified-primary)

Dedicated-server install: Satisfactory Mod Manager (3.0.0+) and ficsit-cli manage a remote server over SFTP/FTP/SMB. The server must be stopped first. Do not copy the client Mods folder; the client binaries do not work on servers. Manual installation is allowed but unsupported on Discord. Manual layout: unzip the target's files into <game root>/FactoryGame/Mods/<ModRef>/, or Mods/GameFeatures/ if the packaged uplugin has GameFeature=true; the folder name must equal the uplugin name. Players must have identical mods to join.

Evidence:

- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/ForUsers/DedicatedServerSetup.adoc>

  ```text
  Do NOT naively copy-paste your client's mods folder to a server - this will not work!
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/ForUsers/DedicatedServerSetup.adoc>

  ```text
  We do not provide support on the Discord for dedicated servers that have had mods manually installed.
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/ForUsers/DedicatedServerSetup.adoc>

  ```text
  Before you start installing mods, make sure the server is not currently running.
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/ManualInstallDirections.adoc>

  ```text
  - If `true`: Files go in `<game root>/FactoryGame/Mods/GameFeatures/`
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/ManualInstallDirections.adoc>

  ```text
  Make sure the name of the folder containing the mod's files is named EXACTLY the same thing as the mod's uplugin file.
  ```

### c10 (verified-primary)

wolveix/satisfactory-server (MIT) works like a standalone install for mods but has no S/FTP server. Use SFTP into the host's gamefiles directory or a sidecar SFTP container. SKIPUPDATE=true stops the container from updating the game on restart, which is the documented way to hold a game build while mods catch up.

Evidence:

- <https://github.com/wolveix/satisfactory-server/blob/11e03a579e6bbbbfff7cfca3d479873df650d0fa/README.md>

  ```text
  Mod support is still a little rough around the edges, but they do now work.
  ```
- <https://github.com/wolveix/satisfactory-server/blob/11e03a579e6bbbbfff7cfca3d479873df650d0fa/README.md>

  ```text
  The game automatically updates when the container is started or restarted (unless you set `SKIPUPDATE=true`).
  ```

### c11 (verified-primary)

Targets and zip layout: the SMR multi-target zip may only contain top-level target directories named Windows, WindowsServer and LinuxServer. Each holds <ModRef>.uplugin and its files. No file may sit outside a target directory, and the uplugin must be byte-identical across targets. Docs recommend all 3 targets, but SMR does not require all of them; a missing target means the mod cannot be installed there. Alpakit stores the choice in <Mod>/Config/Alpakit.ini [ModTargets]. The owner's Alpakit builds Windows and LinuxServer only, so Windows dedicated servers would be unsupported.

Evidence:

- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/validation/validation.go>

  ```text
  var AllowedTargets = []string{"Windows", "WindowsServer", "LinuxServer"}
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/validation/validation.go>

  ```text
  multi-target plugin contains different .uplugin files
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/ReleaseMod.adoc>

  ```text
  If you leave out one of these targets, the mod will not work on that platform!
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/Alpakit/Source/Alpakit/Private/ModTargetsConfig.cpp>

  ```text
  return FPaths::Combine(Mod->GetBaseDir(), TEXT("Config"), TEXT("Alpakit.ini"));
  ```

### c12 (verified-primary)

Approval path: blueprint/pak-only versions are published immediately. Any version containing a .dll or .so (every C++ mod) has those binaries scanned on VirusTotal and is auto-approved only if malicious<=1 and suspicious<=1; otherwise it waits for staff. Docs quote 1-3 minutes; ficsit-cli says up to 15 minutes. Native hooking code may trigger heuristics.

Evidence:

- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/workflows/versionupload/workflow.go>

  ```text
  if obj.Type != "pak" {
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/validation/virustotal.go>

  ```text
  return *stats.Malicious <= 1 && *stats.Suspicious <= 1, nil
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/UploadToSMR.adoc>

  ```text
  The approval process is generally quite quick, about 1 to 3 minutes,
  ```
- <https://github.com/satisfactorymodding/ficsit-cli/blob/608c169c664a7a53a6a3c183347a5bea912a680d/cmd/smr/upload.go>

  ```text
  version successfully uploaded, but has to be scanned for viruses, which may take up to 15 minutes
  ```

### c13 (verified-primary)

CI/CLI upload exists: `ficsit smr upload [flags] <mod-id> <file> <changelog...>` (ficsit-cli v0.7.1, GPL-3.0) takes the mod ID (not the reference) and --stability alpha|beta|release. The --api-key value is sent as the raw Authorization header. In smr-api that header is looked up as a login session token, which logout deletes. The frontend states the token cannot be refreshed. Community mods (FicsIt-Networks, KPatchwork) already run this from GitHub Actions. The ToS adds that the API may not be used in closed-source applications. Per-mod uploads are chunked at 10 MB, up to 100 parts.

Evidence:

- <https://github.com/satisfactorymodding/ficsit-cli/blob/608c169c664a7a53a6a3c183347a5bea912a680d/cmd/smr/upload.go>

  ```text
  Use:   "upload [flags] <mod-id> <file> <changelog...>",
  ```
- <https://github.com/satisfactorymodding/ficsit-cli/blob/608c169c664a7a53a6a3c183347a5bea912a680d/cmd/smr/upload.go>

  ```text
  r.Header.Add("Authorization", viper.GetString("api-key"))
  ```
- <https://github.com/satisfactorymodding/smr-api/blob/f1f39d09e070938dd48f3862816309ab07f96f79/db/user.go>

  ```text
  user, err := From(ctx).UserSession.Query().Where(usersession.Token(authorization)).QueryUser().First(ctx)
  ```
- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/tos/+page.svelte>

  ```text
  The ficsit.app API may not be used in any application that is closed-source.
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/789c32fdc857fc9971f7f73fbbf3789c64f637bc/.github/workflows/deploy-to-smr.yml>

  ```text
  smr upload ${{ inputs.dev_run && '27CoRS8wTsW1wD' || '8d8gk4imvFanRs'}} FicsItNetworks.zip "$BODY"
  ```

### c14 (unverified)

How to obtain a long-lived API key is not documented anywhere I could find. The docs do not mention API tokens or an API-based upload (the UploadToSMR page says nothing about them). The only evidence is that the code accepts the login session token (the frontend stores it in the 'token' cookie for 30 days). Treat CI upload as an inference from code and ask on the modding Discord before relying on it.

Evidence:

- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/lib/core/graphql.ts>

  ```text
  // Token cannot be refreshed currently
  ```
- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/lib/components/auth/LoginDialog.svelte>

  ```text
  const oneMonth = new Date(new Date().getTime() + 30 * 24 * 60 * 60 * 1000);
  ```

### c15 (secondary)

api.ficsit.dev is a reachable GraphQL instance with its own data (67 mods on 2026-10-13). FicsIt-Networks' workflow uses it as --api-base for dry runs. It is not documented in the official docs, so rate it as an unofficial staging target.

Evidence:

- <https://api.ficsit.dev/v2/query?query=%7BgetMods(filter%3A%7Blimit%3A1%7D)%7Bcount%7D%7D>

  ```text
  {"data":{"getMods":{"count":67}}}
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/789c32fdc857fc9971f7f73fbbf3789c64f637bc/.github/workflows/deploy-to-smr.yml>

  ```text
  --api-base https://api.ficsit.dev
  ```

### c16 (verified-primary, decision-driving)

SML is licensed GPL-3.0 (plain GPLv3 text; no linking exception text found; no per-file headers found). The FSF's published position is that a dynamically linked plug-in that calls the main program is one combined program and must be licensed under the GPL or a GPL-compatible licence, with source. A more permissive GPL-compatible licence is allowed (MIT/Expat; Apache-2.0 with GPLv3). Mods in the wild vary (FicsIt-Networks GPL-3.0; FRM and Circuitry show no licence). Whether a given mod is a derivative work is a legal question that no court has settled for SML. This is the FSF's interpretation, not legal advice.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/LICENSE.txt>

  ```text
  GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007
  ```
- <https://www.gnu.org/licenses/gpl-faq.html#GPLPlugins>

  ```text
  If the main program dynamically links plug-ins, and they make function calls to each other and share data structures, we believe they form a single combined program, which must be treated as an extension of both the main program and the plug-ins.
  ```
- <https://www.gnu.org/licenses/gpl-faq.html#GPLAndPlugins>

  ```text
  If the main program and the plugins are a single combined program then this means you must license the plug-in under the GPL or a GPL-compatible free software license and distribute it with source code in a GPL-compliant way.
  ```
- <https://www.gnu.org/licenses/gpl-faq.html#GPLModuleLicense>

  ```text
  You can, if you wish, release your module under a license which is more lax than the GPL but compatible with the GPL.
  ```
- <https://www.gnu.org/licenses/license-list.html#Expat>

  ```text
  This is a lax, permissive non-copyleft free software license, compatible with the GNU GPL.
  ```
- <https://www.gnu.org/licenses/license-list.html#apache2>

  ```text
  This is a free software license, compatible with version 3 of the GNU GPL.
  ```

**Skeptic (SOURCE ACCURACY): confirmed**

Note: SML LICENSE.txt at the pin is byte-identical to the canonical GPLv3 text apart from the trailing newline, so there is no exception. GitHub reports GPL-3.0. 193 files under Mods/SML/Source have no license header (the only grep hits were 'FGPlayerController'); README and CREDITS have no license text. All 3 FAQ quotes and both license-list quotes (#Expat, #apache2) are verbatim in the right sections. FicsIt-Networks (Panakotta00/FicsIt-Networks) is GPL-3.0 per the GitHub API. FRM (porisius) has no license in the API and no LICENSE file in the root tree. Circuitry on SMR has an empty source_url and no license text on its page, which is weak evidence. 'No court has settled it for SML' is an absence claim, so unverifiable. Add: FAQ #GPLPluginsInNF says a GPL plug-in combined with a nonfree main program (FactoryGame) conflicts unless an exception is added, and the more direct entry #IfLibraryIsGPL says the whole combination must be GPL or GPL-compatible, which favours MIT or Apache-2.0 for a mod. FSF position only, not legal advice.

Evidence: <https://www.gnu.org/licenses/gpl-faq.html#GPLAndPlugins>

### c17 (verified-primary, decision-driving)

The FactoryGame headers in the SML repo are Coffee Stain's copyrighted work, not GPL. Coffee Stain itself provides them to the community in the game's CommunityResources folder (its patch notes list 'Adding RainRendering headers to community resources'). Docs say extracted game assets must not be redistributed without permission, while referencing base-game content via the documented placeholder method ships none of it. So the mod repo must not vendor Source/FactoryGame or extracted game assets.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildable.h>

  ```text
  // Copyright Coffee Stain Studios. All Rights Reserved.
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/StarterProjectStructure.adoc>

  ```text
  The header files are provided by Coffee Stain in the `CommunityResources/` folder of the game installation,
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/ExtractGameFiles.adoc>

  ```text
  As a reminder, redistributing these assets should not be done without permission, as they are copyrighted content of Coffee Stain Studios.
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/ReuseGameFiles.adoc>

  ```text
  you do not need to be concerned about shipping copyrighted files with your mod, since this process does not actually include the game's files in your own.
  ```
- <https://steamstore-a.akamaihd.net/news/externalpost/steam_community_announcements/1830797770246159>

  ```text
  Adding RainRendering headers to community resources
  ```

**Skeptic (SOURCE ACCURACY): partially-correct**

Corrected statement: FactoryGame headers carry '// Copyright Coffee Stain Studios. All Rights Reserved.'; Coffee Stain provides them in the game's CommunityResources/ folder; the docs say extracted assets should not be redistributed without permission; referencing base-game content via the documented method ships none of it. Not found in any source: an explicit ban on vendoring the headers. The SML repo itself publishes them (under a GPL-3.0 repo). So 'the mod repo should not vendor Source/FactoryGame or extracted assets' is a prudent inference, not a quoted rule.

Note: Verified verbatim: FGBuildable.h header at the pin, StarterProjectStructure.adoc, ExtractGameFiles.adoc, ReuseGameFiles.adoc. The cited akamai URL 302-redirects to https://steamcommunity.com/ogg/526870/announcements/detail/512989355492508996. The line 'Adding RainRendering headers to community resources' appears under MODDING in the embedded event JSON of that page (title '1.2 Experimental Update - V1.2.2.0'); the JSON holds several events, so do not tie it to one patch version. 'Not GPL' is an inference from All Rights Reserved.

Evidence: <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/ExtractGameFiles.adoc>

### c18 (verified-primary)

What Coffee Stain publishes about mods: only the Community Guidelines on the Satisfactory support page (mods are valued; hate/illegal content is not permitted; 'Mods are maintained by our community so there's nothing we can help with'). The Steam legal notice is a plain 'All rights reserved'. The community modding organization describes itself as unofficial and unaffiliated.

Evidence:

- <https://www.satisfactorygame.com/support>

  ```text
  Mods, worlds, factories, and other content you create are a great way to express your creativity and are highly valued.
  ```
- <https://www.satisfactorygame.com/support>

  ```text
  Mods are maintained by our community so there’s nothing we can help with if there are any issues caused by mods.
  ```
- <https://store.steampowered.com/api/appdetails?appids=526870>

  ```text
  ©2024 Coffee Stain Studios. Developed by Coffee Stain Studios. Published by Coffee Stain Publishing. All rights reserved.
  ```
- <https://github.com/satisfactorymodding/.github/blob/main/profile/README.md>

  ```text
  All members are volunteers. We are not affiliated with Coffee Stain Studios, but we greatly appreciate their support of the modding community.
  ```

### c19 (unverified)

I could not find any Coffee Stain-published EULA text, modding policy, fan-content policy or trademark/brand guideline for mods. Tried: Steam EULA page (load error), coffeestain.com/legal|terms|eula (404), satisfactorygame.com/legal|eula (404), support page, press page, web searches. So whether mods may use 'Satisfactory'/'FICSIT' in a name has no primary answer; this is a legal-risk question left unresolved.

Evidence:

- <https://store.steampowered.com/eula/526870_eula_0>

  ```text
  There was an error loading the content of this EULA.
  ```
- <https://www.satisfactorygame.com/support>

  ```text
  Community Guidelines and Code of Conduct
  ```

### c20 (secondary)

Naming in practice: SMR blocks only exact references (see c5). Many established mods carry the brand words in their names ('Satisfactory Mod Loader', 'SatisfactoryPlus (SF+/SFP)', 'FicsIt-Networks', 'Ficsit Farming'); the community calls SMR the 'FICSIT Augmentation Database'. This is community practice, not permission. For 'Redstone', Mojang's guidelines (the only policy I read) bar using the Minecraft name as the primary or dominant title and bar implying endorsement; I found no statement on the word Redstone itself. Safest choice: an original name with an 'unofficial, not affiliated' disclaimer.

Evidence:

- <https://api.ficsit.app/v2/query?query=%7BgetModByReference%28modReference%3A%22SatisfactoryPlus%22%29%7Bname%20mod_reference%7D%7D>

  ```text
  SatisfactoryPlus (SF+/SFP)
  ```
- <https://api.ficsit.app/v2/query?query=%7BgetModByReference%28modReference%3A%22SML%22%29%7Bname%20mod_reference%7D%7D>

  ```text
  Satisfactory Mod Loader
  ```
- <https://github.com/satisfactorymodding/.github/blob/main/profile/README.md>

  ```text
  also known as the FICSIT Augmentation Database
  ```
- <https://www.minecraft.net/en-us/usage-guidelines>

  ```text
  You may not use the Minecraft name as the primary or dominant name or title.
  ```

### c21 (verified-primary, decision-driving)

SMR Terms of Service constraints on the page and site plan: user-submitted content may not link to off-site mod binaries or archives (so link docs and source, not GitHub release zips). Uploading grants ficsit.app a worldwide royalty-free licence to distribute the content, while the ToS says content is for personal use and redistribution needs the owner's permission, so ship a LICENSE file. Automated bulk downloading beyond expected averages is prohibited (the ToS is dated 2020).

Evidence:

- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/tos/+page.svelte>

  ```text
  Any user-submitted content may not contain links to off-site mod binaries or archives.
  ```
- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/tos/+page.svelte>

  ```text
  By submitting content to our services, you are granting an infinite, non-exclusive, worldwide, royalty-free license for ficsit.app to store, distribute, copy or reproduce, edit, translate, reformat,
  ```

**Skeptic (SOURCE ACCURACY): confirmed**

Note: Both quotes verbatim (tos page identical at the pin and on main). Also verified: 'All user-submitted content is provided for personal use. You are not entitled to redistribute...', the Fair Use Policy ('drastically exceeds the expected average'), and 'Updated: 25/06/2020'. 'Link docs and source, not GitHub release zips' is the claim's advice, not ToS text; the ToS only bans links to off-site mod binaries or archives. Omitted but relevant to the owner's own-website plan: 'The ficsit.app API may not be used in any application that is closed-source.' Live ficsit.app/tos is a client-rendered SPA shell, so the evidence is the frontend source on main.

Evidence: <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/routes/tos/+page.svelte>

### c22 (verified-primary)

Documentation hosting: (a) the ficsit.app mod page full description is GitHub-flavoured Markdown rendered with marked, sanitized by DOMPurify and highlighted by Prism (C, C++, Lua, JS, JSON, Python), so screenshots must be externally hosted images (inferred). (b) ficsit.app also has Guides (Markdown; name 3-32 chars). (c) docs.ficsit.app is an Antora site that already aggregates other mods' docs repos (FicsIt-Networks, FRM, ContentLib, ...). The team will add yours on request via Discord. Its pages are GPL-3.0 unless noted. (d) GitHub Pages is free for public repos with a 1 GB site limit, and it can serve a branch or folder of the owner's repo.

Evidence:

- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/lib/utils/markdown.ts>

  ```text
  import DOMPurify from 'dompurify';
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/README.md>

  ```text
  Documentation for other mods is also served through the same site. If you have written documentation for your mod and would like it to be added, contact us.
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/supplemental-ui/partials/footer-content.hbs>

  ```text
  Except where otherwise noted, docs.ficsit.app is licensed under the
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/antora-playbook.yml>

  ```text
  - url: https://github.com/porisius/FicsitRemoteMonitoring.git
  ```
- <https://docs.github.com/en/pages/getting-started-with-github-pages/what-is-github-pages>

  ```text
  GitHub Pages is available in public repositories with GitHub Free and GitHub Free for organizations,
  ```
- <https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits>

  ```text
  Published GitHub Pages sites may be no larger than 1 GB.
  ```

### c23 (verified-primary)

Icons and metadata: the in-game icon is Resources/Icon128.png (square, >=128x128), included via Config/PluginSettings.ini [StageSettings] +AdditionalNonUSFDirectories=Resources. The SMR logo is optional (default gear logo), the form accepts png/jpeg/gif and the server re-encodes to WebP. No SMR logo dimension requirement was found. DocsURL and SupportURL become buttons in the in-game mod list. The Discord #mod-updates post shows the first lines of the changelog.

Evidence:

- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/Adding_Ingame_Mod_Icon.adoc>

  ```text
  this icon should be a square and at least 128x128 pixels.
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/BeginnersGuide/Adding_Ingame_Mod_Icon.adoc>

  ```text
  +AdditionalNonUSFDirectories=Resources
  ```
- <https://github.com/satisfactorymodding/smr-frontend/blob/0b1293626be45e871136c83f3c055567d0833a93/src/lib/components/mods/ModForm.svelte>

  ```text
  accept="image/png,image/jpeg,image/gif"
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/UploadToSMR.adoc>

  ```text
  The first few lines of this changelog will be displayed along with the version number in the Discord's #mod-updates channel,
  ```

### c24 (verified-primary)

Game-version coupling: SML refuses to load a mod whose GameVersion range does not match the running changelist, so every Coffee Stain patch can break installs until updated. SML master and the owner's pin are identical at CL502094, while SML dev already carries headers for CL513885. After testing, set Compatibility State to Works for Stable and Experimental. SMM shows Damaged/Broken as warnings.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/ModLoading/ModLoadingLibrary.cpp>

  ```text
  Plugin %s requires game version %s (current: %d)
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/commit/8eebc9e659f0b2ff4392d2f8cf6e2cff60c8ea49>

  ```text
  Update headers to CL513885
  ```
- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/UploadToSMR.adoc>

  ```text
  set the "Compatibility State" to `Works` for both the Stable and Experimental branch.
  ```

### c25 (verified-primary)

Build automation: SML itself is built from Linux CI via Wine and msvc-wine on a self-hosted runner. The community cannot publish its runner images (MSVC, Unreal Engine and Wwise licence restrictions), and the custom engine fork is access-controlled. A fully hosted release pipeline is therefore not available; the realistic path is Alpakit on the owner's Windows PC followed by manual or CLI upload. SML's own releases attach the multi-target zip to SMR.

Evidence:

- <https://github.com/satisfactorymodding/Documentation/blob/d35cc911d6849cb39740cb884735ec5d9ffb2f67/modules/ROOT/pages/Development/Linux/LinuxCi.adoc>

  ```text
  Due to MSVC, Unreal Engine, and Wwise license restrictions, built images of our custom runners cannot be public.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/ReleaseProcess.md>

  ```text
  Attach CI artifact to SMR as a new version of the SML mod (multi-target zip file), SMR changelog is the GitHub changelog
  ```

## Catalog

| Name | Reference | Licence | Last updated | Relevance |
| --- | --- | --- | --- | --- |
| [Satisfactory Mod Repository frontend (ficsit.app)](https://github.com/satisfactorymodding/smr-frontend) | 0b1293626be45e871136c83f3c055567d0833a93 | GPL-3.0 | 2026-06-03 | Primary source for submission fields, policy text and ToS. |
| [SMR API](https://github.com/satisfactorymodding/smr-api) | f1f39d09e070938dd48f3862816309ab07f96f79 | GPL-3.0 | 2026-06-02 | Defines exactly what an upload must satisfy. static/uplugin-json-schema.json can be reused for an offline lint on Linux. |
| [Satisfactory Modding Documentation](https://github.com/satisfactorymodding/Documentation) | d35cc911d6849cb39740cb884735ec5d9ffb2f67 | none in repo; site footer states GPL-3.0 | 2026-10-07 | Official release checklist and install instructions. |
| [SatisfactoryModLoader (SML and Starter Project)](https://github.com/satisfactorymodding/SatisfactoryModLoader) | d2162c999bbaa9650594ee9ad6e6e06190306009 (equals master HEAD; latest release v3.12.0) | GPL-3.0 | 2026-10-12 | Licence for dependent mods and the RequiredOnRemote network check. |
| [ficsit-cli](https://github.com/satisfactorymodding/ficsit-cli) | 608c169c664a7a53a6a3c183347a5bea912a680d (release v0.7.1) | GPL-3.0 | 2026-07-09 | Scriptable install and CI upload. |
| [Satisfactory Mod Manager](https://github.com/satisfactorymodding/SatisfactoryModManager) | docs ForUsers/SatisfactoryModManager.adoc | GPL-3.0 | 2026-09-12 | How players and the owner's server get the mod. |
| [wolveix/satisfactory-server](https://github.com/wolveix/satisfactory-server) | 11e03a579e6bbbbfff7cfca3d479873df650d0fa (release v1.9.11) | MIT | 2026-10-06 | Server install and game-version pinning. |
| [FicsIt-Networks deploy-to-smr workflow](https://github.com/Panakotta00/FicsIt-Networks/blob/789c32fdc857fc9971f7f73fbbf3789c64f637bc/.github/workflows/deploy-to-smr.yml) | 789c32fdc857fc9971f7f73fbbf3789c64f637bc | GPL-3.0 (repo) | 2026-10-05 | Shows the unofficial CI path and the api.ficsit.dev staging target. |
| [ficsit.app Content Policy](https://ficsit.app/content-policy) | smr-frontend src/routes/content-policy/+page.svelte | n/a | source commit 2026-06-03 | Binding rules for the mod page and uploads. |
| [SMR GraphQL API](https://api.ficsit.app/v2/query) | smr-api schemas/*.graphql | ToS: no closed-source consumers | live 2026-10-13 | Name and collision checks; disclosure survey. |
| [GNU GPL FAQ](https://www.gnu.org/licenses/gpl-faq.html) | anchors #GPLPlugins, #GPLAndPlugins, #GPLModuleLicense | n/a | n/a | Licence choice for a mod that links SML. |

## Name checks (SMR references, 2026-10-13)

| Candidate | Exists in SMR | Similar existing | Note |
| --- | --- | --- | --- |
| Redstone | no |  | No SMR mod has the exact reference 'Redstone' (2026-10-13). Avoid it anyway: it is a Mojang/Microsoft game term and implies a Minecraft association. |
| FicsitRedstone | no |  | Reference is free, but combines the FICSIT brand with a Minecraft term. Not recommended. |
| Signal | no |  | Exact reference free. |
| SignalGrid | no |  | Exact reference free. |
| Logic | no |  | Exact reference free but generic. |
| LogicGates | no | FicsitWiremod ('Circuitry - Logic gates and More', about 195k downloads) | Reference is free, but the topic overlaps existing prior art. |
| Wiring | no | FicsitWiremod (Circuitry) | Reference is free, but the topic overlaps existing prior art. |
| Ficsitron | no |  | Exact reference free. Contains the FICSIT word; see c20. |
| Satisfactory | no |  | Blocked by the backend: the disallowed list contains satisfactory, factorygame, sml, satisfactorymodloader, examplemod, docmod and gamefeatures, case-insensitive exact match. |

## Open questions

- Coffee Stain's own position on mod licensing, trademark use of 'Satisfactory'/'FICSIT' and fan content: no EULA or policy text was found (the Steam EULA page failed to load). Needs an explicit question to Coffee Stain or the modding Discord.
- How to obtain a stable SMR API key for CI (docs silent; code only shows the login session token). Ask on the modding Discord.
- Whether FeatureFlagAllowMultiTargetUpload is enabled in production. It is inferred on because SML's own release process uploads multi-target zips.
- Required or recommended SMR logo dimensions, and whether description images can be hosted on SMR rather than externally.
- Whether SML's GPL-3.0 is 'only' or 'or later' (LICENSE.txt has no or-later grant and no per-file headers), and legal advice on derivative-work status for a C++ mod linking SML.
- Epic's Unreal Engine EULA terms for distributing compiled plug-ins were not read (page returned 403). Royalties are very likely irrelevant for a free mod, but that is unverified.
- The ficsit.app ToS is dated 2020; its binding force for current uploads and whether the production site matches smr-api/smr-frontend master (dated May-June 2026) was not verifiable.

## Recommendations

- Licence the mod GPL-3.0-or-later (or MIT/Apache-2.0, both GPL-compatible). Ship LICENSE and a NOTICE crediting SML, publish the source and set source_url on the SMR page.
- Never commit Source/FactoryGame headers, placeholders or extracted game assets. Reference the SML Starter Project at the pinned commit, and use only the documented base-game reuse method for visuals.
- Pick the final mod_reference now (immutable, PascalCase, at most 32 chars). Prefer an original name; if the brand words are used, make them non-dominant and add 'Unofficial, not affiliated with or endorsed by Coffee Stain Studios' to the page, README and in-game description. Do not use 'Redstone' in the name.
- First release by hand: sign in via GitHub, create the page, edit it to set the AI disclosure to ai_usage (state that source code was drafted with Claude and reviewed and tested by the owner) and the network disclosure (none), upload the Alpakit zip, set Compatibility to Works, and make sure the page is not Hidden.
- Write a fresh .uplugin: Version 1 and SemVersion/VersionName 1.0.0, GameVersion '>=502094', SML dependency '^3.12.0', RequiredOnRemote default (omit) and no RemoteVersionRange. Add a Linux offline lint using smr-api's uplugin-json-schema.json plus the Version/SemVersion rule and a zip-layout checker.
- Build with Alpakit Release on the Windows PC for Windows and LinuxServer; add WindowsServer if Windows server hosts matter. Check that Config/Alpakit.ini lists the targets and that Resources/Icon128.png is packaged. Bump SemVersion on every upload, since versions are never reusable and 5 per 24 h is the cap.
- Server rollout: stop the container, install the LinuxServer files via SMM/ficsit-cli over SFTP (or a documented manual copy to FactoryGame/Mods/<ModRef>), pin the game build with SKIPUPDATE=true, and require identical mod versions on all clients (c8).
- Host documentation as README plus a docs folder or branch published with GitHub Pages in the mod repo, link it from DocsURL, SupportURL and the SMR description, and never link release zips from the SMR description. Offer the docs to the docs.ficsit.app team on Discord later.
- Consider CI upload only after confirming key issuance on the Discord. Prefer `ficsit smr upload` with the mod ID, test first on api.ficsit.dev, and keep the key as a secret.

