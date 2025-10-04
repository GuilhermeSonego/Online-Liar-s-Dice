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

int main()
{
    Estado estado_jogo(NUMERO_JOGADORES, TAMANHO_MAOS);
    std::vector<unsigned int> ordem_turnos;
    unsigned int jogador_atual = 0;
    Acao_resposta acao_recebida = {.acao_escolhida = 0, .valores_acao = NULL, .novo_valor = false};
    bool acao_valida = false;
    bool jogador_atual_desconectado = false;

    //Definição de uma função responsável pelo desligamento do programa pela evocação do CTRL + C.
    signal(SIGINT, sinalizar_desligamento);
    
    //Iremos tratar desconexões manualmente, portanto dizemos ao sistema que iremos ignorar sinais de erro.
    signal(SIGPIPE, SIG_IGN);

    pthread_create(&estado_servidor.thread_socket, NULL, comunicacao_socket, &acao_recebida);

    pthread_join(estado_servidor.thread_socket, nullptr);

    estado_jogo.aleatorizar_maos(LIMITE_FACE);        
    ordem_turnos = gera_ordem_aleatoria(NUMERO_JOGADORES);

    envia_estado_inicial_mesa(estado_jogo);

    while (servidor_rodando.load())
    {
        estado_jogo++;

        jogador_atual = ordem_turnos[estado_jogo.get_turno_atual() % estado_jogo.get_numero_jogadores()];
        jogador_atual = estado_jogo.get_lista_jogadores()[jogador_atual].get_numero();
        estado_jogo.set_jogador_turno_atual(jogador_atual);

        envia_estado_mesa(estado_jogo);

        jogador_atual_desconectado = checa_conexoes(estado_jogo, jogador_atual);

        if(jogador_atual_desconectado)
            continue;

        sleep(10);
        // std::cout << "Esperando jogada...\n";
        // std::cout << "Jogador atual: " << jogador_atual << std::endl;
        // std::cout << "Aposta atual: " << estado_jogo.get_aposta().get_numero_dados() << " | " << estado_jogo.get_aposta().get_dados().get_valor() << '\n';

        // while(!acao_valida)
            // acao_valida = esperar_acao(acao_recebida, estado_jogo, semaforo_servidor, semaforos_jogadores[jogador_atual-1], jogador_atual);
        
        acao_valida = false;
        
        // estado_jogo.print_tudo();
    }
    
    sinalizar_desligamento(SIGINT);
    cleanup_servidor();
}

void cleanup_servidor()
{
    //Espera o fim de todas as threads abertas no programa, uma a uma.
    for(auto i = 0; i < (int)estado_servidor.jogadores.size(); ++i)
        pthread_join(estado_servidor.jogadores[i]->thread, nullptr);
    
    close(estado_servidor.socket_servidor);
    
    //Enfim finaliza o programa.
    exit(0);
}

void sinalizar_desligamento(int sinal)
{
    servidor_rodando.store(false);

    shutdown(estado_servidor.socket_servidor, SHUT_RDWR);
    
    for (unsigned int i = 0; i < estado_servidor.jogadores.size(); ++i)
    {
        shutdown(estado_servidor.jogadores[i]->socket, SHUT_RDWR);
        sem_post(&estado_servidor.jogadores[i]->semaforo);
    }
}

