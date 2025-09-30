#ifndef _protocolo_h
#define _protocolo_h

enum Tipos_Mensagem
{
    Heartbeat,
    Estado_inicial,
};

#define HEADER_TAMANHO sizeof(unsigned int) + sizeof(unsigned int)
struct Header_protocolo
{
    unsigned int tamanho_mensagem;
    unsigned int tipo_mensagem;
};

#define HEARTBEAT_TAMANHO HEADER_TAMANHO

struct Estado_inicial_msg
{
    Header_protocolo header;
    unsigned int numero_jogadores;
    unsigned int *numero_dados;
    int *valor_dados_jogador;

};

void enviar_mensagem(int socket_destino, const void *informacoes, unsigned int tamanho_mensagem, int tipo_mensagem);
void receber_mensagem(int socket_destino);
void escreve_pro_buffer(char* &buffer, void* valor, unsigned int tamanho);
void escreve_cabecalho(char* &buffer, const struct Header_protocolo &cabecalho);
void send_completo(int socket, const void *buffer, unsigned int tamanho);
#endif //_protocolo_h