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

$(BUILD)/unittests: $(ENGINE_SRC) $(UNIT_SRC) $(wildcard engine/src/*.hpp) $(wildcard engine/gen/*.hpp) $(wildcard tests/unit/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(ENGINE_SRC) $(UNIT_SRC) -o $@

$(BUILD)/droidtest: $(ENGINE_SRC) $(RUNNER_SRC) $(wildcard engine/src/*.hpp) $(wildcard engine/gen/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(ENGINE_SRC) $(RUNNER_SRC) -o $@

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
# is likewise bench-only: it drops the pointer-invariant assert in
# Circuit::memoSlot (circuit.cpp) so the bench measures the fast path with no
# assert overhead, while unittests/goldens keep asserts active to prove the
# invariant.
$(BUILD)/bench: $(ENGINE_SRC) tools/bench/bench.cpp $(wildcard engine/src/*.hpp) $(wildcard engine/gen/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -O2 -DNDEBUG $(ENGINE_SRC) tools/bench/bench.cpp -o $@

bench: $(BUILD)/bench

.PHONY: bench
