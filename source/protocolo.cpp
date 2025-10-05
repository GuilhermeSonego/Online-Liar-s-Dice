#include "protocolo.h"
#include <arpa/inet.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>

// Função de envio das mensagens do protocolo
bool enviar_mensagem(int socket_destino, const void *informacoes, unsigned int tamanho_mensagem, int tipo_mensagem)
{
    // Header definindo o tipo da mensagem e seu tamanho como passados para a função
    Header_protocolo header = {.tamanho_mensagem = tamanho_mensagem, .tipo_mensagem = tipo_mensagem};
    bool sucesso_escrita = true;
    unsigned tamanho_total_buffer = tamanho_mensagem + HEADER_TAMANHO;
    char* buffer_saida = (char*)malloc(tamanho_total_buffer);
    char* itera_buffer = buffer_saida;

    // Procedimentos de escrita do buffer de envio conforme o tipo da mensagem
    switch(tipo_mensagem)
    {
        case Heartbeat:
        {
            escreve_cabecalho(itera_buffer, header);
        }
        break;

        case Estado_inicial:
        {
            Estado_inicial_msg* buffer_entrada = (Estado_inicial_msg*)informacoes;
            unsigned int numero_jogador_atual = buffer_entrada->numero_jogador_atual;
            unsigned int n_dados_jogador_atual = 0;

            escreve_cabecalho(itera_buffer, header);

            escreve_serializado(itera_buffer, buffer_entrada->numero_jogadores);
            escreve_serializado(itera_buffer, buffer_entrada->numero_jogador_atual);
            
            for(unsigned int i = 0; i < buffer_entrada->numero_jogadores; ++i)
            {
                escreve_serializado(itera_buffer, buffer_entrada->dados_jogadores[i].numero_jogador);
                escreve_serializado(itera_buffer, buffer_entrada->dados_jogadores[i].n_dados);

                if(buffer_entrada->dados_jogadores[i].numero_jogador == numero_jogador_atual)
                    n_dados_jogador_atual = buffer_entrada->dados_jogadores[i].n_dados;
            }

            for(unsigned int i = 0; i < n_dados_jogador_atual; ++i)
                escreve_serializado(itera_buffer, (unsigned int)buffer_entrada->valor_dados_jogador[i]);
        }
        break;

        case Estado_mesa:
        {
            Estado_mesa_msg* buffer_entrada = (Estado_mesa_msg*)informacoes;
            
            escreve_cabecalho(itera_buffer, header);

            escreve_serializado(itera_buffer, buffer_entrada->rodada);
            escreve_serializado(itera_buffer, buffer_entrada->n_dados_aposta);
            escreve_serializado(itera_buffer, buffer_entrada->face_dados_aposta);
            escreve_serializado(itera_buffer, buffer_entrada->jogador_atual);
        }
        break;

        case Desconexao:
        {
            Desconexao_msg* buffer_entrada = (Desconexao_msg*)informacoes;

            escreve_cabecalho(itera_buffer, header);

            escreve_serializado(itera_buffer, buffer_entrada->quantia_desconectados);

            for(unsigned int i = 0; i < buffer_entrada->quantia_desconectados; ++i)
                escreve_serializado(itera_buffer, buffer_entrada->numero_jogadores[i]);
        }
        break;

        case Aumento_aposta:
        {
            Aposta_msg* buffer_entrada = (Aposta_msg*)informacoes;

            escreve_cabecalho(itera_buffer, header);

            escreve_serializado(itera_buffer, buffer_entrada->n_dados_aposta);
            escreve_serializado(itera_buffer, buffer_entrada->face_dados_aposta);
        }
        break;

        case Duvida:
        {
            escreve_cabecalho(itera_buffer, header);
        }
        break;

        case Cravada:
        {
            escreve_cabecalho(itera_buffer, header);
        }
        break;

        case Revela_mesa:
        {
            Revela_mesa_msg* buffer_entrada = (Revela_mesa_msg*)informacoes;

            escreve_cabecalho(itera_buffer, header);

            escreve_serializado(itera_buffer, buffer_entrada->numero_jogadores);
            escreve_serializado(itera_buffer, buffer_entrada->numero_jogador_vencedor);

            for(unsigned int i = 0; i < buffer_entrada->numero_jogadores; ++i)
            {
                escreve_serializado(itera_buffer, buffer_entrada->jogadores[i].numero_jogador);
                escreve_serializado(itera_buffer, buffer_entrada->jogadores[i].n_dados);
                for(unsigned j = 0; j < buffer_entrada->jogadores[i].n_dados; ++j)
                    escreve_serializado(itera_buffer, buffer_entrada->jogadores[i].valor_dados[j]);
            }
        }
        break;

        case Vencedor:
        {
            Vencedor_msg* buffer_entrada = (Vencedor_msg*)informacoes;

            escreve_cabecalho(itera_buffer, header);
            escreve_serializado(itera_buffer, buffer_entrada->numero_jogador_vencedor);
        }
        break;

        default:
        break;
    }

    // Envio da mensagem completa e liberação do buffer enviado
    sucesso_escrita = send_completo(socket_destino, buffer_saida, tamanho_total_buffer);
    free(buffer_saida);

    return sucesso_escrita;
}

