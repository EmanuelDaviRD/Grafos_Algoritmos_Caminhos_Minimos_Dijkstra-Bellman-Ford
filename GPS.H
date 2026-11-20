#ifndef GPS_H
#define GPS_H

#define TAM_NOME 50
#define ARQUIVO_DADOS "dados_gps.txt"

typedef struct Aresta {
    int destino;
    float distancia;
    struct Aresta *prox;
} Aresta;

typedef struct {
    int id;
    char nome[TAM_NOME];
    Aresta *listaAdj;
} Cidade;

typedef struct {
    Cidade *cidades;
    int numCidades;
    int capacidade;
} Grafo;

Grafo *criarGrafo(int capacidadeInicial);
int buscarCidadePorNome(Grafo *g, char *nome);
int cadastrarCidade(Grafo *g, char *nome);
void cadastrarEstrada(Grafo *g, int origem, int destino, float distancia, int exibirMensagem);
void listarCidades(Grafo *g);
void listarRede(Grafo *g);
void calcularMenorCaminho(Grafo *g, int origem, int destino);
void executarBFS(Grafo *g, int origem);
void executarDFS(Grafo *g, int origem);
void salvarDados(Grafo *g, char *nomeArquivo);
int carregarDados(Grafo *g, char *nomeArquivo);
void liberarGrafo(Grafo *g);

#endif