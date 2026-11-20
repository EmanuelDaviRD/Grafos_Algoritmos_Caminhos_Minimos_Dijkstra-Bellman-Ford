@"
# Sistema de Navegação GPS em C (Grafos)

Projeto desenvolvido para a disciplina de **Estrutura de Dados**, implementando uma malha rodoviária utilizando **Grafos com Lista de Adjacência** e algoritmos clássicos de busca e caminho mínimo.

## Funcionalidades
- **Algoritmo de Dijkstra:** Cálculo do menor caminho em quilômetros entre duas cidades.
- **Buscas em Grafo (BFS e DFS):** Percurso em largura (usando Fila) e em profundidade (usando Pilha).
- **Persistência em Arquivo:** Leitura e gravação da rede rodoviária em \`dados_gps.txt\`.
- **Duas Interfaces:**
  - **Terminal (\`main.c\`):** Menu interativo para cadastro, listagem e algoritmos de busca.
  - **Interface Gráfica (\`main_gui.c\`):** Mapa interativo em GTK4 com zoom, simulação de trajeto em tempo real e cálculo de tempo por modo de transporte.

## Como Compilar

### Versão Terminal
\`\`\`bash
gcc main.c gps.c -o gps_cli.exe -lm
./gps_cli.exe
\`\`\`

### Versão Gráfica (GTK4)
\`\`\`powershell
gcc main_gui.c gps.c -o gps_gui.exe `$((pkg-config --cflags --libs gtk4).Trim() -split '\s+') -lm
.\gps_gui.exe
\`\`\`
"@ | Out-File -Encoding utf8 README.md