// Função de recebimento e leitura das mensagens enviadas do protocolo.
Mensagem receber_mensagem(int socket_destino)
{
    char buffer_header[HEADER_TAMANHO];
    char* buffer_conteudo_mensagem = nullptr;
    char* itera_buffer;
    Header_protocolo header_mensagem = {};
    Mensagem mensagem_recebida = {};
    bool sucesso_leitura = true;

    // Leitura do header da mensagem para definição do tipo da mensagem e o tamanho do restante da mensagem.
    sucesso_leitura = le_para_buffer(socket_destino, buffer_header, HEADER_TAMANHO);

    if(!sucesso_leitura)
    {   
        mensagem_recebida.tipo_mensagem = Falha;
        return mensagem_recebida;
    }

    memcpy(&header_mensagem, buffer_header, HEADER_TAMANHO);

    header_mensagem = {.tamanho_mensagem = ntohl(header_mensagem.tamanho_mensagem), 
        .tipo_mensagem = (int)ntohl(header_mensagem.tipo_mensagem)};


    // Alocação do buffer de entrada conforme o tamanho da mensagem a ser recebida.
    buffer_conteudo_mensagem = (char*)malloc(header_mensagem.tamanho_mensagem);
    itera_buffer = buffer_conteudo_mensagem;
    mensagem_recebida.tipo_mensagem = header_mensagem.tipo_mensagem;

    // Procedimentos de conversão do buffer de entrada da mensagem recebida para a estrutura da mensagem.
    switch(header_mensagem.tipo_mensagem)
    {
        case Heartbeat:
        {
            mensagem_recebida.conteudo_mensagem = nullptr;
        }
        break;

        case Estado_inicial:
        {
            Estado_inicial_msg* estrutura_mensagem = (Estado_inicial_msg*)malloc(sizeof(Estado_inicial_msg));
            unsigned int n_dados_jogador_atual = 0;

            sucesso_leitura = le_para_buffer(socket_destino, buffer_conteudo_mensagem, header_mensagem.tamanho_mensagem);

            if(!sucesso_leitura)
            {   
                mensagem_recebida.tipo_mensagem = Falha;
                break;
            }

            escreve_desserializado(&estrutura_mensagem->numero_jogadores, itera_buffer, sizeof(estrutura_mensagem->numero_jogadores));
            escreve_desserializado(&estrutura_mensagem->numero_jogador_atual, itera_buffer, sizeof(estrutura_mensagem->numero_jogador_atual));

            estrutura_mensagem->dados_jogadores = (_dados_jogador*)malloc(estrutura_mensagem->numero_jogadores*sizeof(_dados_jogador));

            for(unsigned int i = 0; i < estrutura_mensagem->numero_jogadores; ++i)
            {
                escreve_desserializado(&estrutura_mensagem->dados_jogadores[i].numero_jogador, itera_buffer, sizeof(estrutura_mensagem->dados_jogadores->numero_jogador));
                escreve_desserializado(&estrutura_mensagem->dados_jogadores[i].n_dados, itera_buffer, sizeof(estrutura_mensagem->dados_jogadores->n_dados));

                if(estrutura_mensagem->dados_jogadores[i].numero_jogador == estrutura_mensagem->numero_jogador_atual)
                    n_dados_jogador_atual = estrutura_mensagem->dados_jogadores[i].n_dados;
            }

            estrutura_mensagem->valor_dados_jogador = (int*)malloc(n_dados_jogador_atual*sizeof(*estrutura_mensagem->valor_dados_jogador));

            for(unsigned int i = 0; i < n_dados_jogador_atual; ++i)
                escreve_desserializado(&estrutura_mensagem->valor_dados_jogador[i], itera_buffer, sizeof(*estrutura_mensagem->valor_dados_jogador));
            
            mensagem_recebida.conteudo_mensagem = estrutura_mensagem;
        }
        break;

        case Estado_mesa:
        {
            Estado_mesa_msg* estrutura_mensagem = (Estado_mesa_msg*)malloc(sizeof(Estado_mesa_msg));

            sucesso_leitura = le_para_buffer(socket_destino, buffer_conteudo_mensagem, header_mensagem.tamanho_mensagem);

            if(!sucesso_leitura)
            {
                mensagem_recebida.tipo_mensagem = Falha;
                break;
            }

            escreve_desserializado(&estrutura_mensagem->rodada, itera_buffer, sizeof(estrutura_mensagem->rodada));
            escreve_desserializado(&estrutura_mensagem->n_dados_aposta, itera_buffer, sizeof(estrutura_mensagem->n_dados_aposta));
            escreve_desserializado(&estrutura_mensagem->face_dados_aposta, itera_buffer, sizeof(estrutura_mensagem->face_dados_aposta));
            escreve_desserializado(&estrutura_mensagem->jogador_atual, itera_buffer, sizeof(estrutura_mensagem->jogador_atual));

            mensagem_recebida.conteudo_mensagem = estrutura_mensagem;
        }
        break;

        case Desconexao:
        {
            Desconexao_msg* estrutura_mensagem = (Desconexao_msg*)malloc(sizeof(Desconexao_msg));

            sucesso_leitura = le_para_buffer(socket_destino, buffer_conteudo_mensagem, header_mensagem.tamanho_mensagem);

            if(!sucesso_leitura)
            {
                mensagem_recebida.tipo_mensagem = Falha;
                break;
            }

            escreve_desserializado(&estrutura_mensagem->quantia_desconectados, itera_buffer, sizeof(estrutura_mensagem->quantia_desconectados));

            if(estrutura_mensagem->quantia_desconectados != 0)
                estrutura_mensagem->numero_jogadores = (unsigned int*)malloc(sizeof(*estrutura_mensagem->numero_jogadores)*estrutura_mensagem->quantia_desconectados);

            for(unsigned int i = 0; i < estrutura_mensagem->quantia_desconectados; ++i)
                escreve_desserializado(&estrutura_mensagem->numero_jogadores[i], itera_buffer, sizeof(*estrutura_mensagem->numero_jogadores));
            
            mensagem_recebida.conteudo_mensagem = estrutura_mensagem;
        }
        break;

        case Aumento_aposta:
        {
            Aposta_msg* estrutura_mensagem = (Aposta_msg*)malloc(sizeof(Estado_mesa_msg));

            sucesso_leitura = le_para_buffer(socket_destino, buffer_conteudo_mensagem, header_mensagem.tamanho_mensagem);

            if(!sucesso_leitura)
            {
                mensagem_recebida.tipo_mensagem = Falha;
                break;
            }

            escreve_desserializado(&estrutura_mensagem->n_dados_aposta, itera_buffer, sizeof(estrutura_mensagem->n_dados_aposta));
            escreve_desserializado(&estrutura_mensagem->face_dados_aposta, itera_buffer, sizeof(estrutura_mensagem->face_dados_aposta));

            mensagem_recebida.conteudo_mensagem = estrutura_mensagem;   
        }
        break;

        case Duvida:
        {
            mensagem_recebida.conteudo_mensagem = nullptr;
        }
        break;

        case Cravada:
        {
            mensagem_recebida.conteudo_mensagem = nullptr;
        }
        break;

        case Revela_mesa:
        {
            Revela_mesa_msg* estrutura_recebida = (Revela_mesa_msg*)malloc(sizeof(Revela_mesa_msg));

            sucesso_leitura = le_para_buffer(socket_destino, buffer_conteudo_mensagem, header_mensagem.tamanho_mensagem);

            if(!sucesso_leitura)
            {   
                mensagem_recebida.tipo_mensagem = Falha;
                break;
            }

            escreve_desserializado(&estrutura_recebida->numero_jogadores, itera_buffer, sizeof(estrutura_recebida->numero_jogadores));
            escreve_desserializado(&estrutura_recebida->numero_jogador_vencedor, itera_buffer, sizeof(estrutura_recebida->numero_jogador_vencedor));

            estrutura_recebida->jogadores = (_dados_revelados_jogador*)malloc(estrutura_recebida->numero_jogadores * sizeof(_dados_revelados_jogador));

            for(unsigned int i = 0; i < estrutura_recebida->numero_jogadores; ++i)
            {
                escreve_desserializado(&estrutura_recebida->jogadores[i].numero_jogador, itera_buffer, sizeof(estrutura_recebida->jogadores->numero_jogador));
                escreve_desserializado(&estrutura_recebida->jogadores[i].n_dados, itera_buffer, sizeof(estrutura_recebida->jogadores->n_dados));

                estrutura_recebida->jogadores[i].valor_dados = (int*)malloc(estrutura_recebida->jogadores[i].n_dados * sizeof(*estrutura_recebida->jogadores->valor_dados));

                for(unsigned j = 0; j < estrutura_recebida->jogadores[i].n_dados; ++j)
                    escreve_desserializado(&estrutura_recebida->jogadores[i].valor_dados[j], itera_buffer, sizeof(*estrutura_recebida->jogadores->valor_dados));
            }
            
            mensagem_recebida.conteudo_mensagem = estrutura_recebida;
        }
        break;

        case Vencedor:
        {
            Vencedor_msg* estrutura_recebida = (Vencedor_msg*)malloc(sizeof(Vencedor_msg));

            sucesso_leitura = le_para_buffer(socket_destino, buffer_conteudo_mensagem, header_mensagem.tamanho_mensagem);

            if(!sucesso_leitura)
            {   
                mensagem_recebida.tipo_mensagem = Falha;
                break;
            }

            escreve_desserializado(&estrutura_recebida->numero_jogador_vencedor, itera_buffer, sizeof(estrutura_recebida->numero_jogador_vencedor));

            mensagem_recebida.conteudo_mensagem = estrutura_recebida;
        }
        break;

        default:
        break;
    }

    // Liberação do buffer usado para armazenamento da mensagem recebida.
    free(buffer_conteudo_mensagem);
    return mensagem_recebida;
}

