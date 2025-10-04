#ifndef _protocolo_h
#define _protocolo_h

enum Tipos_Mensagem
{
    Falha,
    Heartbeat,
    Estado_inicial,
    Estado_mesa,
    Desconexao,
    Aumento_aposta,
    Duvida,
    Cravada
};

#define HEADER_TAMANHO (sizeof(unsigned int) + sizeof(unsigned int))
struct Header_protocolo
{
    unsigned int tamanho_mensagem;
    int tipo_mensagem;
};

#define HEARTBEAT_TAMANHO HEADER_TAMANHO

#define DADOS_JOGADOR_TAMANHO sizeof(unsigned int) + sizeof(unsigned int)
struct _dados_jogador
{
    unsigned int numero_jogador;
    unsigned int n_dados;
};

struct Estado_inicial_msg
{
    Header_protocolo header;
    unsigned int numero_jogadores;
    unsigned int numero_jogador_atual;
    struct _dados_jogador* dados_jogadores;
    int *valor_dados_jogador;
};

struct Estado_mesa_msg
{
    Header_protocolo header;
    unsigned int rodada;
    unsigned int n_dados_aposta;
    unsigned int face_dados_aposta;
    unsigned int jogador_atual;
};

struct Desconexao_msg
{
    Header_protocolo header;
    unsigned int quantia_desconectados;
    unsigned int* numero_jogadores;
};

struct Aposta_msg
{
    Header_protocolo header;
    unsigned int n_dados_aposta;
    unsigned int face_dados_aposta;
};

typedef struct _mensagem_t
{
    void* conteudo_mensagem;
    int tipo_mensagem;
} Mensagem;

bool enviar_mensagem(int socket_destino, const void *informacoes, unsigned int tamanho_mensagem, int tipo_mensagem);
Mensagem receber_mensagem(int socket_destino);
void escreve_pro_buffer(char* &buffer, const void* valor, unsigned int tamanho);
void escreve_para_campo(void* campo, int tamanho_campo, const void* valor, unsigned int tamanho);
void escreve_serializado(char* &buffer, unsigned int valor);
void escreve_desserializado(void* campo, char* &buffer, unsigned int tamanho);
void escreve_cabecalho(char* &buffer, const struct Header_protocolo &cabecalho);
bool send_completo(int socket, const void *buffer, unsigned int tamanho);
bool le_para_buffer(int socket, char* buffer, unsigned int tamanho);
void free_mensagem(Mensagem mensagem_usada);
#endif //_protocolo_h