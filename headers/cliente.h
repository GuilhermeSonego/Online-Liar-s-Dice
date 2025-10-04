#ifndef __cliente_h
#define __cliente_h

#define MAXIMO_BUFFER 50

//Estrutura para armazenar informações dos outros jogadores.
typedef struct _jogadores_adversarios
{
    unsigned int numero;
    unsigned int n_dados;
    bool conectado;

} Jogadores_adversarios;

//Estrutura para reunir principais dados do jogo necessários para o cliente.
typedef struct _estado_cliente
{
    std::vector<Jogadores_adversarios> adversarios;
    unsigned int n_dados_aposta;
    unsigned int face_dados_aposta;
    unsigned int rodada;
    unsigned int jogador_atual;
    unsigned int n_dados_total;
    unsigned int numero_jogador_local;
    std::vector<int> valor_dados_jogador;
} Estado_cliente;

//Variáveis globais de controle das threads e sockets do cliente.
int socket_servidor;
std::atomic<bool> conectado_ao_servidor(true);
std::atomic<bool> nova_acao(false);
pthread_t listener_thread, talker_thread;
sem_t conexao;
pthread_mutex_t mutex_estado = PTHREAD_MUTEX_INITIALIZER;

//Funções para
void cleanup_cliente(int sinal);
void* thread_listener(void* parametros);
void* thread_talker(void* parametros);
void desbloqueia_teclado(bool ativar);

void print_estado(const Estado_cliente& estado);

#endif //__cliente_h