// Função de escrita para o buffer e prosseguimento do ponteiro.
void escreve_pro_buffer(char* &buffer, const void* valor, unsigned int tamanho)
{
    memcpy(buffer, valor, tamanho);
    buffer += tamanho;
}

// Função de escrita do buffer para a estrutura de recebimento da mensagem.
void escreve_para_campo(void* campo, char* &buffer, unsigned int tamanho)
{
    memcpy(campo, buffer, tamanho);
    buffer += tamanho;
}

// Função de serialização dos dados.
void escreve_serializado(char* &buffer, unsigned int valor)
{
    valor = htonl(valor);
    escreve_pro_buffer(buffer, &valor, sizeof(valor));
}

// Função de desserilização dos dados.
void escreve_desserializado(void* campo, char* &buffer, unsigned int tamanho)
{
    escreve_para_campo(campo, buffer, tamanho);
    *(unsigned int*)campo = ntohl(*(unsigned int*)campo);
}

// Função utilitária padrão de escrita do cabeçalho do protocolo da mensagem. 
void escreve_cabecalho(char* &buffer, const struct Header_protocolo &cabecalho)
{
    escreve_serializado(buffer, cabecalho.tamanho_mensagem);
    escreve_serializado(buffer, cabecalho.tipo_mensagem);
}

