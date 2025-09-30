#include "jogo.h"
#include "server.h"
#include <stdlib.h>
#include <random>
#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include "protocolo.h"

int socket_servidor;

void cleanup_servidore(int sinal);

int main()
{
    struct sockaddr_in info_socket;
    int a;
    char buffer[100];
    //Definição de uma função responsável pelo desligamento do programa pela evocação do CTRL + C.
    signal(SIGINT, cleanup_servidore);

    //Procedimentos para a criação do socket, seu binding e definição de modo em escuta.
    socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    info_socket = {.sin_family = AF_INET, .sin_port = htons(PORTA)};
    if(inet_pton(AF_INET, "127.0.0.1", &info_socket.sin_addr) <= 0)
    {
        printf("Endereco invalido\n");
        cleanup_servidore(SIGINT);
    }
    if (connect(socket_servidor, (struct sockaddr*)&info_socket, sizeof(info_socket)) < 0) {
        printf("Falha na conexão\n");
        cleanup_servidore(SIGINT);
    }

    int bytes_lidos = 0;
    while (bytes_lidos < HEARTBEAT_TAMANHO) {
        int r = recv(socket_servidor, buffer + bytes_lidos, HEARTBEAT_TAMANHO - bytes_lidos, 0);
        if (r <= 0) {
            perror("recv");
            exit(1);
        }
        bytes_lidos += r;
    }

    memcpy(&a, buffer+sizeof(unsigned int), sizeof(unsigned int));
    a = ntohl(a);
    printf("NUMERO: %d\n", a);

    cleanup_servidore(SIGINT);
}

void cleanup_servidore(int sinal)
{
    close(socket_servidor);
    exit(0);
}