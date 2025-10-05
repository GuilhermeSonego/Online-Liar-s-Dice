# =============================
# Compilador e flags
# =============================
CXX := g++
CXXFLAGS := -Wall -std=c++23 -Iheaders -lpthread

# =============================
# Pastas
# =============================
SRC_DIR := source
OBJ_DIR := obj
BIN_DIR := exec

# =============================
# Arquivos fonte por versão
# =============================

SRC_TESTE    := $(SRC_DIR)/jogo.cpp 
SRC_CLIENTE  := $(SRC_DIR)/cliente.cpp $(SRC_DIR)/protocolo.cpp $(SRC_DIR)/jogo.cpp
SRC_SERVIDOR := $(SRC_DIR)/server.cpp $(SRC_DIR)/jogo.cpp $(SRC_DIR)/protocolo.cpp

# Transformar .cpp → .o em obj/
OBJ_TESTE    := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_TESTE))
OBJ_CLIENTE  := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_CLIENTE))
OBJ_SERVIDOR := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_SERVIDOR))

# =============================
# Alvos (executáveis finais)
# =============================
BIN_TESTE    := $(BIN_DIR)/teste
BIN_CLIENTE  := $(BIN_DIR)/cliente
BIN_SERVIDOR := $(BIN_DIR)/servidor

# =============================
# Regras principais
# =============================

all: teste cliente servidor

# Regras para cada versão
teste:    $(BIN_TESTE)
cliente:  $(BIN_CLIENTE)
servidor: $(BIN_SERVIDOR)

# Linkagem de cada versão
$(BIN_TESTE): $(OBJ_TESTE) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN_CLIENTE): $(OBJ_CLIENTE) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN_SERVIDOR): $(OBJ_SERVIDOR) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Regra genérica: compila qualquer .cpp → .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# =============================
# Criação automática das pastas
# =============================

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# =============================
# Limpeza
# =============================

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all teste cliente servidor clean