void* comunicacao_socket(void* parametros)
{
    struct sockaddr_in info_socket;
    Estado_thread* parametros_thread = {};

    //Procedimentos para a criação do socket, seu binding e definição de modo em escuta.
    estado_servidor.socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    info_socket = {.sin_family = AF_INET, .sin_port = htons(PORTA), .sin_addr = INADDR_ANY};
    bind(estado_servidor.socket_servidor, (struct sockaddr*)&info_socket, sizeof(info_socket));
    listen(estado_servidor.socket_servidor, NUMERO_JOGADORES);

    estado_servidor.jogadores.resize(NUMERO_JOGADORES);
    for(unsigned int i = 0; i < estado_servidor.jogadores.size(); ++i)
        estado_servidor.jogadores[i] = std::make_unique<Jogador_cliente>();

    sem_init(&estado_servidor.semaforo_servidor, 0, 0);

    for(int i = 0; i < NUMERO_JOGADORES; ++i)
    {
        int novo_socket;
        novo_socket = accept(estado_servidor.socket_servidor, NULL, NULL);
        if (novo_socket < 0)
        {
            if (!servidor_rodando.load())
                break;

            perror("accept");
            continue;
        }
        
        pthread_t nova_thread;

        sem_init(&estado_servidor.jogadores[i]->semaforo, 0, 0);

        parametros_thread = (Estado_thread*)malloc(sizeof(Estado_thread));
        *parametros_thread = 
        {
            .numero_jogador   = (unsigned int)i+1,
            .socket           = novo_socket,
            .semaforo_jogador = &estado_servidor.jogadores[i]->semaforo,
            .acao             = (Acao_resposta*)parametros,
            .ligada           = &estado_servidor.jogadores[i]->conectado
        };

        pthread_create(&nova_thread, NULL, main_jogador, parametros_thread);

        estado_servidor.jogadores[i]->thread = nova_thread;
        estado_servidor.jogadores[i]->socket = novo_socket;
        estado_servidor.jogadores[i]->numero = i+1;
    }

    return nullptr;
}

void* main_jogador(void* parametros)
{
    Estado_thread parametros_thread = *(Estado_thread*)parametros;

    sem_t* semaforo = parametros_thread.semaforo_jogador;
    int numero_jogador = parametros_thread.numero_jogador;
    int socket_jogador = parametros_thread.socket;
    Acao_resposta *acao_tomada = parametros_thread.acao;
    Acao_resposta acao_convertida_buffer;
    bool* ligada = parametros_thread.ligada;

    free((Estado_thread*)parametros);
    
    int tamanho_mensagem = 0;
    char buffer[MENSAGEM_CLIENTE];

    *ligada = true;

    while (servidor_rodando.load() && *ligada)
    {
        //A thread do jogador espera pela permissão da thread principal para executar.
        sem_wait(semaforo);

        pthread_mutex_lock(&mutex_acao);

        

        pthread_mutex_unlock(&mutex_acao);

        sem_post(&estado_servidor.semaforo_servidor);
    }

    close(socket_jogador);
    return nullptr;
}

bool esperar_acao(Acao_resposta &acao_recebida, Estado &estado_jogo, sem_t &semaforo_servidor, sem_t &semaforo_jogador_atual, unsigned int jogador_atual)
{
    bool acao_valida = false;
    

    //do while(acao_valida)

    
    //Sinaliza para a thread do jogador atual que essa pode executar.
    sem_post(&semaforo_jogador_atual);

    //Espera pelo término da execução da thread do jogador.
    sem_wait(&semaforo_servidor);

    // cout << "entrando...\n";

    //Tranca o mutex para evitar condições de corrida.
    pthread_mutex_lock(&mutex_acao);

    // cout << "Esperando mutex...\n";
    
    // cout << "Liberado!!!\n";

    //Executa a ação enviada pelo jogador.
    acao_valida = !executar_acao(acao_recebida, estado_jogo, jogador_atual);

    // cout << "Acao executada!!!\n";

    //Destranca o mutex após as operações.
    pthread_mutex_unlock(&mutex_acao);

    return acao_valida;
}

