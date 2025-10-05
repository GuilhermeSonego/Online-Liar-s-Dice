#include "jogo.h"
#include "server.h"
#include "protocolo.h"
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
#include <poll.h>

int main()
{
    Estado estado_jogo(NUMERO_JOGADORES, TAMANHO_MAOS);
    Acao_resposta acao_recebida = {.acao_escolhida = NADA, .valores_acao = {0}};
    Jogador_cliente* jogador_cliente_atual;
    unsigned int numero_jogador_atual = 0;
    unsigned int turno_jogador = 0;
    unsigned jogador_vencedor = 0;
    bool rodada_concluida = true;
    bool houve_desconexoes = false;

    // Definição de uma função responsável pelo desligamento do programa pela evocação do CTRL + C.
    signal(SIGINT, sinalizar_desligamento);
    
    // Iremos tratar desconexões manualmente, portanto dizemos ao sistema que iremos ignorar sinais de erro.
    signal(SIGPIPE, SIG_IGN);

    // Criação de thread para esperar as conexões de jogadores
    pthread_create(&estado_servidor.thread_socket, NULL, comunicacao_socket, &acao_recebida);

    // Espera as conexões serem completas
    pthread_join(estado_servidor.thread_socket, nullptr);

    // Aleatoriza os dados dos jogadores
    estado_jogo.aleatorizar_maos(LIMITE_FACE);        

    // Embaralha os jogadores para aleatorizar a ordem independente de conexão
    gera_ordem_aleatoria();

    while (servidor_rodando.load())
    {
        // Se sobrar somente um jogador o jogo termina.
        if(estado_jogo.get_numero_jogadores() <= 1)
        {
            std::cout << "Fim de jogo!\n";
            break;
        }

        // Ao término da rodada, isso é, quando um ou mais jogadores perdem dados
        if(rodada_concluida)
        {
            // Checa se sobrou um vencedor
            jogador_vencedor = checa_vencedor(estado_jogo);

            if(jogador_vencedor != 0)
            {
                // Reseta o jogo e envia a mensagem contendo o jogador vencedor a todos os clientes.
                Vencedor_msg mensagem_vencedor = {.numero_jogador_vencedor = jogador_vencedor};
                for(unsigned int i = 0; i < estado_servidor.jogadores.size(); ++i)
                    enviar_mensagem(estado_servidor.jogadores[i]->socket, &mensagem_vencedor, sizeof(jogador_vencedor), Vencedor);
                reseta_estado(estado_jogo);
                turno_jogador = 0;
                rodada_concluida = true;
                continue;
            }

            // Envia informações de estado da mesa
            envia_estado_inicial_mesa(estado_jogo);

            // Pula jogadores que não tenham dados na ordem da rodada
            numero_jogador_atual = estado_servidor.jogadores[turno_jogador % estado_servidor.jogadores.size()]->numero;
            while(estado_jogo.get_jogador(numero_jogador_atual).get_mao().get_numero_dados() == 0)
                numero_jogador_atual = estado_servidor.jogadores[++turno_jogador % estado_servidor.jogadores.size()]->numero;

            // Atualiza o jogador que detém a vez e avança a rodada
            estado_jogo.set_jogador_turno_atual(numero_jogador_atual);
            estado_jogo++;
            jogador_cliente_atual = busca_jogador(numero_jogador_atual);
        }

        // Checa se houveram desconexões no meio tempo.
        houve_desconexoes = checa_conexoes(estado_jogo, numero_jogador_atual);

        // Começa uma nova rodada se houve desconexões.
        if(houve_desconexoes)
        {
            rodada_concluida = true;
            continue;
        }

        // Atualiza com mais informações de estado da mesa
        if(rodada_concluida)
        {
            envia_estado_mesa(estado_jogo);
            rodada_concluida = false;
        }

        std::cout << "Esperando jogada...\n";
        std::cout << "Jogador atual: " << numero_jogador_atual << std::endl;
        std::cout << "Aposta atual: " << estado_jogo.get_aposta().get_numero_dados() << " | " << estado_jogo.get_aposta().get_dados().get_valor() << '\n';

        // Printa no servidor informações da mesa
        estado_jogo.print_tudo();

        // Espera uma ação válida para avançar a rodada.
        rodada_concluida = esperar_acao(acao_recebida, estado_jogo, jogador_cliente_atual);

        // Em caso de uma ação válida, a executa.
        if(rodada_concluida)
            executar_acao(acao_recebida, estado_jogo, numero_jogador_atual, turno_jogador);
    }
    
    sinalizar_desligamento(SIGINT);
    cleanup_servidor();

    return 0;
}

