#ifndef _server_h
#define _server_h

#include <pthread.h>
#include <semaphore.h>
#include <atomic>
#include <vector>
#include <memory>
#include "jogo.h"

#define TAMANHO_MAOS 3
#define NUMERO_JOGADORES 4
#define LIMITE_FACE 6
#define MENSAGEM_CLIENTE sizeof(Acao_resposta)
#define PORTA 4621

typedef struct _estrutura_acoes
{
    int acao_escolhida;
    int valores_acao[2];
    bool novo_valor;

} Acao_resposta;

typedef struct __jogador_cliente
{
    pthread_t thread;
    sem_t semaforo;
    int socket;
    unsigned int numero;
    bool conectado;
} Jogador_cliente;

typedef struct _estado_global
{
    pthread_t thread_socket;
    sem_t semaforo_servidor;
    int socket_servidor;
    std::vector <std::unique_ptr<Jogador_cliente>> jogadores;
} Estado_global;

typedef struct _estado_thread
{
    unsigned int numero_jogador;
    int socket;
    sem_t* semaforo_jogador;
    Acao_resposta* acao;
    bool* ligada;

} Estado_thread;

enum Acoes
{
    AUMENTAR_APOSTA = 1,
    DUVIDAR = 2,
    CRAVAR = 3,
    SAIR = 4
};

enum Valores_Dados
{
    BAGO = 1,
    DUQUE = 2,
    TERNO = 3,
    QUADRA = 4,
    QUINA = 5,
    SENA = 6
};

enum Possibilidades
{
    EXATOS = 0,
    MENOS = 1,
    MAIS = 2,
    ERRO = 3
};

//Variáveis e funções de controle global do processo. Serão necessárias para desalocar memória e encerrar o programa corretamente.
Estado_global estado_servidor = {};
std::atomic<bool> servidor_rodando(true);
pthread_mutex_t mutex_acao = PTHREAD_MUTEX_INITIALIZER;
void cleanup_servidor();
void sinalizar_desligamento(int sinal);

//Função que define a subrotina associada a cada jogador.
void* main_jogador(void* parametros);
void* comunicacao_socket(void* parametros);

//Funções associadas ao jogo. São utilitárias e auxiliam no fluxo do processamento do jogo pelo servidor.
std::vector<unsigned int> gera_ordem_aleatoria(unsigned int tamanho);
int executar_acao(Acao_resposta acao, Estado &estado_jogo, unsigned int indice_jogador_atual);
bool esperar_acao(Acao_resposta &acao_recebida, Estado &estado_jogo, sem_t &semaforo_servidor, sem_t &semaforo_jogador_atual, unsigned int jogador_atual);
bool checa_aposta_valida(const Aposta &nova_aposta, const Estado &estado_jogo);
int checa_dados_mesa(const Estado &estado_jogo);
void envia_estado_inicial_mesa(const Estado &estado_jogo);
void envia_estado_mesa(const Estado &estado_jogo);
bool checa_conexoes(Estado &estado_jogo, unsigned int jogador_atual);
void desligar_jogador_cliente(unsigned int indice);


#endif //_server_h