int executar_acao(Acao_resposta acao, Estado &estado_jogo, unsigned int numero_jogador_atual)
{
    switch (acao.acao_escolhida)
    {
        case AUMENTAR_APOSTA:
        {
            int quantia = acao.valores_acao[0], valor = acao.valores_acao[1];
            Aposta nova_aposta(quantia, valor);

            if(!checa_aposta_valida(nova_aposta, estado_jogo))
                return EXIT_FAILURE;

            estado_jogo.set_aposta(nova_aposta, numero_jogador_atual);
            estado_jogo.adiciona_jogada(Jogada(nova_aposta, acao.acao_escolhida));
        }
        break;

        case DUVIDAR:
        {
            if (estado_jogo.get_aposta() == Aposta())
                return EXIT_FAILURE;

            if(checa_dados_mesa(estado_jogo) != MENOS)
            {
                estado_jogo.tirar_dado_jogador(numero_jogador_atual);
                std::cout << "tirando dado de: " << numero_jogador_atual << std::endl;
            }

            else
            {
                estado_jogo.tirar_dado_ultimo_jogador();
                std::cout << "tirando dado de: " << estado_jogo.get_jogador_aposta_atual()+1 << std::endl;
            }

            estado_jogo.set_aposta(Aposta(), 0);
            estado_jogo.aleatorizar_maos(LIMITE_FACE);
            estado_jogo.resetar_ultimas_jogadas();

            //enviar dados da mesa
        }
        break;

        case CRAVAR:
        {
            if (estado_jogo.get_aposta() == Aposta())
                return EXIT_FAILURE;
            
            if(checa_dados_mesa(estado_jogo) == EXATOS)
            {
                for(unsigned int jogadores = 0; jogadores < estado_jogo.get_numero_jogadores(); jogadores++)
                    if(estado_jogo.get_jogador(jogadores).get_numero() != numero_jogador_atual)
                        estado_jogo.tirar_dado_jogador(jogadores);
            }

            else
                estado_jogo.tirar_dado_jogador(numero_jogador_atual);
            
            estado_jogo.set_aposta(Aposta(), 0);
            estado_jogo.aleatorizar_maos(LIMITE_FACE);
            estado_jogo.resetar_ultimas_jogadas();
        }
        break;
    }

    return EXIT_SUCCESS;
}

bool checa_aposta_valida(const Aposta &nova_aposta, const Estado &estado_jogo)
{
    Dado dado_aposta;
    unsigned int numero_dados;
    auto lista_jogadas = estado_jogo.get_ultimas_jogadas();
    int contagem_dados_iguais = 0;
    bool igual_aposta_anterior = false;
    int valor_dados;

    dado_aposta = nova_aposta.get_dados();
    numero_dados = nova_aposta.get_numero_dados();
    valor_dados = dado_aposta.get_valor();

    //Se a aposta envolver menos que um dado, ela é inválida.
    if(numero_dados < 1)
        return false;

    //Se a face dos dados da aposta não estiver entre um e o limite, ela é inválida.
    if(dado_aposta < 1 || dado_aposta > LIMITE_FACE)
        return false;

    //Se não houverem jogadas ainda, a aposta é válida.
    if(lista_jogadas.size() < 1)
        return true;


    switch (valor_dados)
    {
        default:
        {
            //Caso a aposta tenha um número menor de dados que a anterior e aqueles não forem bagos, então ela é inválida.
            if(numero_dados < lista_jogadas.back().get_aposta().get_numero_dados() && lista_jogadas.back().get_aposta().get_dados().get_valor() != BAGO)
                return false;
            
            //Se estes anteriores forem bagos e o valor for menor que o dobro, então ela também é inválida
            if(numero_dados < lista_jogadas.back().get_aposta().get_numero_dados() * 2 && lista_jogadas.back().get_aposta().get_dados().get_valor() == BAGO)
                return false;

            
            /*Caso a aposta repita um número de dados já anunciado duas vezes anteriormente, excluindo no caso de bagos, então ela é inválida.
            Além disso, se ela for igual a qualquer uma das últimas apostas, ela é inválida. (Só será necessário checar as últimas três jogadas)*/
            for(int i = 0; i < 3 && i < (int)lista_jogadas.size(); ++i)
            {
                if(numero_dados == lista_jogadas[i].get_aposta().get_numero_dados() && lista_jogadas[i].get_aposta().get_dados().get_valor() != BAGO)
                    contagem_dados_iguais++;
                
                igual_aposta_anterior = false || (nova_aposta == lista_jogadas[i].get_aposta());
            }
    
        }
        break;

        //Fazemos verificações diferentes no caso de uma aposta envolvendo bagos.
        case BAGO:
        {
            //Caso a aposta seja menor que o número de dados anterior e eles também forem bagos, então ela é inválida
            if(numero_dados < lista_jogadas.back().get_aposta().get_numero_dados() && lista_jogadas.back().get_aposta().get_dados().get_valor() == BAGO)
                return false;
            
            //Caso os dados anteriores não forem bagos, se o dobro da aposta for menor que o número de dados anterior, então ela é inválida
            if(2 * numero_dados < lista_jogadas.back().get_aposta().get_numero_dados() && lista_jogadas.back().get_aposta().get_dados().get_valor() != BAGO)
                return false;
            
            //Se a aposta for igual a qualquer uma das três últimas, ela é inválida.
            for(int i = 0; i < 3 && i < (int)lista_jogadas.size(); ++i)
                igual_aposta_anterior = false || (nova_aposta == lista_jogadas[i].get_aposta());
        }
        break;
    }

    //Última checagem citada nos dois casos.
    if (contagem_dados_iguais >= 2 || igual_aposta_anterior)
        return false;

    //Se nenhuma das verificações de invalidade forem verdadeiras, então é uma aposta válida.
    return true;
}