void cleanup_servidor()
{
    // Espera o fim de todas as threads abertas no programa, uma a uma.
    for(auto i = 0; i < (int)estado_servidor.jogadores.size(); ++i)
        pthread_join(estado_servidor.jogadores[i]->thread, nullptr);
    
    close(estado_servidor.socket_servidor);
}

void sinalizar_desligamento(int sinal)
{
    // Sinaliza a todo o programa o seu desligamento.
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

    // Procedimentos para a criação do socket, seu binding e definição de modo em escuta.
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
        
        // Cria uma thread para cada jogador conectado e registra o jogador na variável global de controle
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
    // Thread principal de comunicação para cada jogador
    Estado_thread parametros_thread = *(Estado_thread*)parametros;
    sem_t* semaforo = parametros_thread.semaforo_jogador;
    int socket_jogador = parametros_thread.socket;
    Acao_resposta *acao_tomada = parametros_thread.acao;
    bool* ligada = parametros_thread.ligada;
    struct pollfd evento = {.fd = socket_jogador, .events = POLLIN, .revents = 0};
    int retorno_evento;
    Mensagem mensagem_recebida;

    free((Estado_thread*)parametros);

    // Sinaliza que o jogador correspondente está conectado e começa a execução
    *ligada = true;

    while (servidor_rodando.load() && *ligada)
    {
        // A thread do jogador espera pela permissão da thread principal para executar.
        sem_wait(semaforo);

        // Define um timeout de 2s antes de checar se o servidor continua rodando.
        retorno_evento = poll(&evento, 1, 2000);

        if(retorno_evento <= 0)
            mensagem_recebida.tipo_mensagem = Falha;
        
        // Espera pela mensagem do jogador da ação a ser tomada.
        if(evento.revents & POLLIN)
            mensagem_recebida = receber_mensagem(socket_jogador);

        // Tranca o mutex para acesso à região crítica e atualização da ação atual tomada pelo jogador correspondente.
        switch(mensagem_recebida.tipo_mensagem)
        {
            case Aumento_aposta:
            {
                Aposta_msg *novos_valores_aposta = (Aposta_msg*)mensagem_recebida.conteudo_mensagem;
                pthread_mutex_lock(&mutex_acao);

                *acao_tomada = {.acao_escolhida = AUMENTAR_APOSTA, .valores_acao = {novos_valores_aposta->n_dados_aposta, novos_valores_aposta->face_dados_aposta}}; 

                pthread_mutex_unlock(&mutex_acao);
            }
            break;

            case Duvida:
            {
                pthread_mutex_lock(&mutex_acao);

                *acao_tomada = {.acao_escolhida = DUVIDAR, .valores_acao = {0}}; 

                pthread_mutex_unlock(&mutex_acao);
            }
            break;

            case Cravada:
            {
                pthread_mutex_lock(&mutex_acao);

                *acao_tomada = {.acao_escolhida = CRAVAR, .valores_acao = {0}}; 

                pthread_mutex_unlock(&mutex_acao);
            }
            break;

            case Falha:
            {
                pthread_mutex_lock(&mutex_acao);

                *acao_tomada = {.acao_escolhida = NADA, .valores_acao = {0}}; 

                pthread_mutex_unlock(&mutex_acao); 
            }
            break;

            default:
            {
                pthread_mutex_lock(&mutex_acao);

                *acao_tomada = {.acao_escolhida = NADA, .valores_acao = {0}}; 

                pthread_mutex_unlock(&mutex_acao); 
            }
            break;
        }

        // Libera a memória alocada para a mensagem recebida e sinaliza ao servidor que a ação foi tomada.
        free_mensagem(mensagem_recebida);
        sem_post(&estado_servidor.semaforo_servidor);
    }

    return nullptr;
}

