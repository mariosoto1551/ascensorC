# =============================================================================
#  Simulador de Ascensor Concurrente -- UPB, Sistemas Operativos
# =============================================================================
#  Objetivos disponibles:
#    make            compila el simulador en bin/ascensor
#    make run        compila y ejecuta
#    make debug      compila con simbolos y sin optimizar
#    make tsan       compila con ThreadSanitizer (detector de data races)
#    make clean      borra objetos y binario
#    make help       muestra esta ayuda
# =============================================================================

# ---- Identidad --------------------------------------------------------------
TARGET   := ascensor
BIN_DIR  := bin
OBJ_DIR  := build
SRC_DIR  := src
INC_DIR  := $(SRC_DIR)/include

# ---- Toolchain --------------------------------------------------------------
CC       := gcc
CSTD     := -std=c11
WARN     := -Wall -Wextra -Wpedantic
DEFS     := -D_POSIX_C_SOURCE=200809L
CFLAGS   := $(CSTD) $(WARN) $(DEFS) -I$(INC_DIR) -O2
LDFLAGS  :=
LDLIBS   := -lpthread -lncurses

# ---- Fuentes ----------------------------------------------------------------
SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)
BIN      := $(BIN_DIR)/$(TARGET)

# ---- Reglas -----------------------------------------------------------------
.PHONY: all run debug tsan clean help
.DEFAULT_GOAL := all

all: $(BIN)

$(BIN): $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@
	@echo "==> Listo: $@"

# -MMD -MP genera los .d para recompilar si cambia un header
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BIN_DIR) $(OBJ_DIR) logs:
	@mkdir -p $@

run: all | logs
	./$(BIN)

debug: CFLAGS := $(CSTD) $(WARN) $(DEFS) -I$(INC_DIR) -O0 -g3
debug: clean all

# ThreadSanitizer: imprescindible para validar la seccion 6 del diseno logico
tsan: CFLAGS := $(CSTD) $(WARN) $(DEFS) -I$(INC_DIR) -O1 -g -fsanitize=thread
tsan: LDFLAGS += -fsanitize=thread
tsan: clean all

clean:
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "==> Limpio."

help:
	@echo "make        -> compila en $(BIN)"
	@echo "make run    -> compila y ejecuta"
	@echo "make debug  -> compila con -O0 -g3"
	@echo "make tsan   -> compila con ThreadSanitizer"
	@echo "make clean  -> borra $(OBJ_DIR)/ y $(BIN_DIR)/"

-include $(DEPS)
