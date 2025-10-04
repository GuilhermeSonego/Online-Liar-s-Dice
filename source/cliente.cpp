#include "jogo.h"
#include "server.h"
#include "cliente.h"
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
#include <termios.h>


int main()
{
    Estado_cliente estado_jogo = {};
    //Definição de uma função responsável pelo desligamento do programa pela evocação do CTRL + C.
    signal(SIGINT, cleanup_cliente);

    sem_init(&conexao, 0, 0);

    pthread_create(&listener_thread, NULL, thread_listener, &estado_jogo);
    pthread_create(&talker_thread, NULL, thread_talker, NULL);

    while(conectado_ao_servidor.load())
    {
        if(!nova_acao)
            continue;
        
        print_estado(estado_jogo);

        nova_acao.store(false);
    }

    cleanup_cliente(SIGINT);
}

void cleanup_cliente(int sinal)
{
    conectado_ao_servidor.store(false);
    shutdown(socket_servidor, SHUT_RDWR);
    pthread_join(listener_thread, nullptr);
    pthread_join(talker_thread, nullptr);
    desbloqueia_teclado(false);
    exit(0);
}

void* thread_listener(void* parametros)
{
    int socket_servidor;
    struct sockaddr_in info_servidor;
    Mensagem mensagem_recebida;
    Estado_cliente* estado_atualizado = (Estado_cliente*)parametros;
    struct pollfd evento = {0};
    int retorno_evento;

    //Procedimentos para a criação do socket de conexao ao servidor.
    socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    info_servidor = {.sin_family = AF_INET, .sin_port = htons(PORTA)};

    //Checagens de conexao.
    if(inet_pton(AF_INET, "127.0.0.1", &info_servidor.sin_addr) <= 0)
    {
        std::cout << "Endereco invalido\n";
        conectado_ao_servidor.store(false);
    }

    if (connect(socket_servidor, (struct sockaddr*)&info_servidor, sizeof(info_servidor)) < 0)
    {
        std::cout << "Falha na conexão\n";
        conectado_ao_servidor.store(false);
    }

    evento = {.fd = socket_servidor, .events = POLLIN, .revents = 0};

    sem_post(&conexao);

    while (conectado_ao_servidor.load())
    {
        retorno_evento = poll(&evento, 1, 500);

        if(retorno_evento < 0)
            break;
        
        if(retorno_evento == 0)
            continue;
        
        // O atributo revents é um bitmap, portanto a comparação é feita com o operador bitwise AND
        if(evento.revents & POLLIN)
            mensagem_recebida = receber_mensagem(socket_servidor);

        switch(mensagem_recebida.tipo_mensagem)
        {
            case Falha:
            {
                conectado_ao_servidor.store(false);
            }
            break;

            case Estado_inicial:
            {
                Estado_inicial_msg estado_inicial = {0};
                estado_inicial = *(Estado_inicial_msg*)mensagem_recebida.conteudo_mensagem;

                estado_atualizado->numero_jogador_local = estado_inicial.numero_jogador_atual;

                for(unsigned int i = 0; i < estado_inicial.numero_jogadores; ++i)
                {

                    if(estado_inicial.dados_jogadores[i].numero_jogador == estado_inicial.numero_jogador_atual)
                        estado_atualizado->valor_dados_jogador.assign(estado_inicial.valor_dados_jogador, estado_inicial.valor_dados_jogador + estado_inicial.dados_jogadores[i].n_dados);

                    else
                    {
                        Jogadores_adversarios adversario =       
                        {
                            .numero = estado_inicial.dados_jogadores[i].numero_jogador,
                            .n_dados = estado_inicial.dados_jogadores[i].n_dados,
                            .conectado = true
                        };
                        estado_atualizado->adversarios.push_back(adversario); 
                    }

                    estado_atualizado->n_dados_total += estado_inicial.dados_jogadores[i].n_dados;
        
                }

                std::cout << "N_DADOS: " << estado_atualizado->n_dados_total 
                << '\n' << "N_JOGADOR: " << estado_atualizado->numero_jogador_local << '\n'
                << "DADOS: ";
                for (auto dado : estado_atualizado->valor_dados_jogador) {
                    std::cout << dado << " ";
                }
                std::cout << '\n';
            }
            break;

            case Estado_mesa:
            {
                Estado_mesa_msg estado_mesa = {0};
                estado_mesa = *(Estado_mesa_msg*)mensagem_recebida.conteudo_mensagem;
                
                estado_atualizado->jogador_atual = estado_mesa.jogador_atual;
                estado_atualizado->rodada = estado_mesa.rodada;
                estado_atualizado->n_dados_aposta = estado_mesa.n_dados_aposta;
                estado_atualizado->face_dados_aposta = estado_mesa.face_dados_aposta;

                nova_acao.store(true);
            }
            break;

            case Desconexao:
            {
                Desconexao_msg* lista_desconectados = (Desconexao_msg*)mensagem_recebida.conteudo_mensagem;

                std::cout << "houve desconexoes!\n";

                std::cout << "NUMERO DESCONECTADOS: " << lista_desconectados->quantia_desconectados << '\n';

                std::cout << "NUMERO DO DESCONECTADO: " << lista_desconectados->numero_jogadores[0] << '\n';
                for(unsigned int i = 0; i < estado_atualizado->adversarios.size(); ++i)
                    for(unsigned int j = 0; j < lista_desconectados->quantia_desconectados; ++j)
                        if(estado_atualizado->adversarios[i].numero == lista_desconectados->numero_jogadores[j])
                        {
                            estado_atualizado->adversarios[i].conectado = false;  
                            estado_atualizado->n_dados_total -= estado_atualizado->adversarios[i].n_dados;
                        }
                
                nova_acao.store(true);
            }
            break;

            default:
            break;
        }

        free_mensagem(mensagem_recebida);
    }

    std::cout << "Voce foi desconectado!\n";

    close(socket_servidor);
    return nullptr;
}