int checa_dados_mesa(const Estado &estado_jogo)

{
    Aposta aposta_mesa(0, Dado(estado_jogo.get_aposta().get_dados().get_valor()));

    for(unsigned int i = 0; i < estado_jogo.get_numero_jogadores(); ++i)
        for(unsigned j = 0; j < estado_jogo.get_lista_jogadores()[i].get_mao().get_numero_dados(); ++j)    
            if(estado_jogo.get_lista_jogadores()[i].get_mao().get_dados()[j].get_valor() == aposta_mesa.get_dados().get_valor() 
            || estado_jogo.get_lista_jogadores()[i].get_mao().get_dados()[j].get_valor() == BAGO)
                aposta_mesa.adiciona_dado();
        
    if(aposta_mesa > estado_jogo.get_aposta())
        return MAIS;
    
    if(aposta_mesa == estado_jogo.get_aposta())
        return EXATOS;

    return MENOS;

}

std::vector<unsigned int> gera_ordem_aleatoria(unsigned int tamanho)
{
    std::vector<unsigned int> vetor_aleatorio(tamanho);
    for (unsigned int i = 0; i < tamanho; ++i)
        vetor_aleatorio[i] = i;

    std::random_device rd;
    std::mt19937 gen(rd());
    shuffle(vetor_aleatorio.begin(), vetor_aleatorio.end(), gen);

    return vetor_aleatorio;
}

void envia_estado_mesa(const Estado &estado_jogo)
{
    //A estruturação da mensagem de estado será feita
    Estado_mesa_msg estado_novo = {0};
    unsigned int tamanho_mensagem = sizeof(estado_novo.rodada) + sizeof(estado_novo.n_dados_aposta) + sizeof(estado_novo.face_dados_aposta) + sizeof(estado_novo.jogador_atual);
    bool sucesso_operacao = true;

    std::cout << "ESTADO MESA SENDO ENVIADO!\n";

    estado_novo = 
    {
        .rodada = estado_jogo.get_turno_atual(),
        .n_dados_aposta = estado_jogo.get_aposta().get_numero_dados(),
        .face_dados_aposta = (unsigned int)estado_jogo.get_aposta().get_dados().get_valor(),
        .jogador_atual = estado_jogo.get_jogador_turno_atual()
    };

    for(unsigned int i = 0; i < estado_servidor.jogadores.size(); ++i)
    {

        if(!estado_servidor.jogadores[i]->conectado)
            continue;

        sucesso_operacao = enviar_mensagem(estado_servidor.jogadores[i]->socket, &estado_novo, tamanho_mensagem, Estado_mesa);

        if(!sucesso_operacao)
            estado_servidor.jogadores[i]->conectado = false;
    }

    return;
}

