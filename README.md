# Sistema de Navegação GPS em C (Grafos)

Projeto desenvolvido para a disciplina de **Estrutura de Dados**, modelando uma malha rodoviária nacional por meio de **Grafos ponderados representados por Lista de Adjacência**, com cálculo de rotas mínimas e algoritmos clássicos de percurso.

## Estruturas e Algoritmos Implementados

* **Lista de Adjacência Dinâmica:** Vértices (`Cidade`) armazenados em vetor dinâmico com expansão automática (`realloc`) e arestas (`Aresta`) encadeadas contendo o destino e a distância em quilômetros.
* **Algoritmo de Dijkstra:** Determina o menor caminho e o custo total em quilômetros entre quaisquer duas cidades conectadas na rede.
* **Busca em Largura (BFS):** Explora a malha rodoviária por níveis a partir de uma cidade inicial utilizando uma **Fila** dinâmica (`FilaAux`).
* **Busca em Profundidade (DFS):** Percorre os ramos da malha rodoviária utilizando uma **Pilha** dinâmica (`PilhaAux`).
* **Persistência em Arquivo:** Carregamento e salvamento da topologia do grafo no arquivo `dados_gps.txt`.

## Arquitetura do Projeto

| Arquivo | Descrição |
| :--- | :--- |
| `GPS.H` | Cabeçalho com a definição das estruturas (`Grafo`, `Cidade`, `Aresta`) e assinaturas das funções. |
| `gps.c` | Implementação do grafo, gerenciamento de memória, Dijkstra, BFS, DFS e leitura/escrita de arquivos. |
| `main.c` | Interface de linha de comando (CLI) com menu interativo para cadastro, listagem e execução dos algoritmos. |
| `main_gui.c` | Interface gráfica interativa construída com **GTK4** e **Cairo**, acoplada diretamente ao núcleo `gps.c`. |
| `dados_gps.txt` | Base de dados contendo cidades, coordenadas normalizadas e conexões rodoviárias. |

## Como Compilar e Executar (Windows / PowerShell)

Certifique-se de executar os comandos na mesma pasta onde se encontra o arquivo `dados_gps.txt`.

### Versão Interface Gráfica (GTK4)

```powershell
gcc main_gui.c gps.c -o gps_gui.exe $((pkg-config --cflags --libs gtk4).Trim() -split '\s+') -lm
.\gps_gui.exe
```

### Versão Terminal (CLI)

```powershell
gcc main.c gps.c -o gps_cli.exe -lm
.\gps_cli.exe
```

## Recursos da Interface Gráfica

* **Interação Direta no Mapa:** Clique com o **botão esquerdo** sobre uma cidade para defini-la como origem; clique com o **botão direito** para defini-la como destino (ou clique em uma área livre para criar um novo ponto conectado automaticamente à cidade mais próxima).
* **Navegação e Zoom:** Utilize a roda do mouse (*scroll*) ou os botões da barra inferior para aplicar zoom e arraste o mapa para explorar diferentes regiões.
* **Simulação de Trajeto:** Acompanhe o deslocamento ao longo da menor rota encontrada com controle de velocidade (`1x`, `3x`, `10x`), barra de progresso em tempo real e estimativa de tempo para **Carro**, **Bicicleta** ou **A pé**.