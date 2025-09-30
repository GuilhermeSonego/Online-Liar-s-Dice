#ifndef _server_h
#define _server_h

#include <pthread.h>
#include <semaphore.h>
#include <atomic>
#include <vector>
#include "jogo.h"

#define TAMANHO_MAOS 3
#define NUMERO_JOGADORES 1
#define LIMITE_FACE 6
#define MENSAGEM_CLIENTE sizeof(Acao_resposta)
#define PORTA 4612

typedef struct _estrutura_acoes
{
    int acao_escolhida;
    int valores_acao[2];
    bool novo_valor;

} Acao_resposta;

typedef struct _estado_global
{
    std::vector<pthread_t> threads_sistema;
    std::vector<int> sockets_jogadores;
    std::vector<int> sockets_servidor;

} Estado_global;

typedef struct _estado_thread
{
    unsigned int indice_jogador;
    int socket;
    sem_t* semaforo_jogador, *semaforo_servidor;
    Acao_resposta* acao;

} Estado_thread;

enum Acoes
{
    AUMENTAR_APOSTA,
    DUVIDAR,
    CRAVAR
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
void cleanup_servidor(int sinal);
pthread_mutex_t mutex_acao = PTHREAD_MUTEX_INITIALIZER;


//DEBUG
pthread_cond_t condicao_acao = PTHREAD_COND_INITIALIZER;
sem_t semaforo_acao;
void* thread_debug_jogador(void* parametros);
//FIM

//Função que define a subrotina associada a cada jogador.
void* main_jogador(void* parametros);

//Funções para auxiliar na comunicação entre sockets.
void desserializa_mensagem(const char* buffer, Acao_resposta &acao);

//Funções associadas ao jogo. São utilitárias e auxiliam no fluxo do processamento do jogo pelo servidor.
std::vector<unsigned int> gera_ordem_aleatoria(unsigned int tamanho);
int executar_acao(Acao_resposta acao, Estado &estado_jogo, unsigned int indice_jogador_atual);
bool esperar_acao(Acao_resposta &acao_recebida, Estado &estado_jogo, sem_t &semaforo_servidor, sem_t &semaforo_jogador_atual, unsigned int jogador_atual);
bool checa_aposta_valida(const Aposta &nova_aposta, const Estado &estado_jogo);
int checa_dados_mesa(const Estado &estado_jogo);
void envia_estado_inicial_mesa(const Estado &estado_jogo);
void envia_estado_mesa(const Estado &estado_jogo);

#endif //_server_h