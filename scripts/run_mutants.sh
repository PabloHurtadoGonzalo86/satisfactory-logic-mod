#!/usr/bin/env bash
# Mutation gate (docs/spec/03-pruebas.md section 7).
#
# For every core/tests/mutants/M*.patch: copy the tree under build/mutants/, apply the patch with
# `patch -p1`, build, run ONLY the tests named in the patch's first line (tokens like T5 or G6) and
# expect a FAILURE. A mutant that survives is a missing test; a patch that does not apply or compile
# is an ERROR, never a kill. Each token is resolved against `slg_tests --list` (a test whose name starts
# with the letter and the number, padded or not: T5 finds T05_...), and the resolved selection must first
# PASS on the unmutated copy with at least one test run, so a wrong name cannot turn into a fake kill.
#
# Usage: scripts/run_mutants.sh [M01 M05 ...]      (default: every patch)
# Environment: SLG_LOCK (lock file; skipped when an ancestor already holds it), SLG_ULIMIT_V (KiB),
#   SLG_CASES, JOBS (make -j, default 1), MUTANT_TIMEOUT (s, default 900), MUTANT_MIN (default 12),
#   MUTANT_PASS_ARGS=1 (also pass the names as arguments; they always go in SLG_ONLY, the harness's
#   comma list of name substrings in core/tests/harness.hpp).
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT"

MUTANT_DIR=${MUTANT_DIR:-core/tests/mutants}
WORK_ROOT=${WORK_ROOT:-build/mutants}
CASES=${SLG_CASES:-200}
JOBS=${JOBS:-1}
TIMEOUT=${MUTANT_TIMEOUT:-900}
MIN_MUTANTS=${MUTANT_MIN:-12}
BASE="$WORK_ROOT/base"
WORK="$WORK_ROOT/work"
LOGS="$WORK_ROOT/logs"
COPY_ITEMS=(Makefile core scripts tools)
[[ -d docs/theory ]] && COPY_ITEMS+=(docs/theory)

lock_held() {
  [[ -n "${SLG_LOCK:-}" ]] || return 1
  local want fd target
  want=$(readlink -f "$SLG_LOCK" 2>/dev/null || printf '%s' "$SLG_LOCK")
  for fd in /proc/$$/fd/*; do
    target=$(readlink "$fd" 2>/dev/null || true)
    [[ "$target" == "$want" ]] && return 0
  done
  return 1
}

if [[ -z "${SLG_LOCK:-}" ]]; then
  echo "run_mutants: SLG_LOCK is not set, so this script takes no lock itself (an outer flock still serialises it; on the VPS prefer SLG_LOCK=<lock file>)" >&2
elif lock_held; then
  echo "run_mutants: lock $SLG_LOCK is already held by an ancestor process; not taking it again" >&2
fi

# locked <dir> <command...>: serialise, lower priority, optional address-space cap.
locked() {
  local dir=$1
  shift
  local -a prefix=()
  if [[ -n "${SLG_LOCK:-}" ]] && ! lock_held; then prefix=(flock "$SLG_LOCK"); fi
  ${prefix[@]+"${prefix[@]}"} nice -n 10 bash -c '
    cd "$1"; shift
    if [[ -n "${SLG_ULIMIT_V:-}" ]]; then ulimit -v "$SLG_ULIMIT_V"; fi
    exec "$@"' _ "$dir" "$@"
}

tests_of() {
  head -n 1 "$1" | grep -oE '\b[TG][0-9]{1,2}\b' | awk '!seen[$0]++' | tr '\n' ' ' | sed 's/ $//' || true
}

# resolve_names <dir> <tokens...>: full test names for tokens such as T5 or G6, from `slg_tests --list`.
# Prints the names; status 1 when a token matches nothing, 2 when the binary cannot list its tests.
resolve_names() {
  local dir=$1
  shift
  local listing token letter number found
  local -a out=()
  listing=$(locked "$dir" build/slg_tests --list 2>/dev/null) || return 2
  [[ -n "$listing" ]] || return 2
  for token in "$@"; do
    letter=${token:0:1}
    number=$((10#${token:1}))
    found=$(awk -F'\t' -v re="^${letter}0*${number}([^0-9]|\$)" '$1 ~ re { print $1 }' <<<"$listing")
    if [[ -z "$found" ]]; then
      echo "$token"
      return 1
    fi
    while read -r name; do out+=("$name"); done <<<"$found"
  done
  echo "${out[*]}"
}

# run_selection <dir> <names...>: the selected tests against the build in <dir>.
run_selection() {
  local dir=$1
  shift
  local csv
  csv=$(IFS=,; printf '%s' "$*")
  local -a args=()
  [[ -n "${MUTANT_PASS_ARGS:-}" ]] && args=("$@")
  locked "$dir" env SLG_CASES="$CASES" SLG_ONLY="$csv" timeout "$TIMEOUT" build/slg_tests ${args[@]+"${args[@]}"}
}

# ran_something <log>: the harness summary line says at least one test passed ("TESTS: n passed, ...").
# A log without that line (another harness) is accepted: the exit status alone has to decide.
ran_something() {
  local line passed
  line=$(grep -E '^TESTS: [0-9]+ passed' "$1" | tail -n 1 || true)
  [[ -z "$line" ]] && return 0
  passed=$(sed -E 's/^TESTS: ([0-9]+) passed.*/\1/' <<<"$line")
  [[ "$passed" -ge 1 ]]
}

