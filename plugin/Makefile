RACK_DIR ?= ../../Rack-SDK

# The DROID engine is compiled into the plugin directly (one engine, two builds).
ENGINE_SOURCES := $(wildcard ../engine/src/*.cpp) \
                  $(wildcard ../engine/circuits/*.cpp) \
                  $(wildcard ../engine/gen/*.cpp)

FLAGS += -I../engine

SOURCES += $(wildcard src/*.cpp) $(ENGINE_SOURCES)
SOURCES += $(wildcard src/uatbridge/*.cpp)
FLAGS += -DVCVOID_GIT_HASH=\"$(shell git -C .. rev-parse --short HEAD)\"
# Tag if the build is exactly at one, else the branch name; empty on a
# detached HEAD that isn't at a tag. Shown in every module's context menu.
FLAGS += -DVCVOID_GIT_REF=\"$(shell git -C .. describe --tags --exact-match 2>/dev/null || git -C .. symbolic-ref --short -q HEAD)\"
DISTRIBUTABLES += res

include $(RACK_DIR)/plugin.mk

# plugin.mk's compile.mk appends -std=c++11 after our flags, so we must strip it
# and force C++17 here (after the include) or the engine sources won't compile.
CXXFLAGS := $(filter-out -std=c++11,$(CXXFLAGS)) -std=c++17