void* thread_talker(void* parametros)
{
    char buffer[MAXIMO_BUFFER];
    char escolha;
    int acao_tomada;
    struct pollfd evento = {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
    int retorno_evento;

    sem_wait(&conexao);

    desbloqueia_teclado(true);

    while(conectado_ao_servidor.load())
    {        
        retorno_evento = poll(&evento, 1, 500);

        if(retorno_evento < 0)
            break;
        
        if(retorno_evento == 0)
            continue;
        
        // O atributo revents é um bitmap, portanto a comparação é feita com o operador bitwise AND
        if(evento.revents & POLLIN)
            read(STDIN_FILENO, &escolha, 1);

        acao_tomada = (escolha - '0');

        switch(acao_tomada)
        {
            case AUMENTAR_APOSTA:
            {                
                Aposta_msg aposta_enviada;
                unsigned int tamanho_mensagem = sizeof(aposta_enviada.face_dados_aposta) + sizeof(aposta_enviada.n_dados_aposta);
             
                desbloqueia_teclado(false);

                std::cout << "Digite sua aposta: ";

                if(fgets(buffer, sizeof(buffer), stdin) == NULL)
                    break;

                if(sscanf(buffer, "%dx%d", &aposta_enviada.n_dados_aposta, &aposta_enviada.face_dados_aposta) == 2)
                {
                    std::cout << "Aposta enviada: " << aposta_enviada.n_dados_aposta << "x" << aposta_enviada.face_dados_aposta << '\n';
                    std::cout << "Texto lido: " << buffer << '\n';
                    enviar_mensagem(socket_servidor, &aposta_enviada, tamanho_mensagem, Aumento_aposta);
                }

                desbloqueia_teclado(true);

            }
            break;

            case DUVIDAR:
            {
                enviar_mensagem(socket_servidor, NULL, 0, Duvida);
            }
            break;

            case CRAVAR:
            {
                enviar_mensagem(socket_servidor, NULL, 0, Cravada);
            }
            break;

            case SAIR:
            {
                std::cout << "saindo...\n";
                conectado_ao_servidor.store(false);
            }
            break;

            default:
            break;
        }
    }

    return nullptr;
}


// Desbloqueia o teclado para que as opções sejam acessíveis simplesmente ao apertar as teclas
void desbloqueia_teclado(bool ativar) 
{
    static struct termios oldt, newt;
    if (ativar) 
    {
        tcgetattr(STDIN_FILENO, &oldt);  // Salva as configurações atuais do teclado
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO); // Desligada o modo canônico (necessidade de apertar enter) e eco
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } 
    else 
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restaura as configurações originais do teclado
}

// void print_estado(const Estado_cliente& estado) 
// {
//     using std::cout;
//     using std::endl;