// Função de leitura do número de dados esperados. No caso de erro, retorna falso para alertar o chamador.
bool le_para_buffer(int socket, char* buffer, unsigned int tamanho)
{
    unsigned int bytes_lidos = 0;
    int recebidos = 0;

    while (bytes_lidos < tamanho) 
    {
        recebidos = recv(socket, buffer + bytes_lidos, tamanho - bytes_lidos, 0);
        if (recebidos <= 0) 
            return false;

        bytes_lidos += recebidos;
    }   

    return true;
}

// Função de envio da mensagem inteira. No caso de erro, retorna falso para alertar o chamador. Define também o não encerramento em caso de erro.
bool send_completo(int socket, const void *buffer, unsigned int tamanho) 
{
    unsigned int total = 0;
    const char *itera_buffer = (const char*)buffer;
    int enviados;

    while (total < tamanho) 
    {
        enviados = send(socket, itera_buffer + total, tamanho - total, MSG_NOSIGNAL);
        if (enviados <= 0)
        {   
            std::cout << "Erro ao enviar mensagem!\n";
            return false;
        }

        total += enviados;
    }

    return true;
}

// Funções para desalocação das estruturas de mensagem recebidas. Só é necessário para mensagens com tamanho > 0.
void free_mensagem(Mensagem mensagem_usada)
{
    switch(mensagem_usada.tipo_mensagem)
    {
        case Estado_inicial:
        {
            Estado_inicial_msg* ponteiro_estado_inicial = (Estado_inicial_msg*) mensagem_usada.conteudo_mensagem;

            free(ponteiro_estado_inicial->dados_jogadores);
            free(ponteiro_estado_inicial->valor_dados_jogador);
            free(ponteiro_estado_inicial);
        }
        break;

        case Estado_mesa:
        {
            Estado_mesa_msg* ponteiro_estado_mesa = (Estado_mesa_msg*)mensagem_usada.conteudo_mensagem;

            free(ponteiro_estado_mesa);
        }
        break;

        case Desconexao:
        {
            Desconexao_msg* ponteiro_desconexao = (Desconexao_msg*)mensagem_usada.conteudo_mensagem;

            free(ponteiro_desconexao->numero_jogadores);
            free(ponteiro_desconexao);
        }
        break;

        case Aumento_aposta:
        {
            Aposta_msg* ponteiro_aposta = (Aposta_msg*)mensagem_usada.conteudo_mensagem;

            free(ponteiro_aposta);
        }
        break;

        case Revela_mesa:
        {
            Revela_mesa_msg* ponteiro_revela_mesa = (Revela_mesa_msg*)mensagem_usada.conteudo_mensagem;

            for(unsigned int i = 0; i < ponteiro_revela_mesa->numero_jogadores; ++i)
                free(ponteiro_revela_mesa->jogadores[i].valor_dados);

            free(ponteiro_revela_mesa->jogadores);
            free(ponteiro_revela_mesa);
            
        }
        break;

        case Vencedor:
        {
            Vencedor_msg* ponteiro_vencedor = (Vencedor_msg*)mensagem_usada.conteudo_mensagem;

            free(ponteiro_vencedor);
        }
        break;

        default:
        break;
    }
}