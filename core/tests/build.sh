#!/usr/bin/env bash
# Builds the test suite: core/tests/build.sh [out-dir]   (default out-dir: build/tests)
#   SLG_WITH_CORE=auto|yes|no   link the core (core/src/*.cpp + bridge_sim.cpp) or only the spec-only layer
#   CXX=g++  JOBS=1  SANITIZE=none|ubsan|asan  OPT=-O1
# Run it through the lock wrapper of the machine; it never starts more than JOBS compilers.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
out="${1:-$root/build/tests}"
cxx="${CXX:-g++}"
jobs="${JOBS:-1}"
opt="${OPT:--O1}"
with_core="${SLG_WITH_CORE:-auto}"
sanitize="${SANITIZE:-none}"

warn="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wfloat-equal"
flags="-std=c++20 $opt -g $warn -fno-exceptions -fno-rtti -I$root/core/tests -I$root/core/include"
case "$sanitize" in
  ubsan) flags="$flags -fsanitize=undefined -fno-sanitize-recover=all -D_GLIBCXX_ASSERTIONS" ;;
  asan)  flags="$flags -fsanitize=address,undefined -fno-sanitize-recover=all -D_GLIBCXX_ASSERTIONS -ftrivial-auto-var-init=pattern" ;;
esac

tests=$(ls "$root"/core/tests/*.cpp | grep -v -e '/bridge_sim.cpp$' -e '/bridge_nosim.cpp$')
core_srcs=""
bridge="$root/core/tests/bridge_nosim.cpp"
lib_srcs=$( { find "$root/core/src" "$root/core/host" -type f -name '*.cpp' 2>/dev/null || true; } | LC_ALL=C sort)
if [ "$with_core" != "no" ] && [ -f "$root/core/include/slg/simulator.hpp" ] && [ -n "$lib_srcs" ]; then
  core_srcs="$lib_srcs"
  bridge="$root/core/tests/bridge_sim.cpp"
elif [ "$with_core" = "yes" ]; then
  echo "SLG_WITH_CORE=yes but core/include/slg/simulator.hpp or core/src/**/*.cpp is missing" >&2
  exit 1
fi

mkdir -p "$out/obj"
objs=""
compile_one() {
  src="$1"; obj="$2"
  if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ] || [ -n "$(find "$root/core/tests" "$root/core/include" -name '*.hpp' -newer "$obj" 2>/dev/null | head -1)" ]; then
    echo "  CXX $(basename "$src")"
    $cxx $flags -c "$src" -o "$obj"
  fi
}
export -f compile_one
export cxx flags root
list=""
for s in $tests $bridge $core_srcs; do
  o="$out/obj/$(echo "${s#$root/}" | tr '/' '_' | sed 's/\.cpp$/.o/')"
  list="$list $s:$o"
  objs="$objs $o"
done
printf '%s\n' $list | xargs -P "$jobs" -I{} bash -c 'compile_one "${1%%:*}" "${1##*:}"' _ {}
echo "  LD  slg_tests"
$cxx $flags $objs -o "$out/slg_tests"
echo "built $out/slg_tests ($([ "$bridge" = "$root/core/tests/bridge_sim.cpp" ] && echo 'with core' || echo 'spec-only'))"
