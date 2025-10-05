#include <vector>
#include <iostream>
#include <algorithm>
#include "jogo.h"

Estado::Estado(unsigned int numero_jogadores, unsigned int tamanho_maos)
{
    this-> _tamanho_maos = tamanho_maos;
    this->_turno_atual = 0;
    this->_jogador_aposta_atual = 0;
    this->_aposta_atual = Aposta();

    this->_jogadores.reserve(numero_jogadores);

    for(unsigned int i = 0; i < numero_jogadores; ++i)
        this->_jogadores.emplace_back(i+1, tamanho_maos);
}

void Estado::print_obj() const
{   
    std::cout << "Jogadores: " << _jogadores.size()
    << " | Tamanho mãos: " << _tamanho_maos
    << " | Turno atual: " << _turno_atual << "\n";
}

void Estado::print_tudo() const
{
    std::cout << "Jogadores: " << _jogadores.size()
    << " | Tamanho mãos: " << _tamanho_maos
    << " | Turno atual: " << _turno_atual
    << " | Dados restantes: " << this->get_dados_mesa();
    for(unsigned int j = 0; j < _jogadores.size(); ++j)
    {
        std::cout << "| Mao " << j+1 << ": ";
        for(unsigned int i = 0; i < _jogadores[j].get_mao().get_numero_dados(); ++i)
            std::cout << _jogadores[j].get_mao().get_dados()[i].get_valor();

    }
    std::cout << std::endl;
}

void Dado::aleatorizar_valor(unsigned int limite_face)
{
    std::random_device rd;  
    std::mt19937 gen(rd()); 
    std::uniform_int_distribution<> distrib(1, limite_face);

    this->_valor = distrib(gen);
}

void Mao::aleatorizar(unsigned int limite_face)
{
    this->_limite_face = limite_face;
    for(unsigned int i = 0; i < this->get_numero_dados(); ++i)
        this->_dados[i].aleatorizar_valor(limite_face);
}

void Estado::aleatorizar_maos(unsigned int quantidade_lados)
{
    for(unsigned int i = 0; i < this->_jogadores.size(); ++i)
        this->_jogadores[i].aleatorizar_mao(quantidade_lados);    
}

Jogador& Estado::get_jogador(unsigned int numero)
{
    auto compara_jogadores = [](const Jogador& primeiro, const Jogador& segundo) 
    {
        return primeiro.get_numero() < segundo.get_numero();
    };

    auto iterador = std::lower_bound(this->_jogadores.begin(), this->_jogadores.end(), numero, compara_jogadores);

    return *iterador;
}

const Jogador& Estado::get_jogador(unsigned int numero) const
{
    auto compara_jogadores = [](const Jogador& primeiro, const unsigned int& valor) 
    {
        return primeiro.get_numero() < valor;
    };

    auto iterador = std::lower_bound(this->_jogadores.begin(), this->_jogadores.end(), numero, compara_jogadores);

    return *iterador;
}

Mao::Mao(int* valores, unsigned int numero_dados)
{
    this->_dados.resize(numero_dados);

    for(unsigned int i = 0; i < numero_dados; ++i)
        this->_dados[i].set_valor(valores[i]);
}

const unsigned int Estado::get_dados_mesa() const
{
    unsigned int quantia = 0;

    for(unsigned int i = 0; i < this->get_numero_jogadores(); ++i)
        quantia += this->_jogadores[i].get_mao().get_numero_dados();
    
    return quantia;
}

void Estado::remover_jogador(unsigned int numero)
{
    for(unsigned int i = 0; i < this->_jogadores.size(); ++i)
        if(this->_jogadores[i].get_numero() == numero)
            this->_jogadores.erase(this->_jogadores.begin() + i);
}