#ifndef _server_h
#define _server_h

#include <pthread.h>
#include <semaphore.h>
#include <atomic>
#include <vector>
#include <memory>
#include "jogo.h"

// Definição de vários parâmetros do jogo como tamanho inicial de mãos, número de jogadores e a porta do servidor.
#define TAMANHO_MAOS 3
#define NUMERO_JOGADORES 3
#define LIMITE_FACE 6
#define PORTA 4737

// Estrutura que carrega as ações tomadas por jogadores.
typedef struct _estrutura_acoes
{
    int acao_escolhida;
    unsigned int valores_acao[2];

} Acao_resposta;

// Estrutura de controle de cada jogador, agrupando valores de controle.
typedef struct __jogador_cliente
{
    pthread_t thread;
    sem_t semaforo;
    int socket;
    unsigned int numero;
    bool conectado;
} Jogador_cliente;

// Estrutura de controle do servidor. Jogadores são armazenados por ponteiros para manter localidade de dados.
typedef struct _estado_global
{
    pthread_t thread_socket;
    sem_t semaforo_servidor;
    int socket_servidor;
    std::vector <std::unique_ptr<Jogador_cliente>> jogadores;
} Estado_global;

// Estrutura de parâmetros a serem passados individualmente a cada thread.
typedef struct _estado_thread
{
    unsigned int numero_jogador;
    int socket;
    sem_t* semaforo_jogador;
    Acao_resposta* acao;
    bool* ligada;

} Estado_thread;

// Ações possíveis a serem tomadas por jogadores.
enum Acoes
{
    NADA = 0,
    AUMENTAR_APOSTA = 1,
    DUVIDAR = 2,
    CRAVAR = 3,
    SAIR = 4
};

// Valores nomeados de cada dado.
enum Valores_Dados
{
    BAGO = 1,
    DUQUE = 2,
    TERNO = 3,
    QUADRA = 4,
    QUINA = 5,
    SENA = 6
};

// Ramificações possíveis dos resultados de análise da mesa.
enum Possibilidades
{
    EXATOS = 0,
    MENOS = 1,
    MAIS = 2,
    ERRO = 3
};

// Variáveis e funções de controle global do processo. Serão necessárias para desalocar memória e encerrar o programa corretamente.
Estado_global estado_servidor = {};
std::atomic<bool> servidor_rodando(true);
pthread_mutex_t mutex_acao = PTHREAD_MUTEX_INITIALIZER;
void cleanup_servidor();
void sinalizar_desligamento(int sinal);

// Funções de checagem e handling de conexões.
bool checa_conexoes(Estado &estado_jogo, unsigned int jogador_atual);
void desligar_jogador_cliente(unsigned int numero);

// Função que define a subrotina associada a cada jogador.
void* main_jogador(void* parametros);
void* comunicacao_socket(void* parametros);

// Funções de broadcast de estado do jogo
void envia_estado_inicial_mesa(const Estado &estado_jogo);
void envia_estado_mesa(const Estado &estado_jogo);

// Funções associadas ao jogo. São utilitárias e auxiliam no fluxo do processamento do jogo pelo servidor.
void gera_ordem_aleatoria();
unsigned int executar_acao(Acao_resposta acao, Estado &estado_jogo, unsigned int numero_jogador_atual, unsigned int &turno_jogador);
bool checa_acao_valida(Acao_resposta& acao,const Estado &estado_jogo);
bool esperar_acao(Acao_resposta &acao_recebida, Estado &estado_jogo, Jogador_cliente* jogador_atual);
bool checa_aposta_valida(const Aposta &nova_aposta, const Estado &estado_jogo);
int checa_dados_mesa(const Estado &estado_jogo);
unsigned int checa_vencedor(const Estado &estado_jogo);
void reseta_estado(Estado& estado_jogo);
Jogador_cliente* busca_jogador(unsigned int numero);

#endif //_server_h