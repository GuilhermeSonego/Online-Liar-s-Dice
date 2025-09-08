#include <vector>
#include <iostream>
#include "jogo.h"

Estado::Estado(unsigned int numero_jogadores, unsigned int tamanho_maos)
{
    this-> _tamanho_maos = tamanho_maos;
    this->_turno_atual = 0;
    this->_quantia_dados = numero_jogadores * tamanho_maos;
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
    << " | Dados restantes: " << _quantia_dados;
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