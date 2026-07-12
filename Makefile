# --- Expected project structure ---
# .
# ├── bin
# │   ├── test_runner
# │   ├── main
# │   ├── lib
# │   │   ├── libx.a
# │   │   └── libx.so
# │   └── obj
# │       └── x.o
# ├── dev
# ├── lib
# ├── main.c
# ├── Makefile
# ├── src
# │   ├── x.c
# │   └── include
# │       └── x.h
# └── tests
#     └── x.test.c

# --- Project Configuration ---
-include .env
# Available env variable are:
# - PROJECT_NAME= name
# - USER_SHARED_LIBS= lib1 lib2 lib3

# --- Build Tools ---
CC := gcc
AR := ar
# Check the operating system to set correct linker flags for shared libraries
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Darwin)
    # macOS linker flags
    LIB_EXT := dylib
    SHARED_LDFLAGS := -Wl,-install_name,@rpath/lib$(PROJECT_NAME).$(LIB_EXT)
    EXE_RPATH_LDFLAGS = -Wl,-rpath,@loader_path/lib -Wl,-rpath,@loader_path/../lib -Wl,-rpath,$(INSTALL_LIB_DIR)
else
    # Linux linker flags
    LIB_EXT := so
    SHARED_LDFLAGS := -Wl,-soname,lib$(PROJECT_NAME).$(LIB_EXT)
    EXE_RPATH_LDFLAGS = -Wl,-rpath,$(INSTALL_LIB_DIR)
endif

# --- AFL++ Fuzzing Tools (Used inside Docker) ---
AFL_CC := afl-clang-lto
AFL_CFLAGS := -Wall -Wextra -Werror -g -fPIC -O3

# --- Build Options ---
BASE_CFLAGS := -Wall -Wextra -Werror -fvisibility=hidden -fPIC
ifeq ($(OPTION), prod)
  CFLAGS := $(BASE_CFLAGS) -O2
else ifeq ($(OPTION), dev)
  CFLAGS := $(BASE_CFLAGS) -g
else ifeq ($(OPTION), test)
  CFLAGS := $(BASE_CFLAGS) -g -Wno-implicit-function-declaration
else
  CFLAGS := $(BASE_CFLAGS) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
endif

# --- Directories ---
SRC_DIR := src
BIN_DIR := bin
OBJ_DIR := $(BIN_DIR)/obj
LIB_DIR := $(BIN_DIR)/lib
TEST_DIR := tests

PREFIX := /usr/local
INSTALL_LIB_DIR := $(PREFIX)/lib
INSTALL_INCLUDE_DIR := $(PREFIX)/include/$(PROJECT_NAME)
FUZZ_LIB := $(LIB_DIR)/lib$(PROJECT_NAME)_fuzz.a

