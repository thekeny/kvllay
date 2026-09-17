CXX ?= g++
SRC ?= src/main.cpp
BUILD_DIR = build
TARGET = $(BUILD_DIR)/kvllay$(EXE)

CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -march=native -flto -I header -I include -I include/kvllay
LDFLAGS = 
LDLIBS = 

MALLOC ?= libc

ifeq ($(MALLOC),jemalloc)
    CXXFLAGS += -DUSE_JEMALLOC=1
    ifneq ($(shell which pkg-config 2>/dev/null),)
        CXXFLAGS += $(shell pkg-config --cflags jemalloc 2>/dev/null)
        LDLIBS += $(shell pkg-config --libs jemalloc 2>/dev/null)
    endif
    ifeq ($(findstring -ljemalloc,$(LDLIBS)),)
        LDLIBS += -ljemalloc
    endif
else ifeq ($(MALLOC),mimalloc)
    CXXFLAGS += -DUSE_MIMALLOC=1
    ifneq ($(shell which pkg-config 2>/dev/null),)
        CXXFLAGS += $(shell pkg-config --cflags mimalloc 2>/dev/null)
        LDLIBS += $(shell pkg-config --libs mimalloc 2>/dev/null)
    endif
    ifeq ($(findstring -lmimalloc,$(LDLIBS)),)
        LDLIBS += -lmimalloc
    endif
endif

WINDRES ?= windres
RC_SRC = src/resources.rc
RC_OBJ = $(BUILD_DIR)/resources.o

ifeq ($(OS),Windows_NT)
EXE = .exe
CXXFLAGS += -D _WIN32_WINNT=0x0A00
LDLIBS += -lws2_32 -lpsapi
MKDIR = cmd /C if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
REMOVE = cmd /C if exist "$(BUILD_DIR)\kvllay.exe" del /Q "$(BUILD_DIR)\kvllay.exe" && if exist "$(BUILD_DIR)\resources.o" del /Q "$(BUILD_DIR)\resources.o"
RUN = $(TARGET)
COMPILE_RC = $(WINDRES) -I . $(RC_SRC) -O coff -o $(RC_OBJ)
COMPILE = $(COMPILE_RC) && $(CXX) $(SRC) $(RC_OBJ) $(CXXFLAGS) -o $(TARGET) $(LDFLAGS) $(LDLIBS)
else
EXE =
MKDIR = mkdir -p $(BUILD_DIR)
REMOVE = rm -f $(BUILD_DIR)/kvllay $(RC_OBJ)
RUN = ./$(TARGET)
LDLIBS += -pthread
COMPILE = $(CXX) $(SRC) $(CXXFLAGS) -o $(TARGET) $(LDFLAGS) $(LDLIBS)
endif

default:
	$(MKDIR)
	$(REMOVE)
	${COMPILE}
	${RUN}

compile:
	$(MKDIR)
	$(REMOVE)
	${COMPILE}

compile-jemalloc:
	$(MAKE) compile MALLOC=jemalloc

compile-mimalloc:
	$(MAKE) compile MALLOC=mimalloc

STATIC_MALLOC_FLAGS =
ifeq ($(MALLOC),jemalloc)
    STATIC_MALLOC_FLAGS = -DUSE_JEMALLOC=1 -ljemalloc
else ifeq ($(MALLOC),mimalloc)
    STATIC_MALLOC_FLAGS = -DUSE_MIMALLOC=1 -lmimalloc
endif

static-linux:
	$(MKDIR)
	$(CXX) $(SRC) -std=c++17 -Wall -Wextra -O3 -flto -static -s $(STATIC_MALLOC_FLAGS) -I header -I include -I include/kvllay -o $(BUILD_DIR)/kvllay-linux-x86_64 -pthread

static-windows:
	$(MKDIR)
	$(WINDRES) -I . $(RC_SRC) -O coff -o $(RC_OBJ)
	$(CXX) $(SRC) $(RC_OBJ) -std=c++17 -Wall -Wextra -O3 -flto -static -static-libgcc -static-libstdc++ -s -D _WIN32_WINNT=0x0A00 $(STATIC_MALLOC_FLAGS) -I header -I include -I include/kvllay -o $(BUILD_DIR)/kvllay-windows-x86_64.exe -lws2_32 -lpsapi

run:
	${RUN}

PYTHON ?= $(shell if [ -f .venv/bin/python3 ]; then echo .venv/bin/python3; else echo python3; fi)
TEST_PORT ?= 6389

test:
	$(PYTHON) tests/run_all.py --start-server $(TEST_PORT)

clean:
	$(REMOVE)

render-benchmarks:
	$(PYTHON) docs/images/render_benchmarks.py

release:
	@bash scripts/release.sh $(VERSION)

docker-build:
	docker build -t kenyka/kvllay:latest .

docker-run:
	docker run -d --name kvllay -p 6379:6379 kenyka/kvllay:latest

docker-compose-up:
	docker compose up -d

docker-compose-down:
	docker compose down

.PHONY: default compile compile-jemalloc compile-mimalloc static-linux static-windows run test clean release docker-build docker-run docker-compose-up docker-compose-down
