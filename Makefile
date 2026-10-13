# Build and check entry points for the engine-independent core (pure C++20, no Unreal).
# Conventions, variables and the VPS procedure: core/README.md.

SHELL := /bin/bash
.SHELLFLAGS := -euo pipefail -c
.DELETE_ON_ERROR:
.DEFAULT_GOAL := all

CXX ?= g++
OPT ?= -O2
EXTRA_FLAGS ?=
EXTRA_LDFLAGS ?=
SLG_CASES ?= 200
PYTHON ?= python3
FUZZ_CXX ?= clang++
FUZZ_SECONDS ?= 60
FUZZ_ALLOW_EMPTY ?=
GOLDEN_DIR ?= core/golden
GOLDEN_TESTS ?= GOLDEN/
ASAN_DETECT_LEAKS ?= 1
ORACLE_CMD ?= $(PYTHON) -I -B tools/oracle/oracle.py run {lnet} {lscn}
RUN_CMD ?= build/slg_run {lnet} {lscn}
MUTANTS ?=

export SLG_CASES

STD_FLAGS := -std=c++20 -fno-exceptions -fno-rtti
WARN_FLAGS := -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wfloat-equal
# vptr needs RTTI; naming it keeps g++ and clang++ from guessing.
SAN_FLAGS := -fno-sanitize-recover=all -fno-sanitize=vptr -D_GLIBCXX_ASSERTIONS -ftrivial-auto-var-init=pattern -fno-omit-frame-pointer -O1 -g
CPPFLAGS := -MMD -MP -Icore/include $(if $(wildcard core/host/include),-Icore/host/include)
# Production code sees only its own tree; tests may also reach core/tests helpers and core/src internals.
EXTRA_INC = $(if $(filter core/src/% core/host/%,$<),-Icore/src)$(if $(filter core/tests/%,$<), -Icore/tests -Icore/src)

