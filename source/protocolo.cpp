#include "protocolo.h"
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void enviar_mensagem(int socket_destino, const void *informacoes, unsigned int tamanho_mensagem, int tipo_mensagem)
{
    char *byte_buffer = (char*)informacoes;
    Header_protocolo header = {.tamanho_mensagem = htonl(tamanho_mensagem), .tipo_mensagem = htonl(tipo_mensagem)};

    switch(tipo_mensagem)
    {
        case Heartbeat:
        {
            char buffer_saida[HEARTBEAT_TAMANHO];
            char* itera_buffer;

            itera_buffer = buffer_saida;

            escreve_cabecalho(itera_buffer, header);

            send_completo(socket_destino, buffer_saida, sizeof(buffer_saida));
        }
        break;

        default:
        break;
    }

}

void receber_mensagem(int socket_destino)
{
    

}

void escreve_pro_buffer(char* &buffer, const void* valor, unsigned int tamanho)
{
    memcpy(buffer, valor, tamanho);
    buffer += tamanho;
}

void escreve_cabecalho(char* &buffer, const struct Header_protocolo &cabecalho)
{
    escreve_pro_buffer(buffer, &cabecalho.tamanho_mensagem, sizeof(cabecalho.tamanho_mensagem));
    escreve_pro_buffer(buffer, &cabecalho.tipo_mensagem, sizeof(cabecalho.tipo_mensagem));
}

void send_completo(int socket, const void *buffer, unsigned int tamanho) 
{
    unsigned int total = 0;
    const char *itera_buffer = (const char*)buffer;
    unsigned int enviados;

    while (total < tamanho) {
        enviados = send(socket, itera_buffer + total, tamanho - total, 0);
        if (enviados <= 0)
            return;

        total += enviados;
    }
}