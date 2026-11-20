#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "GPS.H"

typedef struct NoAux {
    int vertice;
    struct NoAux *prox;
} NoAux;

typedef struct{
    NoAux *inicio;
    NoAux *fim;
} FilaAux;

typedef struct{
    NoAux *topo;
} PilhaAux;

void initFila(FilaAux *f){
    f->inicio = NULL;
    f->fim = NULL;
}

void enqueue(FilaAux *f, int v){
    NoAux *novo = malloc(sizeof(NoAux));
    if(novo = NULL) return;
    novo->vertice = v;
    novo->prox = NULL;
    if(f->inicio == NULL)
        f->inicio = novo;
    else
        f->fim->prox = novo;
    f->fim = novo;
}

int dequeue(FilaAux *f){
    if(f->inicio == NULL) return -1;
    NoAux *temp = f->inicio;
    int v = temp->vertice;
    f->inicio = f->inicio->prox;
    if(f->inicio == NULL) f->fim = NULL;
    free(temp);
    return v;
}

void initPilha(PilhaAux *p){
    p->topo = NULL;
}

void push(PilhaAux *p, int v){
    NoAux *novo = malloc(sizeof(NoAux));
    if(novo == NULL) return;
    novo->vertice = v;
    novo->prox = p->topo;
    p->topo = novo;
}

int pop(PilhaAux *p){
    if(p->topo == NULL) return -1;
    NoAux *temp = p->topo;
    int v =temp->vertice;
    p->topo = p->topo->prox;
    free(temp);
    return v;
}

Grafo *criarGrafo(int capacidadeInicial){
    Grafo *g = malloc(sizeof(Grafo));
    if(g = NULL) return NULL;

    g->cidades = malloc(capacidadeInicial * sizeof(Cidade));
    if(g->cidades == NULL){
        free(g);
        return NULL;
    }

    g->numCidades = 0;
    g->capacidade = capacidadeInicial;
    return g;
}

int buscarCidadePorNome(Grafo *g, char *nome){
    for(int i = 0; i < g->numCidades; i++){
        if(strcmp(g->cidades[i].nome, nome) == 0)
            return i;
    }
    return -1;
}

int cadastrarCidade(Grafo *g, char *nome){
    if(strlen(nome) == 0){
        printf("Nome invalido!\n");
        return -1;
    }

    if(buscarCidadePorNome(g, nome) != -1){
       printf("Cidade '%s' ja esta cadastrada!\n",nome); 
        return -1;
    }

    if(g->numCidades == g->capacidade){
        int novaCapacidade = g->capacidade * 2;
        Cidade *temp = realloc(g->cidades, novaCapacidade * sizeof(Cidade));
        if(temp == NULL){
            printf("Erro ao realocar memoria para cidades!\n");
            return -1;
        }
        g->cidades = temp;
        g->capacidade = novaCapacidade;
    }

    int id = g->numCidades;
    g->cidades[id].id = id;
    strncpy(g->cidades[id].nome,nome, TAM_NOME - 1);
    g->cidades[id].nome[TAM_NOME - 1] = '\0';
    g->cidades[id].listaAdj = NULL;
    g->numCidades++;

    return id;
}

void adicionarArestaDirecionada(Grafo *g, int origem, int destino, float distancia){
    Aresta *aux = g->cidades[origem].listaAdj;
    while(aux != NULL){
        if(aux-destino == destino){
            aux->distancia = distancia;
            return;
        }
        aux = aux->prox;
    }

    Aresta *nova = malloc(sizeof(Aresta));
    if(nova == NULL){
        printf("Erro ao alocar memoria para estrada!\n");
        return;
    }
    
    nova->destino = destino;
    nova->distancia = distancia;
    nova->prox = g->cidades[origem].listaAdj;
    g->cidades[origem].listaAdj = nova;
}

void cadastrarEstrada(Grafo *g, int origem, int destino, float distancia, int exibirMensagem) {
    if (origem < 0 || origem >= g->numCidades || destino < 0 || destino >= g->numCidades) {
        if (exibirMensagem) printf("ID de cidade invalido!\n");
        return;
    }

    if (origem == destino) {
        if (exibirMensagem) printf("Origem e destino devem ser cidades diferentes!\n");
        return;
    }

    if (distancia <= 0) {
        if (exibirMensagem) printf("A distancia deve ser maior que zero!\n");
        return;
    }

    adicionarArestaDirecionada(g, origem, destino, distancia);
    adicionarArestaDirecionada(g, destino, origem, distancia);

    if (exibirMensagem) {
        printf("Estrada cadastrada: %s <-> %s (%.2f km)\n",
               g->cidades[origem].nome, g->cidades[destino].nome, distancia);
    }
}

void listarCidades(Grafo *g){
    if(g->numCidades == 0){
        printf("Nenhuma cidade cadastrada.\n");
        return;
    }

    printf("\nCIDADES CADASTRADAS\n");
    for(int i = 0 ;i < g->numCidades; i++){
        printf("[%d] %s\n",g->cidades[i].id, g->cidades[i].nome);
    }
}