find_cpp = $(shell find $(1) -type f -name '*.cpp' 2>/dev/null | LC_ALL=C sort)
LIB_SRC := $(call find_cpp,core/src core/host)
# *_nosim.cpp is the spec-only stand-in for a core that is not linked; `make test` always links the core.
TEST_EXCLUDE ?= %_nosim.cpp
TEST_SRC := $(filter-out $(TEST_EXCLUDE),$(sort $(wildcard core/tests/*.cpp)))
BENCH_SRC := $(sort $(wildcard core/bench/*.cpp))
SELF_SRC := $(sort $(wildcard core/selfcheck/*.cpp))
RUN_SRC := $(sort $(wildcard core/tools/*.cpp))
FUZZ_SRC := $(sort $(wildcard core/fuzz/fuzz_*.cpp))

VARIANTS := default ubsan asan
BINDIR_default := build
BINDIR_ubsan := build/ubsan
BINDIR_asan := build/asan
OBJDIR_default := build/obj
OBJDIR_ubsan := build/ubsan/obj
OBJDIR_asan := build/asan/obj
CXXFLAGS_default := $(STD_FLAGS) $(WARN_FLAGS) $(OPT) $(EXTRA_FLAGS)
CXXFLAGS_ubsan := $(STD_FLAGS) $(WARN_FLAGS) -fsanitize=undefined $(SAN_FLAGS) $(EXTRA_FLAGS)
CXXFLAGS_asan := $(STD_FLAGS) $(WARN_FLAGS) -fsanitize=address,undefined $(SAN_FLAGS) $(EXTRA_FLAGS)
LDFLAGS_default := $(EXTRA_LDFLAGS)
LDFLAGS_ubsan := -fsanitize=undefined -fno-sanitize=vptr $(EXTRA_LDFLAGS)
LDFLAGS_asan := -fsanitize=address,undefined -fno-sanitize=vptr $(EXTRA_LDFLAGS)

define VARIANT_RULES
$$(BINDIR_$(1))/.flags: FORCE
	@mkdir -p $$(@D)
	@printf '%s\n' '$$(CXX) $$(CPPFLAGS) $$(CXXFLAGS_$(1))' | cmp -s - $$@ || printf '%s\n' '$$(CXX) $$(CPPFLAGS) $$(CXXFLAGS_$(1))' > $$@

$$(OBJDIR_$(1))/%.o: %.cpp $$(BINDIR_$(1))/.flags
	@mkdir -p $$(@D)
	$$(CXX) $$(CPPFLAGS) $$(EXTRA_INC) $$(CXXFLAGS_$(1)) -c $$< -o $$@

$$(BINDIR_$(1))/slg_tests: $$(patsubst %.cpp,$$(OBJDIR_$(1))/%.o,$$(LIB_SRC) $$(TEST_SRC)) | guard-lib guard-tests
	$$(CXX) $$(LDFLAGS_$(1)) $$(filter %.o,$$^) -o $$@
endef
$(foreach v,$(VARIANTS),$(eval $(call VARIANT_RULES,$(v))))

DEPS := $(foreach v,$(VARIANTS),$(patsubst %.cpp,$(OBJDIR_$(v))/%.d,$(LIB_SRC) $(TEST_SRC) $(BENCH_SRC) $(SELF_SRC) $(RUN_SRC)))
-include $(DEPS)

.PHONY: all test test-ubsan test-asan forbidden scripts-test golden golden-oracle mutants bench selfcheck fuzz clean help FORCE guard-lib guard-tests

all: build/slg_tests $(if $(BENCH_SRC),build/slg_bench) $(if $(SELF_SRC),build/slg_selfcheck) $(if $(RUN_SRC),build/slg_run)

guard-lib:
	@test -n "$(LIB_SRC)" || { echo "error: no core/src/**/*.cpp found; nothing to build" >&2; exit 1; }

guard-tests:
	@test -n "$(TEST_SRC)" || { echo "error: no core/tests/*.cpp found; refusing to report a pass" >&2; exit 1; }

test: build/slg_tests
	build/slg_tests

test-ubsan: build/ubsan/slg_tests
	UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 build/ubsan/slg_tests

test-asan: build/asan/slg_tests
	ASAN_OPTIONS=detect_leaks=$(ASAN_DETECT_LEAKS):detect_stack_use_after_return=1:strict_string_checks=1:check_initialization_order=1 \
	UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 build/asan/slg_tests

forbidden:
	$(PYTHON) -I -B scripts/check_forbidden.py

scripts-test:
	$(PYTHON) -I -B -m unittest discover -s scripts -p 'test_*.py'

# golden-oracle needs no test binary: the oracle against the files (and a standalone slg_run if core/tools has one).
# golden adds the C++ side: the golden tests of slg_tests (core/tests/golden_runner.cpp, one per pair, GOLDEN/<name>).
golden-oracle: $(if $(RUN_SRC),build/slg_run)
	$(if $(wildcard tools/oracle/make_goldens.py),$(PYTHON) -I -B tools/oracle/make_goldens.py --check)
	$(PYTHON) -B scripts/check_golden.py --golden-dir '$(GOLDEN_DIR)' --oracle-cmd '$(ORACLE_CMD)'$(if $(RUN_SRC), --runner-cmd '$(RUN_CMD)')

golden: golden-oracle build/slg_tests
	@echo "== C++ golden tests: slg_tests '$(GOLDEN_TESTS)'"
	@out=$$(SLG_GOLDEN_DIR='$(GOLDEN_DIR)' build/slg_tests '$(GOLDEN_TESTS)') || { printf '%s\n' "$$out"; exit 1; }; \
	  printf '%s\n' "$$out"; \
	  if grep -q '^SKIP' <<<"$$out"; then echo "error: a golden test was skipped" >&2; exit 1; fi; \
	  grep -Eq '^TESTS: [1-9][0-9]* passed' <<<"$$out" || { echo "error: no golden test ran" >&2; exit 1; }

mutants:
	CXX='$(CXX)' OPT='$(OPT)' EXTRA_FLAGS='$(EXTRA_FLAGS)' EXTRA_LDFLAGS='$(EXTRA_LDFLAGS)' bash scripts/run_mutants.sh $(MUTANTS)

bench: build/slg_bench
	build/slg_bench

selfcheck: build/slg_selfcheck
	build/slg_selfcheck

build/slg_bench: $(patsubst %.cpp,$(OBJDIR_default)/%.o,$(LIB_SRC) $(BENCH_SRC)) | guard-lib
	@test -n "$(BENCH_SRC)" || { echo "error: no core/bench/*.cpp found" >&2; exit 1; }
	$(CXX) $(LDFLAGS_default) $(filter %.o,$^) -o $@

build/slg_selfcheck: $(patsubst %.cpp,$(OBJDIR_default)/%.o,$(LIB_SRC) $(SELF_SRC)) | guard-lib
	@test -n "$(SELF_SRC)" || { echo "error: no core/selfcheck/*.cpp found" >&2; exit 1; }
	$(CXX) $(LDFLAGS_default) $(filter %.o,$^) -o $@

build/slg_run: $(patsubst %.cpp,$(OBJDIR_default)/%.o,$(LIB_SRC) $(RUN_SRC)) | guard-lib
	@test -n "$(RUN_SRC)" || { echo "error: no core/tools/*.cpp found" >&2; exit 1; }
	$(CXX) $(LDFLAGS_default) $(filter %.o,$^) -o $@

# libFuzzer targets need clang; each one is built from all of core/src so the library is instrumented too.
fuzz: guard-lib
	@if [ -z "$(FUZZ_SRC)" ]; then \
	  if [ -n "$(FUZZ_ALLOW_EMPTY)" ]; then echo "make fuzz: no core/fuzz/fuzz_*.cpp targets (allowed)"; exit 0; fi; \
	  echo "error: no core/fuzz/fuzz_*.cpp targets found; refusing to report a pass" >&2; exit 1; \
	fi
	@for src in $(FUZZ_SRC); do \
	  name=$$(basename $$src .cpp); \
	  mkdir -p build/fuzz/artifacts build/fuzz/corpus-$$name; \
	  echo "== $$name ($(FUZZ_SECONDS) s)"; \
	  $(FUZZ_CXX) $(STD_FLAGS) $(WARN_FLAGS) $(SAN_FLAGS) -fsanitize=fuzzer,address,undefined $(EXTRA_FLAGS) \
	    -Icore/include -Icore/src $(if $(wildcard core/host/include),-Icore/host/include) \
	    $(LIB_SRC) $$src -o build/fuzz/$$name; \
	  seeds=; [ -d core/fuzz/corpus/$$name ] && seeds=core/fuzz/corpus/$$name; \
	  build/fuzz/$$name -max_total_time=$(FUZZ_SECONDS) -artifact_prefix=build/fuzz/artifacts/$$name- build/fuzz/corpus-$$name $$seeds; \
	done

clean:
	rm -rf build/obj build/ubsan build/asan build/fuzz build/mutants build/golden build/.flags \
	  build/slg_tests build/slg_bench build/slg_selfcheck build/slg_run

help:
	@echo "targets: all test test-ubsan test-asan forbidden scripts-test golden-oracle golden mutants bench selfcheck fuzz clean"
	@echo "variables: CXX OPT EXTRA_FLAGS SLG_CASES MUTANTS GOLDEN_DIR ORACLE_CMD RUN_CMD FUZZ_SECONDS (see core/README.md)"

FORCE:
