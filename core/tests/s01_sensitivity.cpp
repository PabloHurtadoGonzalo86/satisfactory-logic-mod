// A check of the tests themselves: the reference is run with deliberate misreadings of the spec (the mutants of
// 03 section 7 that live at the level of semantics and of the blob) and every misreading must make some property
// fail, in particular the one 03 names as its killer. A misreading that survives is a test that is missing.
// (The mutation agent applies the real mutants to the core; this is the quick check at the reference level.)
// SLG_SKIP_SENSITIVITY=1 skips it.
#include <cstdlib>

#include "engine.hpp"
#include "testkit.hpp"

namespace slgt {

namespace {

struct Mutant {
  const char* id;
  Quirk quirk;
  std::vector<const char*> killers;   // properties 03 section 7 names for it
};

}  // namespace

SLG_TEST(S01_sensitivity, 0, "every deliberate misreading of the spec is caught by the properties 03 names") {
  if (env().haveCase) {
    ctx.skip("single-case replay");
    return;
  }
  if (const char* skip = std::getenv("SLG_SKIP_SENSITIVITY")) {
    if (skip[0] != '\0' && skip[0] != '0') {
      ctx.skip("SLG_SKIP_SENSITIVITY is set");
      return;
    }
  }
  static const Mutant table[] = {
      {"M01", Quirk::InPlaceStep, {"T01_order_independence", "G05_ring2"}},
      {"M02", Quirk::SumFusion, {"T05_component_tables", "G06_wired9"}},
      {"M03", Quirk::LastDriverWins, {"T01_order_independence", "T05_component_tables"}},
      {"M04", Quirk::TruthyAboveOne, {"T05_component_tables"}},
      {"M05a", Quirk::DelayShort, {"T04_quiescence_and_fifo", "G03_and_delay2"}},
      {"M05b", Quirk::DelayLong, {"T04_quiescence_and_fifo", "G03_and_delay2"}},
      {"M06", Quirk::CompareLess, {"T05_component_tables"}},
      {"M07", Quirk::LatchSetDominates, {"G04_latch_both", "T05_component_tables"}},
      {"M08", Quirk::ToggleByLevel, {"T05_component_tables", "T14_theory_bridge"}},
      {"M09", Quirk::ToggleStartsLow, {"G08_toggle_new_high"}},
      {"M10", Quirk::ToggleRearmsOnConnect, {"G09_toggle_edit_connect"}},
      {"M11a", Quirk::SensorLate, {"G01_lat_not", "T14_theory_bridge"}},
      {"M11b", Quirk::ActuatorLate, {"G01_lat_not", "T14_theory_bridge"}},
      {"M12", Quirk::NextIdNotSaved, {"T10_determinism_and_hashes", "T11_edit_semantics"}},
      {"M13", Quirk::DelayBlobReversed, {"T10_determinism_and_hashes", "T12_blob_robustness"}},
  };
  const std::uint64_t savedCases = env().cases;
  env().cases = 30;
  const auto fails = [](const std::string& name, const PropertyInfo& info, const EngineFactory& f) {
    Ctx quiet(name, info.number);
    quiet.setQuiet(true);
    info.fn(quiet, f);
    return quiet.failed();
  };
  for (const Mutant& m : table) {
    const EngineFactory f = quirkedRefFactory(m.quirk);
    std::string killedBy;
    bool any = false;
    for (const char* k : m.killers) {
      const auto it = propertyTable().find(k);
      if (it == propertyTable().end()) {
        SLG_FAIL(ctx, cat("the killer ", k, " of ", m.id, " is not a registered property"));
        continue;
      }
      const bool dead = fails(it->first, it->second, f);
      SLG_CHECK_MSG(ctx, dead, cat(m.id, " (", quirkName(m.quirk), ") survives ", k, ", the test 03 section 7 names for it"));
      if (dead) {
        any = true;
        killedBy += std::string(k) + " ";
      }
    }
    if (!any) {   // none of the named ones saw it: look for any property that does
      for (const auto& [name, info] : propertyTable()) {
        if (info.on == RunOn::SimOnly) continue;
        if (fails(name, info, f)) {
          any = true;
          killedBy += "(only) " + name + " ";
          break;
        }
      }
    }
    SLG_CHECK_MSG(ctx, any, cat(m.id, " (", quirkName(m.quirk), ") is not detected by any property"));
    ctx.note(cat(m.id, " ", quirkName(m.quirk), ": killed by ", killedBy.empty() ? std::string("nothing") : killedBy));
  }
  env().cases = savedCases;
}

}  // namespace slgt
