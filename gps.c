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

void listarRede(Grafo *g) {
    if (g->numCidades == 0) {
        printf("Rede vazia! Nenhuma cidade cadastrada.\n");
        return;
    }

    printf("\nMAPA DA REDE (LISTA DE ADJACENCIA)\n");
    for (int i = 0; i < g->numCidades; i++) {
        printf("[%d] %s -> ", g->cidades[i].id, g->cidades[i].nome);
        Aresta *aux = g->cidades[i].listaAdj;
        if (aux == NULL) {
            printf("Sem conexoes");
        }
        while (aux != NULL) {
            printf("%s (%.2f km)", g->cidades[aux->destino].nome, aux->distancia);
            if (aux->prox != NULL) printf(" | ");
            aux = aux->prox;
        }
        printf("\n");
    }
}

void calcularMenorCaminho(Grafo *g, int origem, int destino) {
    if (origem < 0 || origem >= g->numCidades || destino < 0 || destino >= g->numCidades) {
        printf("ID de cidade invalido!\n");
        return;
    }

    int n = g->numCidades;
    float *dist = malloc(n * sizeof(float));
    int *anterior = malloc(n * sizeof(int));
    int *visitado = malloc(n * sizeof(int));

    if (dist == NULL || anterior == NULL || visitado == NULL) {
        free(dist);
        free(anterior);
        free(visitado);
        printf("Erro de alocacao de memoria!\n");
        return;
    }

    for (int i = 0; i < n; i++) {
        dist[i] = FLT_MAX;
        anterior[i] = -1;
        visitado[i] = 0;
    }

    dist[origem] = 0.0f;

    for (int count = 0; count < n; count++) {
        int u = -1;
        float menorDist = FLT_MAX;

        for (int i = 0; i < n; i++) {
            if (!visitado[i] && dist[i] < menorDist) {
                menorDist = dist[i];
                u = i;
            }
        }

        if (u == -1 || u == destino) break;

        visitado[u] = 1;

        Aresta *adj = g->cidades[u].listaAdj;
        while (adj != NULL) {
            int v = adj->destino;
            if (!visitado[v] && dist[u] + adj->distancia < dist[v]) {
                dist[v] = dist[u] + adj->distancia;
                anterior[v] = u;
            }
            adj = adj->prox;
        }
    }

    printf("\nRESULTADO DA ROTA (DIJKSTRA)\n");
    if (dist[destino] == FLT_MAX) {
        printf("Nao existe caminho entre %s e %s.\n",
               g->cidades[origem].nome, g->cidades[destino].nome);
    } else {
        int *caminho = (int *) malloc(n * sizeof(int));
        int tamCaminho = 0;
        int atual = destino;

        while (atual != -1) {
            caminho[tamCaminho++] = atual;
            atual = anterior[atual];
        }

        printf("Trajeto: ");
        for (int i = tamCaminho - 1; i >= 0; i--) {
            printf("%s", g->cidades[caminho[i]].nome);
            if (i > 0) printf(" -> ");
        }
        printf("\nCusto total: %.2f km\n", dist[destino]);
        free(caminho);
    }

    free(dist);
    free(anterior);
    free(visitado);
}

void executarBFS(Grafo *g, int origem) {
    if (origem < 0 || origem >= g->numCidades) {
        printf("ID de cidade invalido!\n");
        return;
    }

    int *visitado = (int *) calloc(g->numCidades, sizeof(int));
    if (visitado == NULL) return;

    FilaAux f;
    initFila(&f);

    visitado[origem] = 1;
    enqueue(&f, origem);

    printf("\n--- BUSCA EM LARGURA (BFS) A PARTIR DE %s ---\n", g->cidades[origem].nome);
    while (f.inicio != NULL) {
        int u = dequeue(&f);
        printf("%s", g->cidades[u].nome);

        Aresta *adj = g->cidades[u].listaAdj;
        while (adj != NULL) {
            int v = adj->destino;
            if (!visitado[v]) {
                visitado[v] = 1;
                enqueue(&f, v);
            }
            adj = adj->prox;
        }

        if (f.inicio != NULL) printf(" -> ");
    }
    printf("\n");

    free(visitado);
}
