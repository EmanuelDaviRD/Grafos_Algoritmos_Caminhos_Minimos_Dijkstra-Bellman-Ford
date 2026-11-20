#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "GPS.H"

void limparBuffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int main() {
    Grafo *gps = criarGrafo(10);
    if (gps == NULL) {
        printf("Erro critico ao inicializar o sistema!\n");
        return 1;
    }

    int opcao, origem, destino, idCriado;
    float distancia;
    char nome[TAM_NOME];

    do {
        printf("   SISTEMA DE GERENCIAMENTO DE ROTAS    \n");
        printf("1 - Cadastrar cidade\n");
        printf("2 - Cadastrar estrada\n");
        printf("3 - Listar cidades e conexoes\n");
        printf("4 - Calcular menor caminho (Dijkstra)\n");
        printf("5 - Percorrer rede em largura (BFS)\n");
        printf("6 - Percorrer rede em profundidade (DFS)\n");
        printf("7 - Salvar dados em arquivo\n");
        printf("8 - Carregar dados do arquivo\n");
        printf("0 - Sair\n");
        printf("Escolha uma opcao: ");

        if (scanf("%d", &opcao) != 1) {
            limparBuffer();
            opcao = -1;
        } else {
            limparBuffer();
        }

        switch (opcao) {
            case 1:
                printf("Digite o nome da cidade: ");
                fgets(nome, sizeof(nome), stdin);
                nome[strcspn(nome, "\n")] = '\0';
                idCriado = cadastrarCidade(gps, nome);
                if (idCriado != -1) {
                    printf("Cidade '%s' cadastrada com ID [%d].\n", nome, idCriado);
                }
                break;

            case 2:
                if (gps->numCidades < 2) {
                    printf("Cadastre pelo menos 2 cidades antes de criar estradas!\n");
                    break;
                }
                listarCidades(gps);
                printf("Digite o ID da cidade de origem: ");
                scanf("%d", &origem);
                printf("Digite o ID da cidade de destino: ");
                scanf("%d", &destino);
                printf("Digite a distancia (km): ");
                scanf("%f", &distancia);
                limparBuffer();
                cadastrarEstrada(gps, origem, destino, distancia, 1);
                break;

            case 3:
                listarRede(gps);
                break;

            case 4:
                if (gps->numCidades < 2) {
                    printf("Cadastre pelo menos 2 cidades para calcular rotas!\n");
                    break;
                }
                listarCidades(gps);
                printf("Digite o ID da cidade de origem: ");
                scanf("%d", &origem);
                printf("Digite o ID da cidade de destino: ");
                scanf("%d", &destino);
                limparBuffer();
                calcularMenorCaminho(gps, origem, destino);
                break;

            case 5:
                if (gps->numCidades == 0) {
                    printf("Nenhuma cidade cadastrada!\n");
                    break;
                }
                listarCidades(gps);
                printf("Digite o ID da cidade inicial para BFS: ");
                scanf("%d", &origem);
                limparBuffer();
                executarBFS(gps, origem);
                break;

            case 6:
                if (gps->numCidades == 0) {
                    printf("Nenhuma cidade cadastrada!\n");
                    break;
                }
                listarCidades(gps);
                printf("Digite o ID da cidade inicial para DFS: ");
                scanf("%d", &origem);
                limparBuffer();
                executarDFS(gps, origem);
                break;

            case 7:
                salvarDados(gps, ARQUIVO_DADOS);
                break;

            case 8:
                if (carregarDados(gps, ARQUIVO_DADOS)) {
                    for (int i = 0; i < gps->numCidades; i++) {
                        char *sep = strchr(gps->cidades[i].nome, ';');
                        if (sep != NULL) {
                            *sep = '\0';
                        }
                    }
                }
                break;
                
            case 0:
                printf("Encerrando sistema GPS...\n");
                break;

            default:
                printf("Opcao invalida!\n");
                break;
        }
    } while (opcao != 0);

    liberarGrafo(gps);

    return 0;
}