bool esperar_acao(Acao_resposta &acao_recebida, Estado &estado_jogo, Jogador_cliente* jogador_atual)
{
    bool acao_valida = false;

    // Sinaliza para a thread do jogador atual que essa pode executar.
    sem_post(&jogador_atual->semaforo);

    // Espera pelo sinal de recebimento de mensagem da thread do jogador.
    sem_wait(&estado_servidor.semaforo_servidor);

    // Tranca o mutex para evitar condições de corrida.
    pthread_mutex_lock(&mutex_acao);

    // Checagem se a ação enviada é válida.
    acao_valida = checa_acao_valida(acao_recebida, estado_jogo);

    // Destranca o mutex após as operações.
    pthread_mutex_unlock(&mutex_acao);

    return acao_valida;
}

unsigned int executar_acao(Acao_resposta acao, Estado &estado_jogo, unsigned int numero_jogador_atual, unsigned int &turno_jogador)
{
    // Executa a ação escolhida conforme seu tipo.
    switch (acao.acao_escolhida)
    {
        case AUMENTAR_APOSTA:
        {
            // Aumenta o valor da aposta e prossegue para o próximo jogador.
            int quantia = acao.valores_acao[0], valor = acao.valores_acao[1];
            Aposta nova_aposta(quantia, valor);

            turno_jogador++;
            estado_jogo.set_aposta(nova_aposta, numero_jogador_atual);
            estado_jogo.adiciona_jogada(Jogada(nova_aposta, acao.acao_escolhida));
        }
        break;

        case DUVIDAR:
        {
            // Monta a mensagem de dados da mesa para envio.
            Revela_mesa_msg dados_mesa;
            unsigned int n_dados_jogador;
            unsigned int tamanho_mensagem = sizeof(dados_mesa.numero_jogadores) + sizeof(dados_mesa.numero_jogador_vencedor);

            dados_mesa.numero_jogadores = estado_jogo.get_numero_jogadores();
            dados_mesa.jogadores = (_dados_revelados_jogador*)malloc(dados_mesa.numero_jogadores*sizeof(*dados_mesa.jogadores));

            tamanho_mensagem += dados_mesa.numero_jogadores*(sizeof(dados_mesa.jogadores->numero_jogador) + sizeof(dados_mesa.jogadores->n_dados));

            for(unsigned int i = 0; i < dados_mesa.numero_jogadores; ++i)
            {   
                dados_mesa.jogadores[i].numero_jogador = estado_jogo.get_lista_jogadores()[i].get_numero();
                n_dados_jogador = estado_jogo.get_lista_jogadores()[i].get_mao().get_numero_dados();
                dados_mesa.jogadores[i].n_dados = n_dados_jogador;
                dados_mesa.jogadores[i].valor_dados = (int*)malloc(n_dados_jogador*sizeof(*dados_mesa.jogadores->valor_dados));
                tamanho_mensagem += n_dados_jogador*sizeof(*dados_mesa.jogadores->valor_dados);

                for(unsigned j = 0; j < n_dados_jogador; ++j)
                    dados_mesa.jogadores[i].valor_dados[j] = estado_jogo.get_lista_jogadores()[i].get_mao().get_dados()[j].get_valor();   
            }

            // Caso o jogador tenha errado em sua contestação, ele perde um dado e o turno volta ao jogador anterior.
            if(checa_dados_mesa(estado_jogo) != MENOS)
            {
                estado_jogo.tirar_dado_jogador(numero_jogador_atual);
                dados_mesa.numero_jogador_vencedor = estado_jogo.get_jogador_aposta_atual();
                turno_jogador--;
            }

            // Caso contrário o último jogador perde um dado e o turno é novamente do jogador atual
            else
            {
                estado_jogo.tirar_dado_ultimo_jogador();
                dados_mesa.numero_jogador_vencedor = numero_jogador_atual;
            }

            // Faz o broadcast dos dados revelados de todos os jogadores.
            for(unsigned int i = 0; i < estado_servidor.jogadores.size(); ++i)
                enviar_mensagem(estado_servidor.jogadores[i]->socket, &dados_mesa, tamanho_mensagem, Revela_mesa);
        
            for(unsigned int i = 0; i < dados_mesa.numero_jogadores; ++i)
                free(dados_mesa.jogadores[i].valor_dados);
            
            free(dados_mesa.jogadores);

            // Reseta o estado da mesa para uma nova rodada.
            estado_jogo.set_aposta(Aposta(), 0);
            estado_jogo.aleatorizar_maos(LIMITE_FACE);
            estado_jogo.resetar_ultimas_jogadas();
        }
        break;

        case CRAVAR:
        {
            // Monta a mensagem de dados da mesa para envio.
            Revela_mesa_msg dados_mesa;
            unsigned int n_dados_jogador;
            unsigned int tamanho_mensagem = sizeof(dados_mesa.numero_jogadores) + sizeof(dados_mesa.numero_jogador_vencedor);

            dados_mesa.numero_jogadores = estado_jogo.get_numero_jogadores();
            dados_mesa.jogadores = (_dados_revelados_jogador*)malloc(dados_mesa.numero_jogadores*sizeof(*dados_mesa.jogadores));

            tamanho_mensagem += dados_mesa.numero_jogadores*(sizeof(dados_mesa.jogadores->numero_jogador) + sizeof(dados_mesa.jogadores->n_dados));

            for(unsigned int i = 0; i < dados_mesa.numero_jogadores; ++i)
            {   
                dados_mesa.jogadores[i].numero_jogador = estado_jogo.get_lista_jogadores()[i].get_numero();
                n_dados_jogador = estado_jogo.get_lista_jogadores()[i].get_mao().get_numero_dados();
                dados_mesa.jogadores[i].n_dados = n_dados_jogador;
                dados_mesa.jogadores[i].valor_dados = (int*)malloc(n_dados_jogador*sizeof(*dados_mesa.jogadores->valor_dados));
                tamanho_mensagem += n_dados_jogador*sizeof(*dados_mesa.jogadores->valor_dados);

                for(unsigned j = 0; j < n_dados_jogador; ++j)
                    dados_mesa.jogadores[i].valor_dados[j] = estado_jogo.get_lista_jogadores()[i].get_mao().get_dados()[j].get_valor();  
            }
             
            // Caso o jogador tenha acertado em sua cravada, todos os jogadores menos ele perdem um dado e o turno é dele novamente.
            if(checa_dados_mesa(estado_jogo) == EXATOS)
            {
                for(unsigned int jogadores = 0; jogadores < estado_jogo.get_numero_jogadores(); jogadores++)
                {
                    if(estado_jogo.get_lista_jogadores()[jogadores].get_numero() != numero_jogador_atual)
                        estado_jogo.tirar_dado_jogador(estado_jogo.get_lista_jogadores()[jogadores].get_numero());
                }

                dados_mesa.numero_jogador_vencedor = numero_jogador_atual;
            }

            // Caso contrário, ele perde um dado e o turno volta ao jogador anterior.
            else
            {
                turno_jogador--;
                estado_jogo.tirar_dado_jogador(numero_jogador_atual);
                dados_mesa.numero_jogador_vencedor = estado_jogo.get_jogador_aposta_atual();
            }

            for(unsigned int i = 0; i < estado_servidor.jogadores.size(); ++i)
                enviar_mensagem(estado_servidor.jogadores[i]->socket, &dados_mesa, tamanho_mensagem, Revela_mesa);
        
            for(unsigned int i = 0; i < dados_mesa.numero_jogadores; ++i)
                free(dados_mesa.jogadores[i].valor_dados);
        
            free(dados_mesa.jogadores);
                    
            // Reseta o estado da mesa para uma nova rodada.
            estado_jogo.set_aposta(Aposta(), 0);
            estado_jogo.aleatorizar_maos(LIMITE_FACE);
            estado_jogo.resetar_ultimas_jogadas();
        }
        break;

        default:
        break;
    }

    return 0;
}

