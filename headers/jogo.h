#include <vector>
#include <iostream>
#include <random>

class Dado
{
    private:

    int _valor;

    public:

    //Construtores
    Dado(int numero = 0) : _valor(numero) {}

    //Métodos 
    void aleatorizar_valor(unsigned int limite_face);

    //Getters
    const int get_valor() const {return this->_valor;}

    //Setters

    //Sobrecarga de Operadores
    Dado& operator=(const Dado& outro) {if (this != &outro) this->_valor = outro._valor; return *this;}
    auto operator<=>(const Dado& outro) const = default;
};

class Mao
{
    private:

    unsigned int _limite_face;
    std::vector<Dado> _dados;

    public:
    //Construtores
    Mao(unsigned int numero = 0, unsigned int limite = 0) : _limite_face(limite) {this->_dados = std::vector<Dado>(numero);}

    //Métodos
    void aleatorizar(unsigned int limite_face);
    void perder_dado() {if(!this->_dados.empty())this->_dados.pop_back();}

    //Getters
    const std::vector<Dado>& get_dados() const {return this->_dados;}
    const unsigned int get_numero_dados() const {return _dados.size();}
    const unsigned int get_limite_face() const {return this->_limite_face;}

    //Setters
    void set_tamanho_mao(unsigned int tamanho = 0) {this->_dados.resize(tamanho);}
    
};

class Jogador
{
    private:

    unsigned int _numero;
    Mao _mao;

    public:

    //Construtores
    Jogador(unsigned int numero_jogador = 0, unsigned int tamanho_mao = 0) : _numero(numero_jogador), _mao(tamanho_mao) {}

    //Métodos
    void aleatorizar_mao(unsigned int limite_face) {this->_mao.aleatorizar(limite_face);}
    void perder_dado() {this->_mao.perder_dado();}

    //Getters
    const Mao& get_mao() const {return this->_mao;}
    const unsigned int get_numero() const {return this->_numero;}

    //Setters
    void set_mao(Mao mao_nova) {this->_mao = mao_nova;}
    void set_numero(unsigned int novo_numero) {this->_numero = novo_numero;}
};

class Aposta
{
    private:

    unsigned int _numero_dados;
    Dado _dado;

    public:
    //Construtores
    Aposta(unsigned int numero_dados=0, Dado tipo_dados = Dado()) : _numero_dados(numero_dados), _dado(tipo_dados) {}

    //Métodos
    void adiciona_dado() {this->_numero_dados++;}

    //Getters
    const Dado& get_dados() const {return this->_dado;}
    const unsigned int get_numero_dados() const {return this->_numero_dados;}

    //Setters
    void set_dados(Dado dado_novo) {this->_dado = dado_novo;}
    void set_numero_dados(unsigned int novo_numero) {this->_numero_dados = novo_numero;}

    //Sobrecarga de operadores
    bool operator==(const Aposta& outra_aposta) const {return _numero_dados == outra_aposta._numero_dados && _dado == outra_aposta._dado;}
    bool operator>=(const Aposta& outra_aposta) const {return _numero_dados >= outra_aposta._numero_dados;}
    bool operator>(const Aposta& outra_aposta) const {return _numero_dados > outra_aposta._numero_dados && _dado == outra_aposta._dado;}
};

class Jogada
{
    private:

    Aposta _aposta_feita;
    int _acao_tomada;

    public:
    //Construtores
    Jogada(Aposta aposta_da_jogada = Aposta(), int acao = 0) : _aposta_feita(aposta_da_jogada), _acao_tomada(acao) {}

    //Getters
    const Aposta& get_aposta() const {return this->_aposta_feita;}
    const int get_acao() const {return this->_acao_tomada;} 

    //Setters

    friend std::ostream& operator<<(std::ostream& os, const Jogada& jogada) {
        os << "Numero de dados: " << jogada._aposta_feita.get_numero_dados()
           << " | Valor dos dados: " << jogada._aposta_feita.get_dados().get_valor();
        return os;
    }

};

class Estado
{
    private:
    unsigned int _tamanho_maos, _turno_atual, _quantia_dados; //_jogador_aposta_atual;
    std::vector<Jogador> _jogadores;
    std::vector<Jogada> _jogadas, _buffer_jogadas;
    Aposta _aposta_atual;

    public:
    unsigned int _jogador_aposta_atual;
    //Construtores
    Estado(unsigned int numero_jogadores=0, unsigned int tamanho_maos=0);

    //Métodos
    void aleatorizar_maos(unsigned int quantidade_lados);
    void tirar_dado_jogador(unsigned int indice_jogador) {this->_jogadores[indice_jogador].perder_dado(); _quantia_dados--;}
    void tirar_dado_ultimo_jogador() {this->tirar_dado_jogador(_jogador_aposta_atual);}
    void resetar_ultimas_jogadas() {this->_buffer_jogadas.clear();}

    //Getters
    const unsigned int get_turno_atual() const {return _turno_atual;}
    const unsigned int get_tamanho_maos() const {return _tamanho_maos;}
    const unsigned int get_numero_jogadores() const {return this->_jogadores.size();}
    const Aposta& get_aposta() const {return _aposta_atual;}
    const std::vector<Jogada>& get_lista_jogadas() const {return _jogadas;}
    const std::vector<Jogada>& get_ultimas_jogadas() const {return _buffer_jogadas;}
    const std::vector<Jogador>& get_lista_jogadores() const {return _jogadores;}
    const unsigned int get_dados_mesa() const {return _quantia_dados;}

    //Setters
    void set_turno_atual(unsigned int novo_valor) {this->_turno_atual = novo_valor;}
    void set_tamanho_maos(unsigned int novo_valor) {this->_tamanho_maos = novo_valor;}
    void set_aposta(Aposta nova_aposta, unsigned int jogador_responsavel) {this->_aposta_atual = nova_aposta; this->_jogador_aposta_atual = jogador_responsavel;}
    void adiciona_jogada(Jogada nova_jogada) {this->_jogadas.push_back(nova_jogada); this->_buffer_jogadas.push_back(nova_jogada);}
    void set_jogadores(std::vector<Jogador> novos_jogadores) {this->_jogadores = novos_jogadores;}

    //Sobrecarga de Operadores
    void operator++() {this->_turno_atual++;}
    void operator++(int) {this->_turno_atual++;}

    // DEBUG
    void print_obj() const;
    void print_tudo() const;
};