//     // Cabeçalho do estado
//     cout << "\n╔══════════════════════════════════╗\n";
//     cout << "║         ESTADO DO JOGO           ║\n";
//     cout << "╚══════════════════════════════════╝\n\n";

//     // Informplkjnhbv ações da rodada
//     cout << "Rodada: " << estado.rodada << endl;
//     cout << "Jogador atual: " << estado.jogador_atual << endl;
//     cout << "Dados totais na mesa: " << estado.n_dados_total << endl;

//     if (estado.n_dados_aposta > 0) {
//         cout << "Última aposta: " 
//              << estado.n_dados_aposta << " dado(s) de face " 
//              << estado.face_dados_aposta << endl;
//     } else {
//         cout << "Nenhuma aposta feita ainda." << endl;
//     }

//     // Seus dados
//     cout << "\n╔══════════════════════════════════╗\n";
//     cout << "║          SEUS DADOS              ║\n";
//     cout << "╚══════════════════════════════════╝\n";

//     cout << "Jogador " << estado.numero_jogador_local << ": [ ";
//     for (int valor : estado.valor_dados_jogador) {
//         cout << valor << " ";
//     }
//     cout << "]" << endl;

//     // Adversários
//     cout << "\n╔══════════════════════════════════╗\n";
//     cout << "║          ADVERSÁRIOS             ║\n";
//     cout << "╚══════════════════════════════════╝\n";

//     for (size_t i = 0; i < NUMERO_JOGADORES-1; i++) // lista_jogadores tem NUMERO_JOGADORES-1
//     {
//         const Jogadores_adversarios& adversario = estado.adversarios[i];

//         if (adversario.conectado) {
//             cout << "Jogador " << adversario.numero
//                  << " -> Dados restantes: " << adversario.n_dados << endl;
//         } else {
//             cout << "Jogador " << adversario.numero
//                  << " -> DESCONECTADO" << endl;
//         }
//     }

//     cout << "\n════════════════════════════════════\n" << endl;
// }

void print_estado(const Estado_cliente& estado) 
{
    using std::cout;
    using std::endl;

    // Cabeçalho do estado
    cout << "\n╔══════════════════════════════════╗\n";
    cout << "║         ESTADO DO JOGO           ║\n";
    cout << "╚══════════════════════════════════╝\n\n";

    // Informações da rodada
    cout << "Rodada: " << estado.rodada << endl;
    cout << "Jogador atual: " << estado.jogador_atual << endl;
    cout << "Dados totais na mesa: " << estado.n_dados_total << endl;

    if (estado.n_dados_aposta > 0) {
        cout << "Última aposta: " 
             << estado.n_dados_aposta << " dado(s) de face " 
             << estado.face_dados_aposta << endl;
    } else {
        cout << "Nenhuma aposta feita ainda." << endl;
    }

    // Seus dados
    cout << "\n╔══════════════════════════════════╗\n";
    cout << "║          SEUS DADOS              ║\n";
    cout << "╚══════════════════════════════════╝\n";

    cout << "Jogador " << estado.numero_jogador_local << ": [ ";
    for (int valor : estado.valor_dados_jogador) {
        cout << valor << " ";
    }
    cout << "]" << endl;

    // Adversários
    cout << "\n╔══════════════════════════════════╗\n";
    cout << "║          ADVERSÁRIOS             ║\n";
    cout << "╚══════════════════════════════════╝\n";

    for (size_t i = 0; i < NUMERO_JOGADORES-1; i++) // lista_jogadores tem NUMERO_JOGADORES-1
    {
        const Jogadores_adversarios& adversario = estado.adversarios[i];

        if (adversario.conectado) {
            cout << "Jogador " << adversario.numero
                 << " -> Dados restantes: " << adversario.n_dados << endl;
        } else {
            cout << "Jogador " << adversario.numero
                 << " -> DESCONECTADO" << endl;
        }
    }

    // Menu de ações
    cout << "\n╔══════════════════════════════════╗\n";
    cout << "║        SUAS OPÇÕES (teclas)      ║\n";
    cout << "╚══════════════════════════════════╝\n";

    cout << "[1] Fazer aposta\n";
    cout << "[2] Contestar aposta\n";
    cout << "[3] Cravar aposta\n";
    cout << "[4] Sair do jogo\n";

    cout << "\n════════════════════════════════════\n" << endl;
}