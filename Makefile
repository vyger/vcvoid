CXX ?= clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -g -O1 -Iengine -Itests -Iplugin/src
BUILD := build

ENGINE_SRC := $(wildcard engine/src/*.cpp) $(wildcard engine/circuits/*.cpp) $(wildcard engine/gen/*.cpp)
UNIT_SRC   := $(wildcard tests/unit/*.cpp)
RUNNER_SRC := $(wildcard tests/runner/*.cpp)
GOLDENS    := $(shell find tests/golden -name '*.gold' 2>/dev/null | sort)

.PHONY: all test unittests goldens gen clean crosscheck layoutcheck artcheck

VENDOR := tools/droidcheck/vendor/droidforge/droidforge
# The Forge's main/tuning.h gates a few constants behind Qt's platform macros
# (Q_OS_MAC/WIN/LINUX); define the right one so it compiles outside Qt.
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LAYOUTCHECK_DEFS := -DQ_OS_MAC
else
LAYOUTCHECK_DEFS := -DQ_OS_LINUX
endif

all: test

# -DVCVOID_VERIFY_MEMO=1 is test-build-only: it turns on the dynamic
# pointer=>content invariant check in Circuit::memoSlot (engine/src/circuit.cpp).
# Golden runs + unit tests are the dynamic proof that the stable-pointer
# invariant actually holds across every circuit; the shipped Rack plugin
# builds with the Rack SDK's own flags (-O3, no -DNDEBUG — see compile.mk),
# so it must NOT carry this check onto the audio thread, where a mismatch
# would abort() a running Rack session instead of failing a test.
$(BUILD)/unittests: $(ENGINE_SRC) $(UNIT_SRC) $(wildcard engine/src/*.hpp) $(wildcard engine/gen/*.hpp) $(wildcard tests/unit/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -DVCVOID_VERIFY_MEMO=1 $(ENGINE_SRC) $(UNIT_SRC) -o $@

$(BUILD)/droidtest: $(ENGINE_SRC) $(RUNNER_SRC) $(wildcard engine/src/*.hpp) $(wildcard engine/gen/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -DVCVOID_VERIFY_MEMO=1 $(ENGINE_SRC) $(RUNNER_SRC) -o $@

unittests: $(BUILD)/unittests
	$(BUILD)/unittests

goldens: $(BUILD)/droidtest
	@if [ -n "$(GOLDENS)" ]; then $(BUILD)/droidtest $(GOLDENS); else echo "no goldens yet"; fi

test: unittests goldens layoutcheck artcheck

gen:
	python3 tools/jackgen/jackgen.py
	python3 tools/scalegen/scalegen.py

clean:
	rm -rf $(BUILD)

crosscheck:
	tools/crosscheck.sh

layoutcheck:
	@if [ -d "$(VENDOR)/modules" ]; then \
		tests/layoutcheck/import.sh && \
		$(CXX) $(CXXFLAGS) $(LAYOUTCHECK_DEFS) -Iplugin/src -Ibuild/layoutcheck-src \
			build/layoutcheck-src/*.cpp tests/layoutcheck/main.cpp \
			-o $(BUILD)/layoutcheck && $(BUILD)/layoutcheck; \
	else echo "layoutcheck: vendor checkout missing, SKIPPED (run tools/droidcheck/build.sh)"; fi

$(BUILD)/layoutdump: tests/layoutcheck/dump_json.cpp plugin/src/Layout.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Iplugin/src tests/layoutcheck/dump_json.cpp -o $@

artcheck: $(BUILD)/layoutdump
	@if python3 -c 'import cv2' 2>/dev/null; then \
		$(BUILD)/layoutdump > $(BUILD)/layout.json && \
		python3 tools/check_art_alignment.py $(BUILD)/layout.json plugin/res/faceplates; \
	else echo "artcheck: opencv-python not installed, SKIPPED (pip3 install opencv-python)"; fi

$(BUILD)/patchsmoke: $(ENGINE_SRC) tests/tools/patchsmoke.cpp $(wildcard engine/src/*.hpp) $(wildcard engine/gen/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(ENGINE_SRC) tests/tools/patchsmoke.cpp -o $@

patchsmoke: $(BUILD)/patchsmoke

.PHONY: patchsmoke

$(BUILD)/eqcheck: $(ENGINE_SRC) tests/tools/eqcheck.cpp $(wildcard engine/src/*.hpp) $(wildcard engine/gen/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(ENGINE_SRC) tests/tools/eqcheck.cpp -o $@

eqcheck: $(BUILD)/eqcheck

.PHONY: eqcheck

# bench: headless per-circuit CPU profiler (issue #5). -O2 overrides the
# global -O1 for this target only — perf measurement wants an optimized
# build; the rest of the tree stays at -O1 for fast edit/test cycles. -DNDEBUG
# is bench-only (drops any plain assert()s elsewhere in the tree) and, unlike
# unittests/droidtest above, this rule deliberately does NOT define
# VCVOID_VERIFY_MEMO: the bench should measure the same fast path the shipped
# Rack plugin runs (no pointer-invariant check on the hit path in
# Circuit::memoSlot, circuit.cpp), while unittests/droidtest keep the check
# active to prove the invariant on every golden.
$(BUILD)/bench: $(ENGINE_SRC) tools/bench/bench.cpp $(wildcard engine/src/*.hpp) $(wildcard engine/gen/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -O2 -DNDEBUG $(ENGINE_SRC) tools/bench/bench.cpp -o $@

bench: $(BUILD)/bench

.PHONY: bench
