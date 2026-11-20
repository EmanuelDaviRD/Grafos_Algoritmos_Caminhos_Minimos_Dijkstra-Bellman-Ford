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