# --- Source Files and Objects ---
SRC_FILES := $(shell find $(SRC_DIR) -type f -name "*.c")
TEST_SRC_FILES := $(wildcard $(TEST_DIR)/*.c)

# --- Optional YAML parsing support (default: YAML=1) ---
YAML ?= 1
ifeq ($(YAML), 0)
  SRC_FILES := $(filter-out src/parser/yaml_lexer.c src/parser/yaml_parser.c, $(SRC_FILES))
  TEST_SRC_FILES := $(filter-out tests/yaml.test.c, $(TEST_SRC_FILES))
else
  CFLAGS += -DJSONV_YAML_SUPPORT
endif

OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
TEST_OBJS := $(patsubst $(TEST_DIR)/%.c,$(OBJ_DIR)/test_%.o,$(TEST_SRC_FILES))
FUZZ_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/fuzz_%.o,$(SRC_FILES))

# --- Libraries ---
TEST_LIBS := criterion
LINK_USER_SHARED_LIBS := $(patsubst %, -l%, $(USER_SHARED_LIBS))
LINK_TEST_LIBS := $(patsubst %, -l%, $(TEST_LIBS))
# Corrected find commands to search within the lib directory
STATIC_LIB_BIN_PATHS := $(shell find lib -maxdepth 2 -type d -name "bin")
STATIC_LIB_INCLUDE_PATHS := $(shell find lib -maxdepth 2 -type d -name "include")
LDFLAGS := -L $(LIB_DIR) -L/usr/local/lib $(patsubst %, -L%, $(STATIC_LIB_BIN_PATHS))
LDLIBS := -l$(PROJECT_NAME) $(LINK_USER_SHARED_LIBS)

SRC_INCLUDE_PATHS := $(shell find $(SRC_DIR) -type d)
INC_FLAGS := $(patsubst %, -I%, $(SRC_INCLUDE_PATHS)) -I/usr/local/include $(patsubst %, -I%, $(STATIC_LIB_INCLUDE_PATHS))

MAIN_APP_STATIC := $(BIN_DIR)/main
MAIN_APP_DYNAMIC := $(BIN_DIR)/main_d
TEST_APP := $(BIN_DIR)/test_runner
FUZZ_APP := $(BIN_DIR)/fuzz

# --- Phony Targets ---
.PHONY: all static shared test main_d run run_test clean install uninstall bear dirs main run_d fuzz run_fuzz start_afl build_afl clean_afl inspect

# --- Main Targets ---
all: static

bear: clean dirs
	@echo "Generating compile_commands.json..."
	@bear -- $(MAKE) all
	@echo "compile_commands.json generated."

# --- Directories ---
dirs:
	@mkdir -p $(OBJ_DIR) $(LIB_DIR) $(BIN_DIR)

# --- Build Static Library ---
static: $(LIB_DIR)/lib$(PROJECT_NAME).a
$(LIB_DIR)/lib$(PROJECT_NAME).a: $(OBJS) | dirs
	@echo "[AR] $@"
	@$(AR) rcs $@ $^

fuzz: $(FUZZ_LIB) $(FUZZ_APP)

# --- Build Fuzz Static Library ---
$(FUZZ_LIB): $(FUZZ_OBJS) | dirs
	@echo "[AR] Building Fuzzing Library $@"
	@$(AR) rcs $@ $^

# --- Build Shared Library ---
shared: $(LIB_DIR)/lib$(PROJECT_NAME).$(LIB_EXT)
$(LIB_DIR)/lib$(PROJECT_NAME).$(LIB_EXT): $(OBJS) | dirs
	@echo "[CC-shared] $@"
	@$(CC) -shared $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LINK_USER_SHARED_LIBS) $(SHARED_LDFLAGS)

# --- Main Executable (Dynamic Link) ---
main_d: $(MAIN_APP_DYNAMIC)
$(MAIN_APP_DYNAMIC): $(OBJ_DIR)/main.o $(LIB_DIR)/lib$(PROJECT_NAME).$(LIB_EXT) | dirs
	@echo "[CC] Linking DYNAMIC $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LINK_USER_SHARED_LIBS) $(EXE_RPATH_LDFLAGS)

# --- Main Executable (Static Link) ---
main: static $(MAIN_APP_STATIC) # Ensure the static library is built first
$(MAIN_APP_STATIC): $(OBJ_DIR)/main.o $(LIB_DIR)/lib$(PROJECT_NAME).a | dirs
	@echo "[CC] Linking STATIC $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LINK_USER_SHARED_LIBS)

# --- Fuzz Executable (AFL++ Link) ---
$(FUZZ_APP): $(OBJ_DIR)/fuzz.o $(FUZZ_LIB) | dirs
	@echo "[AFL++] Linking FUZZER $@"
	@$(AFL_CC) $(AFL_CFLAGS) -o $@ $^ $(LDFLAGS) $(LINK_USER_SHARED_LIBS)

# --- New Compile Rule for Instrumented Library Objects ---
$(OBJ_DIR)/fuzz_%.o: $(SRC_DIR)/%.c | dirs
	@mkdir -p $(dir $@)
	@echo "[AFL++-FUZZ] $<"
	@$(AFL_CC) $(AFL_CFLAGS) $(INC_FLAGS) -c $< -o $@

# --- Test Executable ---
test: static $(TEST_APP)
$(TEST_APP): $(TEST_OBJS) $(LIB_DIR)/lib$(PROJECT_NAME).a | dirs
	@echo "[CC] Linking $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LINK_TEST_LIBS) $(LINK_USER_SHARED_LIBS)

# --- Compile Rules ---
$(OBJ_DIR)/fuzz.o: fuzz.c | dirs
	@echo "[AFL++-TARGET] $<"
	@$(AFL_CC) $(AFL_CFLAGS) $(INC_FLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | dirs
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) $(INC_FLAGS) -c $< -o $@

$(OBJ_DIR)/main.o: main.c | dirs
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) $(INC_FLAGS) -c $< -o $@

$(OBJ_DIR)/test_%.o: $(TEST_DIR)/%.c | dirs
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) $(INC_FLAGS) -c $< -o $@

# --- Install/Uninstall ---
install: static shared
	@mkdir -p $(DESTDIR)$(INSTALL_LIB_DIR) $(DESTDIR)$(INSTALL_INCLUDE_DIR)
	@cp $(LIB_DIR)/*.a $(LIB_DIR)/*.$(LIB_EXT) $(DESTDIR)$(INSTALL_LIB_DIR)/
	@cp -R $(SRC_DIR)/include/* $(DESTDIR)$(INSTALL_INCLUDE_DIR)/ | true
	@echo "Installed to $(DESTDIR)$(INSTALL_INCLUDE_DIR)"
ifeq ($(UNAME_S), Linux)
	@echo "Updating shared library cache..."
	@-ldconfig 2>/dev/null || sudo ldconfig 2>/dev/null || echo "Warning: Could not run ldconfig. You may need to run 'sudo ldconfig' manually."
endif

uninstall:
	@rm -f $(DESTDIR)$(INSTALL_LIB_DIR)/lib$(PROJECT_NAME).a
	@rm -f $(DESTDIR)$(INSTALL_LIB_DIR)/lib$(PROJECT_NAME).$(LIB_EXT)
	@rm -rf $(DESTDIR)$(INSTALL_INCLUDE_DIR)
	@echo "Uninstalled from $(PREFIX)"

# --- Run Targets ---
run: $(MAIN_APP_STATIC)
	@$(MAIN_APP_STATIC)

run_d: $(MAIN_APP_DYNAMIC)
	@$(MAIN_APP_DYNAMIC)

run_test: $(TEST_APP)
	@MallocNanoZone=0 $(TEST_APP) || true

run_fuzz: $(FUZZ_APP)
	@mkdir -p output_fuzz local_seed_corpus
	@echo "Starting AFL++ Fuzzing. Use Ctrl+C to stop."
	@afl-fuzz -i seed_corpus -o output_fuzz -- $(FUZZ_APP) @@

resume_fuzz: $(FUZZ_APP)
	@echo "Resume AFL++ Fuzzing. Use Ctrl+C to stop."
	@afl-fuzz -i - -o output_fuzz -- $(FUZZ_APP) @@

build_afl:
	@echo "Build AFL++ in docker"
	docker build -t afl-jq .

start_afl:
	@echo "Start AFL++ in docker (reusing container)"
	@docker start -i afl-jq || docker run -ti --name afl-jq -v $(shell pwd):/src afl-jq

inspect:
	@echo "Inspect exposed symbols"
	@nm -gU $(LIB_DIR)/lib$(PROJECT_NAME).$(LIB_EXT)

# --- Clean Targets ---
clean:
	@echo "Clean targets"
	@rm -rf $(BIN_DIR)

clean_afl:
	@echo "Removing AFL++ docker container"
	@docker rm -f afl-jq || true