void envia_estado_inicial_mesa(const Estado &estado_jogo)
{
    Estado_inicial_msg estado_inicial = {.header = {}};
    unsigned int tamanho_mensagem = 0, tamanho_mensagem_atual;
    unsigned int numero_jogadores = estado_jogo.get_numero_jogadores();
    unsigned int n_dados_jogador_atual = 0;
    bool sucesso_operacao = true;

    std::cout << "ESTADO INICIAL SENDO ENVIADO!\n";

    estado_inicial.numero_jogadores = numero_jogadores;
    estado_inicial.dados_jogadores = (_dados_jogador*)malloc(numero_jogadores * sizeof(_dados_jogador));
    tamanho_mensagem = sizeof(estado_inicial.numero_jogadores) + numero_jogadores*(sizeof(estado_inicial.dados_jogadores->n_dados) + sizeof(estado_inicial.dados_jogadores->numero_jogador)) + sizeof(estado_inicial.numero_jogador_atual);

    for (unsigned int i = 0; i < numero_jogadores; ++i)
    {
        estado_inicial.dados_jogadores[i].numero_jogador = estado_jogo.get_lista_jogadores()[i].get_numero();
        estado_inicial.dados_jogadores[i].n_dados = estado_jogo.get_lista_jogadores()[i].get_mao().get_numero_dados();
    }

    for (unsigned int i = 0; i < numero_jogadores; ++i)
    {
        tamanho_mensagem_atual = tamanho_mensagem;
        estado_inicial.numero_jogador_atual = estado_jogo.get_lista_jogadores()[i].get_numero();
        n_dados_jogador_atual = estado_jogo.get_lista_jogadores()[i].get_mao().get_dados().size();
        estado_inicial.valor_dados_jogador = (int*)malloc(n_dados_jogador_atual * sizeof(*estado_inicial.valor_dados_jogador));

        for(unsigned int j = 0; j < n_dados_jogador_atual; ++j)
            estado_inicial.valor_dados_jogador[j] = estado_jogo.get_lista_jogadores()[i].get_mao().get_dados()[j].get_valor();
        
        tamanho_mensagem_atual += n_dados_jogador_atual*sizeof(*estado_inicial.valor_dados_jogador);
        
        std::cout << tamanho_mensagem_atual << '\n';

        sucesso_operacao = enviar_mensagem(estado_servidor.jogadores[i]->socket, &estado_inicial, tamanho_mensagem_atual, Estado_inicial);

        if(!sucesso_operacao)
            estado_servidor.jogadores[i]->conectado = false;
        
        free (estado_inicial.valor_dados_jogador);
    }

    free (estado_inicial.dados_jogadores);
}

bool checa_conexoes(Estado &estado_jogo, unsigned int jogador_atual)
{
    Desconexao_msg mensagem_desconexao = {};
    bool jogador_atual_desconectado = false;
    unsigned int jogadores_desconectados[NUMERO_JOGADORES];
    unsigned int numero_desconectados = 0;
    unsigned tamanho_mensagem = sizeof(unsigned int);
    bool sucesso_operacao = true;

    std::cout << "Checando desconexoes!\n";
    for(unsigned int i = 0; i < estado_servidor.jogadores.size(); ++i)
        if(!estado_servidor.jogadores[i]->conectado)
        {
            if(estado_servidor.jogadores[i]->numero == jogador_atual)
                jogador_atual_desconectado = true;
            
            std::cout << "Desconectou-se: " << estado_servidor.jogadores[i]->numero << '\n';
            jogadores_desconectados[numero_desconectados++] = estado_servidor.jogadores[i]->numero;
            std::cout << "desligando...\n";
            desligar_jogador_cliente(i);
            std::cout << "removendo do jogo...\n";
            estado_jogo.remover_jogador(i);
            std::cout << "removido!\n";
        }
    
    if(numero_desconectados == 0)
    {
        std::cout << "Nenhuma desconexao!\n";
        return false;
    }
    
    tamanho_mensagem += sizeof(unsigned int) * numero_desconectados;
    
    mensagem_desconexao = {.quantia_desconectados = numero_desconectados, .numero_jogadores = jogadores_desconectados};

    std::cout << "Enviando desconectados!!\n";
    for(unsigned int i = 0; i < estado_servidor.jogadores.size(); ++i)
    {
        sucesso_operacao = enviar_mensagem(estado_servidor.jogadores[i]->socket, &mensagem_desconexao, tamanho_mensagem, Desconexao);

        if(!sucesso_operacao)
            estado_servidor.jogadores[i]->conectado = false;
    }

    return jogador_atual_desconectado;
}

void desligar_jogador_cliente(unsigned int indice)
{
    std::cout << "desligando socket...\n";
    shutdown(estado_servidor.jogadores[indice]->socket, SHUT_RDWR);
    sem_post(&estado_servidor.jogadores[indice]->semaforo);
    std::cout << "esperando a thread...\n";
    pthread_join(estado_servidor.jogadores[indice]->thread, nullptr);
    std::cout << "fechando o socket...\n";
    close(estado_servidor.jogadores[indice]->socket);
    std::cout << "tirando da lista...\n";
    estado_servidor.jogadores.erase(estado_servidor.jogadores.begin() + indice);
    std::cout << "jogador desligado com sucesso!\n";
}