bool checa_acao_valida(Acao_resposta& acao,const Estado &estado_jogo)
{
    // Checa se a ação tomada é possível.
    switch(acao.acao_escolhida)
    {
        case AUMENTAR_APOSTA:
        {
            // Se a aposta não quebrar nenhuma das regras ela é válida.
            int quantia = acao.valores_acao[0], valor = acao.valores_acao[1];
            Aposta nova_aposta(quantia, valor);

            if(checa_aposta_valida(nova_aposta, estado_jogo))
                return true;
        }
        break;

        case DUVIDAR:
        {
            // Contanto que hajam apostas a ação é válida.
            if (estado_jogo.get_aposta() == Aposta())
                return false;
            
            return true;
        }
        break;

        case CRAVAR:
        {
            // Contanto que hajam apostas a ação é válida.
            if (estado_jogo.get_aposta() == Aposta())
                return false;
            
            return true;
        }
        break;

        default:
        break;
    }

    return false;
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

    // Se a aposta envolver menos que um dado, ela é inválida.
    if(numero_dados < 1)
        return false;

    // Se a face dos dados da aposta não estiver entre um e o limite, ela é inválida.
    if(dado_aposta < 1 || dado_aposta > LIMITE_FACE)
        return false;

    // Se não houverem jogadas ainda, a aposta é válida.
    if(lista_jogadas.size() < 1)
        return true;


    switch (valor_dados)
    {
        default:
        {
            // Caso a aposta tenha um número menor de dados que a anterior e aqueles não forem bagos, então ela é inválida.
            if(numero_dados < lista_jogadas.back().get_aposta().get_numero_dados() && lista_jogadas.back().get_aposta().get_dados().get_valor() != BAGO)
                return false;
            
            // Se estes anteriores forem bagos e o valor for menor que o dobro, então ela também é inválida
            if(numero_dados < lista_jogadas.back().get_aposta().get_numero_dados() * 2 && lista_jogadas.back().get_aposta().get_dados().get_valor() == BAGO)
                return false;

            
            /* Caso a aposta repita um número de dados já anunciado duas vezes anteriormente, excluindo no caso de bagos, então ela é inválida.
            Além disso, se ela for igual a qualquer uma das últimas apostas, ela é inválida. (Só será necessário checar as últimas três jogadas)*/
            for(int i = 0; i < 3 && i < (int)lista_jogadas.size(); ++i)
            {
                if(numero_dados == lista_jogadas[i].get_aposta().get_numero_dados() && lista_jogadas[i].get_aposta().get_dados().get_valor() != BAGO)
                    contagem_dados_iguais++;
                
                igual_aposta_anterior = false || (nova_aposta == lista_jogadas[i].get_aposta());
            }
    
        }
        break;

        // Fazemos verificações diferentes no caso de uma aposta envolvendo bagos.
        case BAGO:
        {
            // Caso a aposta seja menor que o número de dados anterior e eles também forem bagos, então ela é inválida
            if(numero_dados < lista_jogadas.back().get_aposta().get_numero_dados() && lista_jogadas.back().get_aposta().get_dados().get_valor() == BAGO)
                return false;
            
            // Caso os dados anteriores não forem bagos, se o dobro da aposta for menor que o número de dados anterior, então ela é inválida
            if(2 * numero_dados < lista_jogadas.back().get_aposta().get_numero_dados() && lista_jogadas.back().get_aposta().get_dados().get_valor() != BAGO)
                return false;
            
            // Se a aposta for igual a qualquer uma das três últimas, ela é inválida.
            for(int i = 0; i < 3 && i < (int)lista_jogadas.size(); ++i)
                igual_aposta_anterior = false || (nova_aposta == lista_jogadas[i].get_aposta());
        }
        break;
    }

    // Última checagem citada nos dois casos.
    if (contagem_dados_iguais >= 2 || igual_aposta_anterior)
        return false;

    // Se nenhuma das verificações de invalidade forem verdadeiras, então é uma aposta válida.
    return true;
}

