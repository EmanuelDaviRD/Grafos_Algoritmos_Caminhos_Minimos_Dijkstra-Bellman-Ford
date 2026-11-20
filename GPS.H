#ifndef GPS_H
#define GPS_H

#define TAM_NOME 50
#define ARQUIVO_DADOS "dados_gps.txt"

typedef struct{
    int destino;
    float distancia;
    struct Aresta *prox;
} Aresta;

typedef struct{
    Cidade *cidades;
    char nome[TAM_NOME];
    Aresta *listaAdj;
} Cidade;

typedef struct{
    Cidade *cidades;
    int numCidades;
    int capacidade;
} Grafo;

Grafo *criarGrafo(int capacidadeInical);

#endif