declare -A equivalent=()
if [[ -f "$MUTANT_DIR/equivalent.txt" ]]; then
  while read -r id reason; do
    [[ -z "$id" || "$id" == \#* ]] && continue
    equivalent[$id]=${reason:-"(no reason given)"}
  done < "$MUTANT_DIR/equivalent.txt"
fi

selected=("$@")
if [[ ${#selected[@]} -eq 0 && -n "${MUTANTS:-}" ]]; then read -r -a selected <<< "$MUTANTS"; fi

mapfile -t all_patches < <(find "$MUTANT_DIR" -maxdepth 1 -type f -name 'M*.patch' 2>/dev/null | LC_ALL=C sort)
patches=()
if [[ ${#selected[@]} -eq 0 ]]; then
  patches=("${all_patches[@]}")
else
  for id in "${selected[@]}"; do
    found=
    for p in "${all_patches[@]}"; do
      [[ "$(basename "$p" .patch)" == "$id" ]] && { patches+=("$p"); found=1; }
    done
    [[ -n "$found" ]] || { echo "run_mutants: no patch for $id in $MUTANT_DIR" >&2; exit 2; }
  done
fi
if [[ ${#patches[@]} -eq 0 ]]; then
  echo "run_mutants: no $MUTANT_DIR/M*.patch found; refusing to report a pass" >&2
  exit 2
fi
if [[ ${#selected[@]} -eq 0 && ${#patches[@]} -lt $MIN_MUTANTS ]]; then
  echo "run_mutants: only ${#patches[@]} mutants, 03 section 7 asks for at least $MIN_MUTANTS (MUTANT_MIN=0 to override)" >&2
  exit 1
fi

rm -rf "$BASE" "$WORK" "$LOGS"
mkdir -p "$BASE" "$LOGS"
trap 'rm -rf "$WORK"' EXIT
tar --exclude='__pycache__' -cf - "${COPY_ITEMS[@]}" | tar -C "$BASE" -xf -
echo "run_mutants: building the unmutated tree in $BASE"
if ! locked "$BASE" make -j"$JOBS" build/slg_tests >"$LOGS/base.build.log" 2>&1; then
  echo "run_mutants: the unmutated tree does not build; see $LOGS/base.build.log" >&2
  tail -n 15 "$LOGS/base.build.log" >&2
  exit 1
fi

declare -A baseline=()
killed=0 survived=0 errors=0 tolerated=0
rows=()

record() { # id tests result detail
  rows+=("$(printf '%-6s %-22s %-12s %s' "$1" "$2" "$3" "$4")")
  printf '%-6s %-22s %-12s %s\n' "$1" "$2" "$3" "$4"
}

for patch in "${patches[@]}"; do
  id=$(basename "$patch" .patch)
  names=$(tests_of "$patch")
  if [[ -z "$names" ]]; then
    record "$id" "-" "ERROR" "first line of $patch names no tests (expected tokens like T5 or G6)"
    errors=$((errors + 1))
    continue
  fi
  # shellcheck disable=SC2206
  token_list=($names)
  resolved_status=0
  resolved=$(resolve_names "$BASE" "${token_list[@]}") || resolved_status=$?
  if [[ $resolved_status -eq 1 ]]; then
    record "$id" "$names" "ERROR" "$resolved matches no test in 'slg_tests --list'"
    errors=$((errors + 1))
    continue
  fi
  # Without --list the tokens themselves are the (substring) selection.
  [[ $resolved_status -eq 0 ]] || resolved=$names
  # shellcheck disable=SC2206
  name_list=($resolved)

  if [[ -z "${baseline[$resolved]+set}" ]]; then
    log="$LOGS/baseline-${names// /_}.log"
    if run_selection "$BASE" "${name_list[@]}" >"$log" 2>&1 && ran_something "$log"; then
      baseline[$resolved]=ok
    else
      baseline[$resolved]=bad
    fi
  fi
  if [[ "${baseline[$resolved]}" != ok ]]; then
    record "$id" "$names" "ERROR" "'$resolved' does not pass (with at least one test run) on the UNMUTATED tree (log: $LOGS/baseline-${names// /_}.log)"
    errors=$((errors + 1))
    continue
  fi

  rm -rf "$WORK"
  cp -a "$BASE" "$WORK"
  if ! (cd "$WORK" && patch -p1 --fuzz=0 --no-backup-if-mismatch -s -f <"$(readlink -f "$ROOT/$patch")") >"$LOGS/$id.patch.log" 2>&1; then
    record "$id" "$names" "ERROR" "patch does not apply (log: $LOGS/$id.patch.log)"
    errors=$((errors + 1))
    continue
  fi
  if ! locked "$WORK" make -j"$JOBS" build/slg_tests >"$LOGS/$id.build.log" 2>&1; then
    record "$id" "$names" "ERROR" "mutant does not compile (log: $LOGS/$id.build.log)"
    errors=$((errors + 1))
    continue
  fi

  status=0
  run_selection "$WORK" "${name_list[@]}" >"$LOGS/$id.test.log" 2>&1 || status=$?
  if [[ $status -eq 0 ]]; then
    if [[ -n "${equivalent[$id]+set}" ]]; then
      record "$id" "$names" "equivalent" "${equivalent[$id]}"
      tolerated=$((tolerated + 1))
    else
      record "$id" "$names" "SURVIVED" "tests passed on the mutant (log: $LOGS/$id.test.log)"
      survived=$((survived + 1))
    fi
  else
    detail="exit status $status"
    [[ $status -eq 124 ]] && detail="timed out after ${TIMEOUT}s"
    [[ -n "${equivalent[$id]+set}" ]] && detail="$detail; NOTE: listed in equivalent.txt but killed"
    record "$id" "$names" "killed" "$detail"
    killed=$((killed + 1))
  fi
done
rm -rf "$WORK"

echo
echo "ID     TESTS                  RESULT       DETAIL"
printf '%s\n' "${rows[@]}"
echo
echo "run_mutants: $killed killed, $survived survived, $tolerated proven equivalent, $errors errors (of ${#patches[@]})"
[[ $survived -eq 0 && $errors -eq 0 ]]