int checa_dados_mesa(const Estado &estado_jogo)

{
    // Checagem dos dados da mesa em comparação à aposta atual.
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


void gera_ordem_aleatoria()
{
    // Embaralha o vetor de jogadores.
    std::random_device rd;
    std::mt19937 gen(rd());

    shuffle(estado_servidor.jogadores.begin(), estado_servidor.jogadores.end(), gen);
}

unsigned int checa_vencedor(const Estado &estado_jogo)
{
    // Checa se sobrou somente um jogador com dados na mesa.
    unsigned int numero_jogadores = estado_jogo.get_numero_jogadores();
    unsigned int numero_perdedores = 0;
    unsigned int numero_jogador_vencedor = 0;

    for(unsigned int i = 0; i < numero_jogadores; ++i)
    {
        if(estado_jogo.get_lista_jogadores()[i].get_mao().get_numero_dados() <= 0)
            numero_perdedores++;
        
        else
            numero_jogador_vencedor = estado_jogo.get_lista_jogadores()[i].get_numero();
    }

    return (numero_jogadores - numero_perdedores) <= 1 ? numero_jogador_vencedor : 0;
}

void reseta_estado(Estado& estado_jogo)
{
    // Reinicia o estado do jogo.
    estado_jogo.set_turno_atual(0);
    estado_jogo.resetar_maos(LIMITE_FACE);
    estado_jogo.aleatorizar_maos(LIMITE_FACE);
    estado_jogo.resetar_ultimas_jogadas();
    estado_jogo.set_aposta(Aposta(), 0);
}

void envia_estado_mesa(const Estado &estado_jogo)
{
    // A estruturação da mensagem de estado será feita
    Estado_mesa_msg estado_novo = {0};
    unsigned int tamanho_mensagem = sizeof(estado_novo.rodada) + sizeof(estado_novo.n_dados_aposta) + sizeof(estado_novo.face_dados_aposta) + sizeof(estado_novo.jogador_atual);
    bool sucesso_operacao = true;

    estado_novo = 
    {
        .rodada = estado_jogo.get_turno_atual(),
        .n_dados_aposta = estado_jogo.get_aposta().get_numero_dados(),
        .face_dados_aposta = (unsigned int)estado_jogo.get_aposta().get_dados().get_valor(),
        .jogador_atual = estado_jogo.get_jogador_turno_atual()
    };

    // Envia em broadcast para os jogadores e anota aqueles que não receberam como desconectados.
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
    // Definição da estrutura da mensagem.
    Estado_inicial_msg estado_inicial = {.header = {}};
    unsigned int tamanho_mensagem = 0, tamanho_mensagem_atual;
    unsigned int numero_jogadores = estado_servidor.jogadores.size();
    unsigned int n_dados_jogador_atual = 0;
    bool sucesso_operacao = true;

    // Definição e cálculo do tamanho da mensagem a ser enviada.
    estado_inicial.numero_jogadores = numero_jogadores;
    estado_inicial.dados_jogadores = (_dados_jogador*)malloc(numero_jogadores * sizeof(_dados_jogador));
    tamanho_mensagem = sizeof(estado_inicial.numero_jogadores) + numero_jogadores*(sizeof(estado_inicial.dados_jogadores->n_dados) + sizeof(estado_inicial.dados_jogadores->numero_jogador)) + sizeof(estado_inicial.numero_jogador_atual);

    for (unsigned int i = 0; i < numero_jogadores; ++i)
    {
        estado_inicial.dados_jogadores[i].numero_jogador = estado_servidor.jogadores[i]->numero;
        estado_inicial.dados_jogadores[i].n_dados = estado_jogo.get_jogador(estado_servidor.jogadores[i]->numero).get_mao().get_numero_dados();
    }

    for (unsigned int i = 0; i < numero_jogadores; ++i)
    {
        // Cálculo do tamanho da mensagem a depender do número de dados do jogador cliente.
        tamanho_mensagem_atual = tamanho_mensagem;
        estado_inicial.numero_jogador_atual = estado_servidor.jogadores[i]->numero;
        n_dados_jogador_atual = estado_jogo.get_jogador(estado_servidor.jogadores[i]->numero).get_mao().get_numero_dados();
        estado_inicial.valor_dados_jogador = (int*)malloc(n_dados_jogador_atual * sizeof(*estado_inicial.valor_dados_jogador));
        Jogador_cliente* jogador_atual = busca_jogador(estado_inicial.numero_jogador_atual);

        for(unsigned int j = 0; j < n_dados_jogador_atual; ++j)
            estado_inicial.valor_dados_jogador[j] = estado_jogo.get_jogador(estado_inicial.numero_jogador_atual).get_mao().get_dados()[j].get_valor();
        
        tamanho_mensagem_atual += n_dados_jogador_atual*sizeof(*estado_inicial.valor_dados_jogador);
        
        // Envio em broadcast e anotação de jogadores que não receberam a mensagem como desconectados.
        sucesso_operacao = enviar_mensagem(jogador_atual->socket, &estado_inicial, tamanho_mensagem_atual, Estado_inicial);

        if(!sucesso_operacao)
            estado_servidor.jogadores[i]->conectado = false;
        
        free (estado_inicial.valor_dados_jogador);
    }

    free (estado_inicial.dados_jogadores);
}

bool checa_conexoes(Estado &estado_jogo, unsigned int jogador_atual)
{
    // Definição da estrutura de mensagem de desconexões.
    Desconexao_msg mensagem_desconexao = {};
    bool houve_desconexoes = false;
    unsigned int jogadores_desconectados[NUMERO_JOGADORES];
    unsigned int numero_desconectados = 0;
    unsigned tamanho_mensagem = sizeof(unsigned int);
    unsigned int numero_jogador_removido_atual;
    bool sucesso_operacao = true;

    // Envio de heartbeats para cada jogador e anotação dos que não receberam como desconectados.
    std::cout << "Enviando heartbeats...\n";
    for(unsigned int i = 0; i < estado_servidor.jogadores.size(); ++i)
    {
        sucesso_operacao = enviar_mensagem(estado_servidor.jogadores[i]->socket, NULL, 0, Heartbeat);

        if(!sucesso_operacao)
            estado_servidor.jogadores[i]->conectado = false;
    }

    // Checa todos os jogadores registrados como desconectados e os salva em um buffer.  
    std::cout << "Checando desconexoes!\n";
    for(unsigned int i = 0; i < estado_servidor.jogadores.size(); ++i)
        if(!estado_servidor.jogadores[i]->conectado)
        {   
            numero_jogador_removido_atual = estado_servidor.jogadores[i]->numero;
            jogadores_desconectados[numero_desconectados++] = numero_jogador_removido_atual;
        }
    
    // Termina a execução se não houveram jogadores desconectados.
    if(numero_desconectados == 0)
    {
        std::cout << "Nenhuma desconexao!\n";
        return false;
    }

    // Caso contrário remove jogadores desconectados da variável de controle e do jogo.
    std::cout << "Removendo jogadores desconectados...\n";
    for(unsigned int i = 0; i < numero_desconectados; ++i)
    {
        
        desligar_jogador_cliente(jogadores_desconectados[i]);
        estado_jogo.remover_jogador(jogadores_desconectados[i]);
    }

    // Marca a flag de que houveram jogadores desconectados.
    houve_desconexoes = true;
    
    // Cálculo e definição da mensagem de desconexão
    tamanho_mensagem += sizeof(unsigned int) * numero_desconectados;
    
    mensagem_desconexao = {.quantia_desconectados = numero_desconectados, .numero_jogadores = jogadores_desconectados};

    for(unsigned int i = 0; i < estado_servidor.jogadores.size(); ++i)
    {
        // Envio em broadcast dos jogadores desconectados. Aqueles que não receberem novamente são marcados como desconectados para checagens futuras.
        sucesso_operacao = enviar_mensagem(estado_servidor.jogadores[i]->socket, &mensagem_desconexao, tamanho_mensagem, Desconexao);

        if(!sucesso_operacao)
            estado_servidor.jogadores[i]->conectado = false;
    }

    // Reinicia a rodada no caso de jogadores desconectados.
    estado_jogo.resetar_ultimas_jogadas();
    estado_jogo.aleatorizar_maos(LIMITE_FACE);
    estado_jogo.set_aposta(Aposta(), 0);

    return houve_desconexoes;
}

void desligar_jogador_cliente(unsigned int numero)
{
    // Encerra o socket e a thread do jogador desconectado e o retira da variável de controle.
    Jogador_cliente* jogador_desligado = busca_jogador(numero);
    shutdown(jogador_desligado->socket, SHUT_RDWR);
    sem_post(&jogador_desligado->semaforo);
    pthread_join(jogador_desligado->thread, nullptr);
    close(jogador_desligado->socket);

    for(unsigned int i = 0; i < estado_servidor.jogadores.size(); ++i)
        if(estado_servidor.jogadores[i]->numero == numero)
            estado_servidor.jogadores.erase(estado_servidor.jogadores.begin() + i);
}

Jogador_cliente* busca_jogador(unsigned int numero)
{
    // Busca por um jogador por número.
    for(unsigned int i = 0; i < estado_servidor.jogadores.size(); ++i)
        if(estado_servidor.jogadores[i]->numero == numero)
            return estado_servidor.jogadores[i].get();


    exit(1);
}