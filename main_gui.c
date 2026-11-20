#include <gtk/gtk.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "GPS.H"

#define MIN_ZOOM 0.5
#define MAX_ZOOM 40.0
#define MAX_NOS  4096

typedef enum {
    MODO_CARRO = 0,
    MODO_BICICLETA = 1,
    MODO_A_PE = 2
} ModoTransporte;

typedef struct {
    Grafo* mapa;
    double coordX[MAX_NOS];
    double coordY[MAX_NOS];

    int origemId;
    int destinoId;
    int hoverId;
    int caminho[MAX_NOS];
    int tamCaminho;
    float distanciaTotal;

    ModoTransporte modo;
    double distanciaPercorrida;
    double multiplicadorSim;
    gboolean simulando;
    guint simTimerId;

    int fontSizeRota;
    GtkCssProvider* fontCssProvider;

    double zoom;
    double panX;
    double panY;
    double mouseX;
    double mouseY;
    double dragStartX;
    double dragStartY;
    double panStartX;
    double panStartY;
    gboolean dragging;

    GtkWidget* drawingArea;
    GtkWidget* dropOrigem;
    GtkWidget* dropDestino;
    GtkWidget* lblDistancia;
    GtkWidget* lblTempo;
    GtkWidget* lblParadas;
    GtkWidget* lblAndado;
    GtkWidget* lblRestante;
    GtkWidget* progressBar;
    GtkWidget* btnSimular;
    GtkWidget* lblRota;
    GtkWidget* lblFontZoom;
    GtkWidget* lblZoom;
    GtkWidget* lblStatus;
} AppData;

static const char* CAPITAIS[] = {
    "Rio Branco - AC", "Manaus - AM", "Boa Vista - RR", "Porto Velho - RO",
    "Belem - PA", "Macapa - AP", "Palmas - TO", "Sao Luis - MA",
    "Teresina - PI", "Fortaleza - CE", "Natal - RN", "Joao Pessoa - PB",
    "Recife - PE", "Maceio - AL", "Aracaju - SE", "Salvador - BA",
    "Cuiaba - MT", "Campo Grande - MS", "Goiania - GO", "Brasilia - DF",
    "Belo Horizonte - MG", "Vitoria - ES", "Rio de Janeiro - RJ",
    "Sao Paulo - SP", "Curitiba - PR", "Florianopolis - SC", "Porto Alegre - RS"
};

static void inicializar_coordenadas_mapa(AppData* app) {
    if (!app->mapa) return;
    int n = app->mapa->numCidades;
    if (n > MAX_NOS) n = MAX_NOS;

    gboolean temCoord[MAX_NOS] = {FALSE};
    int qtdComCoord = 0;

    for (int i = 0; i < n; i++) {
        char* sep = strchr(app->mapa->cidades[i].nome, ';');
        if (sep != NULL) {
            double cx = 0.0, cy = 0.0;
            if (sscanf(sep + 1, "%lf;%lf", &cx, &cy) == 2) {
                app->coordX[i] = cx;
                app->coordY[i] = cy;
                temCoord[i] = TRUE;
                qtdComCoord++;
            }
            *sep = '\0';
        }
    }

    for (int i = 0; i < n; i++) {
        if (!temCoord[i]) {
            double angulo = i * 2.39996;
            double raio = 0.08 + 0.24 * sqrt((double)(i + 1) / (double)(n > 0 ? n : 1));
            app->coordX[i] = 0.67 + raio * cos(angulo);
            app->coordY[i] = 0.50 + raio * sin(angulo) * 1.15;
        }
    }

    if (qtdComCoord == 0 && n > 1) {
        double dispX[MAX_NOS];
        double dispY[MAX_NOS];
        double area = 0.22;
        double k = sqrt(area / (double)n);

        for (int iter = 0; iter < 140; iter++) {
            double temp = 0.06 * (1.0 - (double)iter / 140.0);
            for (int i = 0; i < n; i++) {
                dispX[i] = 0.0;
                dispY[i] = 0.0;
            }

            for (int i = 0; i < n; i++) {
                for (int j = i + 1; j < n; j++) {
                    double dx = app->coordX[i] - app->coordX[j];
                    double dy = app->coordY[i] - app->coordY[j];
                    double dist = hypot(dx, dy);
                    if (dist < 0.0005) dist = 0.0005;
                    double rep = (k * k) / dist;
                    double fx = (dx / dist) * rep;
                    double fy = (dy / dist) * rep;
                    dispX[i] += fx; dispY[i] += fy;
                    dispX[j] -= fx; dispY[j] -= fy;
                }
            }

            for (int u = 0; u < n; u++) {
                Aresta* a = app->mapa->cidades[u].listaAdj;
                while (a != NULL) {
                    int v = a->destino;
                    if (u < v && v < n) {
                        double dx = app->coordX[u] - app->coordX[v];
                        double dy = app->coordY[u] - app->coordY[v];
                        double dist = hypot(dx, dy);
                        if (dist < 0.0005) dist = 0.0005;
                        double atr = (dist * dist) / k;
                        double fx = (dx / dist) * atr;
                        double fy = (dy / dist) * atr;
                        dispX[u] -= fx; dispY[u] -= fy;
                        dispX[v] += fx; dispY[v] += fy;
                    }
                    a = a->prox;
                }
            }

            for (int i = 0; i < n; i++) {
                double d = hypot(dispX[i], dispY[i]);
                if (d > 0.0) {
                    double passo = d < temp ? d : temp;
                    app->coordX[i] += (dispX[i] / d) * passo;
                    app->coordY[i] += (dispY[i] / d) * passo;
                }
                if (app->coordX[i] < 0.42) app->coordX[i] = 0.42;
                if (app->coordX[i] > 0.94) app->coordX[i] = 0.94;
                if (app->coordY[i] < 0.08) app->coordY[i] = 0.08;
                if (app->coordY[i] > 0.92) app->coordY[i] = 0.92;
            }
        }
    }
}

static float gui_obter_menor_caminho(Grafo* g, int origem, int destino, int* caminho, int* tamCaminho) {
    *tamCaminho = 0;
    if (!g || origem < 0 || origem >= g->numCidades || destino < 0 || destino >= g->numCidades) {
        return -1.0f;
    }

    int n = g->numCidades;
    float* dist = malloc(n * sizeof(float));
    int* anterior = malloc(n * sizeof(int));
    int* visitado = malloc(n * sizeof(int));

    if (!dist || !anterior || !visitado) {
        free(dist);
        free(anterior);
        free(visitado);
        return -1.0f;
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

        Aresta* adj = g->cidades[u].listaAdj;
        while (adj != NULL) {
            int v = adj->destino;
            if (!visitado[v] && dist[u] + adj->distancia < dist[v]) {
                dist[v] = dist[u] + adj->distancia;
                anterior[v] = u;
            }
            adj = adj->prox;
        }
    }

    float resultado = -1.0f;
    if (dist[destino] != FLT_MAX) {
        resultado = dist[destino];
        int* pilha = malloc(n * sizeof(int));
        if (pilha != NULL) {
            int count = 0;
            int atual = destino;
            while (atual != -1 && count < n) {
                pilha[count++] = atual;
                atual = anterior[atual];
            }
            for (int i = count - 1; i >= 0 && *tamCaminho < MAX_NOS; i--) {
                caminho[(*tamCaminho)++] = pilha[i];
            }
            free(pilha);
        }
    }

    free(dist);
    free(anterior);
    free(visitado);
    return resultado;
}

static double obter_velocidade_kmh(ModoTransporte modo) {
    switch (modo) {
        case MODO_BICICLETA: return 20.0;
        case MODO_A_PE:      return 5.0;
        case MODO_CARRO:
        default:             return 80.0;
    }
}

static gboolean is_capital(const char* nome) {
    int total = (int)(sizeof(CAPITAIS) / sizeof(CAPITAIS[0]));
    for (int i = 0; i < total; i++) {
        if (strcmp(nome, CAPITAIS[i]) == 0) return TRUE;
    }
    return FALSE;
}

static void city_to_screen(AppData* app, double normX, double normY, double w, double h, double* sx, double* sy) {
    double mapCenterX = w * 0.67;
    double mapCenterY = h * 0.50;
    double baseX = normX * w;
    double baseY = normY * h;
    *sx = mapCenterX + (baseX - mapCenterX) * app->zoom + app->panX;
    *sy = mapCenterY + (baseY - mapCenterY) * app->zoom + app->panY;
}

static void screen_to_norm(AppData* app, double sx, double sy, double w, double h, double* normX, double* normY) {
    double mapCenterX = w * 0.67;
    double mapCenterY = h * 0.50;
    double baseX = (sx - app->panX - mapCenterX) / app->zoom + mapCenterX;
    double baseY = (sy - app->panY - mapCenterY) / app->zoom + mapCenterY;
    *normX = baseX / w;
    *normY = baseY / h;
}

static void aplicar_zoom_no_ponto(AppData* app, double novoZoom, double anchorX, double anchorY) {
    if (novoZoom < MIN_ZOOM) novoZoom = MIN_ZOOM;
    if (novoZoom > MAX_ZOOM) novoZoom = MAX_ZOOM;

    int w = gtk_widget_get_width(app->drawingArea);
    int h = gtk_widget_get_height(app->drawingArea);
    if (w > 0 && h > 0 && app->zoom > 0.0) {
        double mapCenterX = w * 0.67;
        double mapCenterY = h * 0.50;
        double fator = novoZoom / app->zoom;
        app->panX = anchorX - mapCenterX - (anchorX - mapCenterX - app->panX) * fator;
        app->panY = anchorY - mapCenterY - (anchorY - mapCenterY - app->panY) * fator;
    }

    app->zoom = novoZoom;

    char buf[32];
    snprintf(buf, sizeof(buf), "Zoom: %.0f%%", app->zoom * 100.0);
    gtk_label_set_text(GTK_LABEL(app->lblZoom), buf);
    gtk_widget_queue_draw(app->drawingArea);
}

static float obter_distancia_segmento(Grafo* g, int u, int v) {
    Aresta* a = g->cidades[u].listaAdj;
    while (a != NULL) {
        if (a->destino == v) return a->distancia;
        a = a->prox;
    }
    return 0.0f;
}

static gboolean edge_in_path(AppData* app, int u, int v) {
    for (int i = 0; i < app->tamCaminho - 1; i++) {
        int a = app->caminho[i];
        int b = app->caminho[i + 1];
        if ((a == u && b == v) || (a == v && b == u)) return TRUE;
    }
    return FALSE;
}

static gboolean node_in_path(AppData* app, int id) {
    for (int i = 0; i < app->tamCaminho; i++) {
        if (app->caminho[i] == id) return TRUE;
    }
    return FALSE;
}

static void draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -G_PI / 2.0, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, G_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, G_PI / 2.0, G_PI);
    cairo_arc(cr, x + r, y + r, r, G_PI, 3.0 * G_PI / 2.0);
    cairo_close_path(cr);
}

static void atualizar_fonte_lateral(AppData* app) {
    if (!app->fontCssProvider) return;
    char css[256];
    snprintf(css, sizeof(css),
             ".route-text { color: #f3f4f6; font-size: %dpx; font-weight: 500; line-height: 1.55; }",
             app->fontSizeRota);
    gtk_css_provider_load_from_string(app->fontCssProvider, css);

    if (app->lblFontZoom) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%dpx", app->fontSizeRota);
        gtk_label_set_text(GTK_LABEL(app->lblFontZoom), buf);
    }
}

static void on_font_aumentar(GtkButton* btn, gpointer user_data) {
    (void)btn;
    AppData* app = (AppData*)user_data;
    if (app->fontSizeRota < 26) {
        app->fontSizeRota += 2;
        atualizar_fonte_lateral(app);
    }
}

static void on_font_diminuir(GtkButton* btn, gpointer user_data) {
    (void)btn;
    AppData* app = (AppData*)user_data;
    if (app->fontSizeRota > 14) {
        app->fontSizeRota -= 2;
        atualizar_fonte_lateral(app);
    }
}

static void configurar_dropdown_busca(GtkWidget* dropdown) {
    GtkExpression* expr = gtk_property_expression_new(GTK_TYPE_STRING_OBJECT, NULL, "string");
    gtk_drop_down_set_expression(GTK_DROP_DOWN(dropdown), expr);
    gtk_expression_unref(expr);

    gtk_drop_down_set_enable_search(GTK_DROP_DOWN(dropdown), TRUE);
#if GTK_CHECK_VERSION(4, 12, 0)
    gtk_drop_down_set_search_match_mode(GTK_DROP_DOWN(dropdown), GTK_STRING_FILTER_MATCH_MODE_SUBSTRING);
#endif
}

static void atualizar_painel_progresso(AppData* app) {
    char bufAndado[64];
    char bufRestante[64];

    if (app->distanciaTotal <= 0.0f || app->tamCaminho < 2) {
        gtk_label_set_text(GTK_LABEL(app->lblAndado), "0,0 km");
        gtk_label_set_text(GTK_LABEL(app->lblRestante), "-- km");
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->progressBar), 0.0);
        return;
    }

    if (app->distanciaPercorrida < 0.0) app->distanciaPercorrida = 0.0;
    if (app->distanciaPercorrida > (double)app->distanciaTotal) {
        app->distanciaPercorrida = (double)app->distanciaTotal;
    }

    double restante = (double)app->distanciaTotal - app->distanciaPercorrida;
    if (restante < 0.0) restante = 0.0;

    double frac = app->distanciaPercorrida / (double)app->distanciaTotal;
    if (frac > 1.0) frac = 1.0;

    snprintf(bufAndado, sizeof(bufAndado), "%.1f km (%.0f%%)", app->distanciaPercorrida, frac * 100.0);
    snprintf(bufRestante, sizeof(bufRestante), "%.1f km", restante);

    gtk_label_set_text(GTK_LABEL(app->lblAndado), bufAndado);
    gtk_label_set_text(GTK_LABEL(app->lblRestante), bufRestante);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->progressBar), frac);
}

static void parar_simulacao(AppData* app) {
    if (app->simTimerId != 0) {
        g_source_remove(app->simTimerId);
        app->simTimerId = 0;
    }
    app->simulando = FALSE;
    if (app->btnSimular) {
        gtk_button_set_label(GTK_BUTTON(app->btnSimular), "Iniciar Simulação");
    }
}

static void atualizar_tempo_estimado(AppData* app) {
    if (app->distanciaTotal < 0.0f || app->tamCaminho <= 0) {
        gtk_label_set_text(GTK_LABEL(app->lblTempo), "--");
        return;
    }

    double vel = obter_velocidade_kmh(app->modo);
    double horasTotais = (double)app->distanciaTotal / vel;
    int dias = (int)(horasTotais / 24.0);
    int horas = (int)fmod(horasTotais, 24.0);
    int minutos = (int)round((horasTotais - floor(horasTotais)) * 60.0);
    if (minutos == 60) {
        horas++;
        minutos = 0;
        if (horas == 24) { dias++; horas = 0; }
    }

    char bufTempo[64];
    if (dias > 0)
        snprintf(bufTempo, sizeof(bufTempo), "%dd %dh %02dm", dias, horas, minutos);
    else if (horas > 0)
        snprintf(bufTempo, sizeof(bufTempo), "%dh %02dmin", horas, minutos);
    else
        snprintf(bufTempo, sizeof(bufTempo), "%d min", minutos);

    gtk_label_set_text(GTK_LABEL(app->lblTempo), bufTempo);
}

static void recalcular_rota(AppData* app) {
    char bufDist[64];
    char bufParadas[64];
    char bufZoom[32];

    parar_simulacao(app);
    app->distanciaPercorrida = 0.0;

    snprintf(bufZoom, sizeof(bufZoom), "Zoom: %.0f%%", app->zoom * 100.0);
    gtk_label_set_text(GTK_LABEL(app->lblZoom), bufZoom);

    if (!app->mapa || app->origemId < 0 || app->destinoId < 0) return;

    app->distanciaTotal = gui_obter_menor_caminho(
        app->mapa, app->origemId, app->destinoId,
        app->caminho, &app->tamCaminho
    );

    if (app->distanciaTotal < 0.0f || app->tamCaminho <= 0) {
        gtk_label_set_text(GTK_LABEL(app->lblDistancia), "-- km");
        gtk_label_set_text(GTK_LABEL(app->lblTempo), "--");
        gtk_label_set_text(GTK_LABEL(app->lblParadas), "0");
        gtk_label_set_text(GTK_LABEL(app->lblRota), "Nenhuma ligação rodoviária disponível entre os pontos selecionados.");
        gtk_label_set_text(GTK_LABEL(app->lblStatus), "Sem rota");
        atualizar_painel_progresso(app);
        gtk_widget_queue_draw(app->drawingArea);
        return;
    }

    snprintf(bufDist, sizeof(bufDist), "%.1f km", app->distanciaTotal);
    atualizar_tempo_estimado(app);

    int paradas = app->tamCaminho > 2 ? app->tamCaminho - 2 : 0;
    snprintf(bufParadas, sizeof(bufParadas), "%d", paradas);

    gtk_label_set_text(GTK_LABEL(app->lblDistancia), bufDist);
    gtk_label_set_text(GTK_LABEL(app->lblParadas), bufParadas);

    char rotaTexto[16384] = "";
    for (int i = 0; i < app->tamCaminho; i++) {
        int id = app->caminho[i];
        char etapa[220];
        if (i == 0) {
            snprintf(etapa, sizeof(etapa), "1. Origem: %s\n", app->mapa->cidades[id].nome);
        } else {
            float segKm = obter_distancia_segmento(app->mapa, app->caminho[i - 1], id);
            if (i == app->tamCaminho - 1) {
                snprintf(etapa, sizeof(etapa), "%d. Destino: %s (+%.0f km)",
                         i + 1, app->mapa->cidades[id].nome, segKm);
            } else {
                snprintf(etapa, sizeof(etapa), "%d. %s (+%.0f km)\n",
                         i + 1, app->mapa->cidades[id].nome, segKm);
            }
        }
        strncat(rotaTexto, etapa, sizeof(rotaTexto) - strlen(rotaTexto) - 1);
    }
    gtk_label_set_text(GTK_LABEL(app->lblRota), rotaTexto);
    gtk_label_set_text(GTK_LABEL(app->lblStatus), "Rota calculada");
    atualizar_painel_progresso(app);
    gtk_widget_queue_draw(app->drawingArea);
}

static gboolean on_sim_tick(gpointer user_data) {
    AppData* app = (AppData*)user_data;
    if (!app->simulando || app->tamCaminho < 2 || app->distanciaTotal <= 0.0f) {
        app->simTimerId = 0;
        app->simulando = FALSE;
        return G_SOURCE_REMOVE;
    }

    double velKmh = obter_velocidade_kmh(app->modo);
    double dt = 0.03;
    double escalaBase = 300.0;
    double avancoKm = (velKmh / 3600.0) * escalaBase * app->multiplicadorSim * dt;

    app->distanciaPercorrida += avancoKm;
    if (app->distanciaPercorrida >= (double)app->distanciaTotal) {
        app->distanciaPercorrida = (double)app->distanciaTotal;
        atualizar_painel_progresso(app);
        gtk_label_set_text(GTK_LABEL(app->lblStatus), "Destino alcançado");
        gtk_widget_queue_draw(app->drawingArea);
        app->simTimerId = 0;
        app->simulando = FALSE;
        gtk_button_set_label(GTK_BUTTON(app->btnSimular), "Iniciar Simulação");
        return G_SOURCE_REMOVE;
    }

    atualizar_painel_progresso(app);
    gtk_widget_queue_draw(app->drawingArea);
    return G_SOURCE_CONTINUE;
}

static void on_simular_clicked(GtkButton* btn, gpointer user_data) {
    (void)btn;
    AppData* app = (AppData*)user_data;
    if (app->tamCaminho < 2 || app->distanciaTotal <= 0.0f) return;

    if (app->simulando) {
        parar_simulacao(app);
        gtk_label_set_text(GTK_LABEL(app->lblStatus), "Pausado");
    } else {
        if (app->distanciaPercorrida >= (double)app->distanciaTotal) {
            app->distanciaPercorrida = 0.0;
        }
        app->simulando = TRUE;
        gtk_button_set_label(GTK_BUTTON(app->btnSimular), "Pausar Simulação");
        gtk_label_set_text(GTK_LABEL(app->lblStatus), "Em percurso");
        app->simTimerId = g_timeout_add(30, on_sim_tick, app);
    }
}

static void on_reiniciar_sim_clicked(GtkButton* btn, gpointer user_data) {
    (void)btn;
    AppData* app = (AppData*)user_data;
    parar_simulacao(app);
    app->distanciaPercorrida = 0.0;
    atualizar_painel_progresso(app);
    gtk_label_set_text(GTK_LABEL(app->lblStatus), "Rota calculada");
    gtk_widget_queue_draw(app->drawingArea);
}

static void on_modo_toggled(GtkToggleButton* btn, gpointer user_data) {
    if (!gtk_toggle_button_get_active(btn)) return;
    AppData* app = (AppData*)user_data;
    int modo = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "modo"));
    app->modo = (ModoTransporte)modo;
    atualizar_tempo_estimado(app);
    gtk_widget_queue_draw(app->drawingArea);
}

static void on_vel_sim_toggled(GtkToggleButton* btn, gpointer user_data) {
    if (!gtk_toggle_button_get_active(btn)) return;
    AppData* app = (AppData*)user_data;
    int mult = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "mult"));
    app->multiplicadorSim = (double)mult;
}

static void draw_navigation_puck(cairo_t* cr, double x, double y, double angle, ModoTransporte modo) {
    cairo_save(cr);
    cairo_translate(cr, x, y);

    cairo_arc(cr, 0, 0, 14.0, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 0.06, 0.09, 0.16, 0.92);
    cairo_fill(cr);

    cairo_arc(cr, 0, 0, 14.0, 0, 2 * G_PI);
    if (modo == MODO_CARRO)
        cairo_set_source_rgb(cr, 0.23, 0.51, 0.96);
    else if (modo == MODO_BICICLETA)
        cairo_set_source_rgb(cr, 0.96, 0.62, 0.04);
    else
        cairo_set_source_rgb(cr, 0.06, 0.72, 0.50);
    cairo_set_line_width(cr, 2.4);
    cairo_stroke(cr);

    cairo_rotate(cr, angle);
    cairo_new_path(cr);
    cairo_move_to(cr, 8.5, 0.0);
    cairo_line_to(cr, -6.0, -5.5);
    cairo_line_to(cr, -3.0, 0.0);
    cairo_line_to(cr, -6.0, 5.5);
    cairo_close_path(cr);

    cairo_set_source_rgb(cr, 0.98, 0.99, 1.0);
    cairo_fill(cr);

    cairo_restore(cr);
}

static void on_draw(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data) {
    AppData* app = (AppData*)user_data;
    (void)area;

    cairo_set_source_rgb(cr, 0.043, 0.059, 0.090);
    cairo_paint(cr);

    cairo_set_source_rgba(cr, 0.58, 0.64, 0.72, 0.055);
    cairo_set_line_width(cr, 1.0);
    double step = 56.0 * app->zoom;
    while (step > 140.0) step *= 0.5;
    if (step < 18.0) step = 18.0;
    double offX = fmod(app->panX, step);
    double offY = fmod(app->panY, step);
    for (double x = offX; x < width; x += step) {
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, height);
    }
    for (double y = offY; y < height; y += step) {
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
    }
    cairo_stroke(cr);

    if (!app->mapa) return;

    int totalNos = app->mapa->numCidades < MAX_NOS ? app->mapa->numCidades : MAX_NOS;

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.5);

    for (int u = 0; u < totalNos; u++) {
        double x1, y1;
        city_to_screen(app, app->coordX[u], app->coordY[u], width, height, &x1, &y1);

        Aresta* adj = app->mapa->cidades[u].listaAdj;
        while (adj != NULL) {
            int v = adj->destino;
            if (u < v && v < totalNos && !edge_in_path(app, u, v)) {
                double x2, y2;
                city_to_screen(app, app->coordX[v], app->coordY[v], width, height, &x2, &y2);

                if ((x1 < -50 && x2 < -50) || (x1 > width + 50 && x2 > width + 50) ||
                    (y1 < -50 && y2 < -50) || (y1 > height + 50 && y2 > height + 50)) {
                    adj = adj->prox;
                    continue;
                }

                cairo_set_source_rgba(cr, 0.45, 0.53, 0.65, 0.22);
                cairo_set_line_width(cr, 1.2);
                cairo_move_to(cr, x1, y1);
                cairo_line_to(cr, x2, y2);
                cairo_stroke(cr);

                if (app->zoom >= 2.2) {
                    double mx = (x1 + x2) * 0.5;
                    double my = (y1 + y2) * 0.5;
                    char kmStr[32];
                    snprintf(kmStr, sizeof(kmStr), "%.0f km", adj->distancia);
                    cairo_set_source_rgba(cr, 0.62, 0.70, 0.82, 0.72);
                    cairo_move_to(cr, mx + 3, my - 3);
                    cairo_show_text(cr, kmStr);
                }
            }
            adj = adj->prox;
        }
    }

    if (app->tamCaminho > 1) {
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

        cairo_set_source_rgb(cr, 0.11, 0.25, 0.52);
        cairo_set_line_width(cr, 8.0);
        for (int i = 0; i < app->tamCaminho; i++) {
            int id = app->caminho[i];
            double sx, sy;
            city_to_screen(app, app->coordX[id], app->coordY[id], width, height, &sx, &sy);
            if (i == 0) cairo_move_to(cr, sx, sy);
            else cairo_line_to(cr, sx, sy);
        }
        cairo_stroke(cr);

        cairo_set_source_rgb(cr, 0.23, 0.51, 0.96);
        cairo_set_line_width(cr, 4.6);
        for (int i = 0; i < app->tamCaminho; i++) {
            int id = app->caminho[i];
            double sx, sy;
            city_to_screen(app, app->coordX[id], app->coordY[id], width, height, &sx, &sy);
            if (i == 0) cairo_move_to(cr, sx, sy);
            else cairo_line_to(cr, sx, sy);
        }
        cairo_stroke(cr);

        double arrowX = 0.0, arrowY = 0.0, arrowAngle = 0.0;
        int segAtual = 0;
        double accKm = 0.0;

        for (int i = 0; i < app->tamCaminho - 1; i++) {
            int u = app->caminho[i];
            int v = app->caminho[i + 1];
            double segKm = (double)obter_distancia_segmento(app->mapa, u, v);
            double x1, y1, x2, y2;
            city_to_screen(app, app->coordX[u], app->coordY[u], width, height, &x1, &y1);
            city_to_screen(app, app->coordX[v], app->coordY[v], width, height, &x2, &y2);

            if (app->distanciaPercorrida <= accKm + segKm || i == app->tamCaminho - 2) {
                double t = (segKm > 0.0) ? (app->distanciaPercorrida - accKm) / segKm : 0.0;
                if (t < 0.0) t = 0.0;
                if (t > 1.0) t = 1.0;
                arrowX = x1 + t * (x2 - x1);
                arrowY = y1 + t * (y2 - y1);
                arrowAngle = atan2(y2 - y1, x2 - x1);
                segAtual = i;
                break;
            }
            accKm += segKm;
        }

        if (app->distanciaPercorrida > 0.0) {
            cairo_set_source_rgb(cr, 0.06, 0.72, 0.50);
            cairo_set_line_width(cr, 5.2);
            for (int i = 0; i <= segAtual; i++) {
                int id = app->caminho[i];
                double sx, sy;
                city_to_screen(app, app->coordX[id], app->coordY[id], width, height, &sx, &sy);
                if (i == 0) cairo_move_to(cr, sx, sy);
                else cairo_line_to(cr, sx, sy);
            }
            cairo_line_to(cr, arrowX, arrowY);
            cairo_stroke(cr);
        }

        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 11.0);
        for (int i = 0; i < app->tamCaminho - 1; i++) {
            int u = app->caminho[i];
            int v = app->caminho[i + 1];
            float distSeg = obter_distancia_segmento(app->mapa, u, v);
            double x1, y1, x2, y2;
            city_to_screen(app, app->coordX[u], app->coordY[u], width, height, &x1, &y1);
            city_to_screen(app, app->coordX[v], app->coordY[v], width, height, &x2, &y2);

            double distPx = hypot(x2 - x1, y2 - y1);
            if (distPx < 42.0 && app->zoom < 2.2) continue;

            double mx = (x1 + x2) * 0.5;
            double my = (y1 + y2) * 0.5;
            char segStr[32];
            snprintf(segStr, sizeof(segStr), "%.0f km", distSeg);

            cairo_text_extents_t ext;
            cairo_text_extents(cr, segStr, &ext);
            draw_rounded_rect(cr, mx - ext.width * 0.5 - 6, my - ext.height - 5, ext.width + 12, ext.height + 8, 4.0);
            cairo_set_source_rgba(cr, 0.07, 0.10, 0.16, 0.94);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 0.23, 0.51, 0.96, 0.55);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);

            cairo_set_source_rgb(cr, 0.88, 0.93, 0.99);
            cairo_move_to(cr, mx - ext.width * 0.5, my - 2);
            cairo_show_text(cr, segStr);
        }

        draw_navigation_puck(cr, arrowX, arrowY, arrowAngle, app->modo);

        if (app->distanciaPercorrida > 0.0) {
            char badgeKm[48];
            snprintf(badgeKm, sizeof(badgeKm), "%.1f km percorridos", app->distanciaPercorrida);
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 11.0);
            cairo_text_extents_t ext;
            cairo_text_extents(cr, badgeKm, &ext);
            double bx = arrowX - ext.width * 0.5;
            double by = arrowY - 20.0;
            draw_rounded_rect(cr, bx - 7, by - ext.height - 4, ext.width + 14, ext.height + 8, 5.0);
            cairo_set_source_rgba(cr, 0.06, 0.14, 0.12, 0.95);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 0.06, 0.72, 0.50, 0.65);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);

            cairo_set_source_rgb(cr, 0.43, 0.91, 0.72);
            cairo_move_to(cr, bx, by);
            cairo_show_text(cr, badgeKm);
        }
    }

    for (int i = 0; i < totalNos; i++) {
        double sx, sy;
        city_to_screen(app, app->coordX[i], app->coordY[i], width, height, &sx, &sy);

        if (sx < -40 || sx > width + 40 || sy < -40 || sy > height + 40) continue;

        gboolean isOrig = (i == app->origemId);
        gboolean isDest = (i == app->destinoId);
        gboolean isHover = (i == app->hoverId);
        gboolean inPath = node_in_path(app, i);
        gboolean cap = is_capital(app->mapa->cidades[i].nome);

        double radius = cap ? 4.4 : 2.8;
        if (app->zoom >= 6.0) radius += 1.2;
        if (inPath) radius = 5.2;
        if (isOrig || isDest || isHover) radius = 7.4;

        cairo_arc(cr, sx, sy, radius, 0, 2 * G_PI);
        if (isOrig) cairo_set_source_rgb(cr, 0.06, 0.72, 0.50);
        else if (isDest) cairo_set_source_rgb(cr, 0.94, 0.27, 0.27);
        else if (inPath) cairo_set_source_rgb(cr, 0.96, 0.98, 1.0);
        else if (cap) cairo_set_source_rgb(cr, 0.88, 0.92, 0.98);
        else cairo_set_source_rgba(cr, 0.58, 0.66, 0.78, 0.75);
        cairo_fill(cr);

        if (cap || inPath || isOrig || isDest || isHover) {
            cairo_arc(cr, sx, sy, radius, 0, 2 * G_PI);
            if (isOrig || isDest) {
                cairo_set_source_rgb(cr, 0.98, 0.99, 1.0);
                cairo_set_line_width(cr, 2.0);
            } else if (inPath) {
                cairo_set_source_rgb(cr, 0.15, 0.39, 0.92);
                cairo_set_line_width(cr, 2.0);
            } else {
                cairo_set_source_rgb(cr, 0.05, 0.08, 0.12);
                cairo_set_line_width(cr, 1.3);
            }
            cairo_stroke(cr);
        }

        gboolean showLabel = isOrig || isDest || isHover || cap ||
                             (inPath && app->zoom >= 1.25) ||
                             (app->zoom >= 1.55);

        if (showLabel) {
            const char* nome = app->mapa->cidades[i].nome;
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                (isOrig || isDest || isHover || cap) ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
            double fsize = (isOrig || isDest || isHover) ? 13.0 : (cap ? 11.5 : 10.8);
            if (app->zoom >= 8.0) fsize += 1.5;
            cairo_set_font_size(cr, fsize);

            cairo_text_extents_t ext;
            cairo_text_extents(cr, nome, &ext);
            double lx = sx + radius + 6.0;
            double ly = sy + ext.height * 0.35;

            if (strcmp(nome, "Belo Horizonte - MG") == 0 ||
                strcmp(nome, "Goiania - GO") == 0 ||
                strcmp(nome, "Sao Paulo - SP") == 0) {
                lx = sx - radius - 6.0 - ext.width;
            }

            draw_rounded_rect(cr, lx - 5, ly - ext.height - 3, ext.width + 10, ext.height + 6, 4.0);
            cairo_set_source_rgba(cr, 0.05, 0.07, 0.11, 0.88);
            cairo_fill(cr);

            if (isOrig) cairo_set_source_rgb(cr, 0.43, 0.91, 0.72);
            else if (isDest) cairo_set_source_rgb(cr, 0.99, 0.65, 0.65);
            else if (inPath) cairo_set_source_rgb(cr, 0.93, 0.96, 1.0);
            else if (cap) cairo_set_source_rgb(cr, 0.95, 0.97, 1.0);
            else cairo_set_source_rgb(cr, 0.80, 0.86, 0.93);

            cairo_move_to(cr, lx, ly);
            cairo_show_text(cr, nome);
        }
    }
}

static void on_origem_changed(GObject* obj, GParamSpec* pspec, gpointer user_data) {
    (void)pspec;
    AppData* app = (AppData*)user_data;
    guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
    if (sel != GTK_INVALID_LIST_POSITION && (int)sel < app->mapa->numCidades) {
        app->origemId = (int)sel;
        recalcular_rota(app);
    }
}

static void on_destino_changed(GObject* obj, GParamSpec* pspec, gpointer user_data) {
    (void)pspec;
    AppData* app = (AppData*)user_data;
    guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(obj));
    if (sel != GTK_INVALID_LIST_POSITION && (int)sel < app->mapa->numCidades) {
        app->destinoId = (int)sel;
        recalcular_rota(app);
    }
}

static void on_inverter_clicked(GtkButton* btn, gpointer user_data) {
    (void)btn;
    AppData* app = (AppData*)user_data;
    int tmp = app->origemId;
    app->origemId = app->destinoId;
    app->destinoId = tmp;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(app->dropOrigem), (guint)app->origemId);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(app->dropDestino), (guint)app->destinoId);
    recalcular_rota(app);
}

static void on_zoom_in(GtkButton* btn, gpointer user_data) {
    (void)btn;
    AppData* app = (AppData*)user_data;
    int w = gtk_widget_get_width(app->drawingArea);
    int h = gtk_widget_get_height(app->drawingArea);
    aplicar_zoom_no_ponto(app, app->zoom * 1.4, w * 0.67, h * 0.50);
}

static void on_zoom_out(GtkButton* btn, gpointer user_data) {
    (void)btn;
    AppData* app = (AppData*)user_data;
    int w = gtk_widget_get_width(app->drawingArea);
    int h = gtk_widget_get_height(app->drawingArea);
    aplicar_zoom_no_ponto(app, app->zoom / 1.4, w * 0.67, h * 0.50);
}

static void on_reset_view(GtkButton* btn, gpointer user_data) {
    (void)btn;
    AppData* app = (AppData*)user_data;
    app->zoom = 1.0;
    app->panX = 0.0;
    app->panY = 0.0;
    gtk_label_set_text(GTK_LABEL(app->lblZoom), "Zoom: 100%");
    gtk_widget_queue_draw(app->drawingArea);
}

static gboolean on_scroll(GtkEventControllerScroll* ctrl, double dx, double dy, gpointer user_data) {
    (void)ctrl; (void)dx;
    AppData* app = (AppData*)user_data;
    double novoZoom = app->zoom;
    if (dy < 0) novoZoom *= 1.25;
    else if (dy > 0) novoZoom /= 1.25;
    aplicar_zoom_no_ponto(app, novoZoom, app->mouseX, app->mouseY);
    return TRUE;
}

static void on_drag_begin(GtkGestureDrag* gesture, double start_x, double start_y, gpointer user_data) {
    (void)gesture;
    AppData* app = (AppData*)user_data;
    app->dragStartX = start_x;
    app->dragStartY = start_y;
    app->panStartX = app->panX;
    app->panStartY = app->panY;
    app->dragging = TRUE;
}

static void on_drag_update(GtkGestureDrag* gesture, double offset_x, double offset_y, gpointer user_data) {
    (void)gesture;
    AppData* app = (AppData*)user_data;
    if (!app->dragging) return;
    app->panX = app->panStartX + offset_x;
    app->panY = app->panStartY + offset_y;
    gtk_widget_queue_draw(app->drawingArea);
}

static void on_drag_end(GtkGestureDrag* gesture, double offset_x, double offset_y, gpointer user_data) {
    (void)gesture; (void)offset_x; (void)offset_y;
    AppData* app = (AppData*)user_data;
    app->dragging = FALSE;
}

static void on_motion(GtkEventControllerMotion* ctrl, double x, double y, gpointer user_data) {
    (void)ctrl;
    AppData* app = (AppData*)user_data;
    app->mouseX = x;
    app->mouseY = y;
    if (!app->mapa) return;

    int w = gtk_widget_get_width(app->drawingArea);
    int h = gtk_widget_get_height(app->drawingArea);
    int found = -1;
    double bestDist = 14.0;
    int totalNos = app->mapa->numCidades < MAX_NOS ? app->mapa->numCidades : MAX_NOS;

    for (int i = 0; i < totalNos; i++) {
        double sx, sy;
        city_to_screen(app, app->coordX[i], app->coordY[i], w, h, &sx, &sy);
        double d = hypot(x - sx, y - sy);
        if (d < bestDist) {
            bestDist = d;
            found = i;
        }
    }

    if (found != app->hoverId) {
        app->hoverId = found;
        gtk_widget_queue_draw(app->drawingArea);
    }
}

static void on_click_pressed(GtkGestureClick* gesture, int n_press, double x, double y, gpointer user_data) {
    (void)n_press;
    AppData* app = (AppData*)user_data;
    if (!app->mapa) return;

    int w = gtk_widget_get_width(app->drawingArea);
    int h = gtk_widget_get_height(app->drawingArea);
    int clickedId = -1;
    double bestDist = 16.0;
    int totalNos = app->mapa->numCidades < MAX_NOS ? app->mapa->numCidades : MAX_NOS;

    for (int i = 0; i < totalNos; i++) {
        double sx, sy;
        city_to_screen(app, app->coordX[i], app->coordY[i], w, h, &sx, &sy);
        double d = hypot(x - sx, y - sy);
        if (d < bestDist) {
            bestDist = d;
            clickedId = i;
        }
    }

    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));

    if (clickedId >= 0) {
        if (button == 1) {
            app->origemId = clickedId;
            gtk_drop_down_set_selected(GTK_DROP_DOWN(app->dropOrigem), (guint)clickedId);
        } else if (button == 3) {
            app->destinoId = clickedId;
            gtk_drop_down_set_selected(GTK_DROP_DOWN(app->dropDestino), (guint)clickedId);
        }
        recalcular_rota(app);
        return;
    }

    if (button == 3 && app->mapa->numCidades < MAX_NOS) {
        double nx, ny;
        screen_to_norm(app, x, y, w, h, &nx, &ny);

        int nearest = -1;
        double minGeoDist = 1e9;
        for (int i = 0; i < totalNos; i++) {
            double d = hypot(nx - app->coordX[i], ny - app->coordY[i]);
            if (d < minGeoDist) {
                minGeoDist = d;
                nearest = i;
            }
        }

        if (nearest >= 0) {
            char novoNome[TAM_NOME];
            snprintf(novoNome, sizeof(novoNome), "Ponto %d", app->mapa->numCidades + 1);
            int novoId = cadastrarCidade(app->mapa, novoNome);

            if (novoId >= 0 && novoId < MAX_NOS) {
                app->coordX[novoId] = nx;
                app->coordY[novoId] = ny;

                float km = roundf((float)(minGeoDist * 3800.0));
                if (km < 5.0f) km = 5.0f;
                cadastrarEstrada(app->mapa, novoId, nearest, km, 0);

                GtkStringList* modelO = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(app->dropOrigem)));
                GtkStringList* modelD = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(app->dropDestino)));
                gtk_string_list_append(modelO, novoNome);
                gtk_string_list_append(modelD, novoNome);

                app->destinoId = novoId;
                gtk_drop_down_set_selected(GTK_DROP_DOWN(app->dropDestino), (guint)novoId);
                recalcular_rota(app);
            }
        }
    }
}

static void on_window_destroy(GtkWidget* widget, gpointer user_data) {
    (void)widget;
    AppData* app = (AppData*)user_data;
    app->btnSimular = NULL;
    parar_simulacao(app);
}

static void apply_css(AppData* app) {
    const char* css =
        "window { background-color: #0b0f17; }"
        ".sidebar { background-color: #111827; border-radius: 14px; border: 1px solid #1f2937; padding: 18px; }"
        ".title { color: #f9fafb; font-size: 22px; font-weight: 800; }"
        ".subtitle { color: #9ca3af; font-size: 13px; font-weight: 500; }"
        ".section-label { color: #e5e7eb; font-size: 14px; font-weight: 700; margin-top: 6px; }"

        ".dark-btn { background-image: none; background-color: #1e293b; color: #f1f5f9; border: 1px solid #334155; border-radius: 8px; padding: 8px 12px; min-height: 38px; }"
        ".dark-btn:hover { background-color: #283548; border-color: #475569; }"
        ".dark-btn label { color: #f1f5f9; font-size: 15px; font-weight: 600; }"

        ".seg-btn { background-image: none; background-color: #1e293b; color: #cbd5e1; border: 1px solid #334155; border-radius: 8px; padding: 8px 10px; min-height: 40px; }"
        ".seg-btn:hover { background-color: #283548; }"
        ".seg-btn label { color: #cbd5e1; font-size: 15px; font-weight: 700; }"
        ".seg-btn:checked { background-color: #2563eb; border-color: #3b82f6; }"
        ".seg-btn:checked label { color: #ffffff; }"

        ".primary-btn { background-image: none; background-color: #2563eb; color: #ffffff; border: 1px solid #3b82f6; border-radius: 8px; padding: 8px 14px; min-height: 40px; }"
        ".primary-btn:hover { background-color: #1d4ed8; }"
        ".primary-btn label { color: #ffffff; font-size: 15px; font-weight: 700; }"

        ".speed-btn { background-image: none; background-color: #1e293b; border: 1px solid #334155; border-radius: 8px; padding: 6px 10px; min-height: 40px; }"
        ".speed-btn label { color: #cbd5e1; font-size: 14px; font-weight: 700; }"
        ".speed-btn:checked { background-color: #334155; border-color: #60a5fa; }"
        ".speed-btn:checked label { color: #ffffff; }"

        ".city-dropdown button { background-image: none; background-color: #1e293b; color: #f8fafc; border: 1px solid #334155; border-radius: 8px; padding: 10px 14px; min-height: 44px; }"
        ".city-dropdown button:hover { background-color: #283548; border-color: #3b82f6; }"
        ".city-dropdown button label { color: #f8fafc; font-size: 17px; font-weight: 700; }"
        ".city-dropdown button arrow { color: #f8fafc; min-width: 16px; min-height: 16px; }"
        "popover.background, popover contents { background-color: #111827; border: 1px solid #334155; color: #f8fafc; }"
        "popover listview { background-color: #111827; }"
        "popover listview row { padding: 10px 14px; }"
        "popover listview row:selected, popover listview row:hover { background-color: #1e293b; }"
        "popover listview row label { color: #f1f5f9; font-size: 17px; font-weight: 600; }"
        "popover entry { background-color: #1e293b; color: #f8fafc; border: 1px solid #3b82f6; font-size: 16px; padding: 8px 12px; margin: 6px; }"

        ".stat-card { background-color: #182232; border-radius: 10px; border: 1px solid #263244; padding: 10px; }"
        ".stat-title { color: #9ca3af; font-size: 13px; font-weight: 600; }"
        ".stat-value { color: #f9fafb; font-size: 19px; font-weight: 800; }"
        ".stat-blue { color: #60a5fa; font-size: 19px; font-weight: 800; }"
        ".stat-green { color: #34d399; font-size: 19px; font-weight: 800; }"

        ".sim-box { background-color: #182232; border-radius: 10px; border: 1px solid #263244; padding: 12px; }"
        "progressbar trough { min-height: 10px; border-radius: 5px; background-color: #0b0f17; }"
        "progressbar progress { min-height: 10px; border-radius: 5px; background-color: #10b981; }"

        ".route-box { background-color: #0d131f; border-radius: 10px; border: 1px solid #263244; padding: 12px; }"
        ".badge-status { background-color: #064e3b; color: #6ee7b7; border: 1px solid #059669; border-radius: 6px; padding: 5px 10px; font-size: 13px; font-weight: 700; }"

        ".map-toolbar { background-color: #05080f; border-radius: 10px; border: 1px solid #334155; padding: 8px 10px; }"
        ".zoom-readout { color: #ffffff; font-size: 14px; font-weight: 800; }"
        "button.zoom-btn, .map-toolbar button { background-image: none; background-color: #0b0f17; color: #ffffff; border: 1px solid #475569; border-radius: 8px; padding: 6px 12px; min-width: 36px; min-height: 34px; }"
        "button.zoom-btn:hover, .map-toolbar button:hover { background-color: #1e293b; border-color: #3b82f6; }"
        "button.zoom-btn label, .map-toolbar button label { color: #ffffff; font-size: 15px; font-weight: 800; }"

        ".font-zoom-lbl { color: #9ca3af; font-size: 13px; font-weight: 700; }"
        ".hint-bar { background-color: rgba(17, 24, 39, 0.92); color: #e5e7eb; border-radius: 8px; border: 1px solid #1f2937; padding: 8px 14px; font-size: 13px; }";

    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, css);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);

    app->fontCssProvider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(app->fontCssProvider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1
    );
    atualizar_fonte_lateral(app);
}

static void on_activate(GtkApplication* gtkApp, gpointer user_data) {
    AppData* app = (AppData*)user_data;
    apply_css(app);

    GtkWidget* window = gtk_application_window_new(gtkApp);
    gtk_window_set_title(GTK_WINDOW(window), "Sistema de Navegação GPS — Malha Rodoviária Nacional");
    gtk_window_set_default_size(GTK_WINDOW(window), 1400, 840);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), app);

    GtkWidget* overlay = gtk_overlay_new();
    gtk_window_set_child(GTK_WINDOW(window), overlay);

    app->drawingArea = gtk_drawing_area_new();
    gtk_widget_set_hexpand(app->drawingArea, TRUE);
    gtk_widget_set_vexpand(app->drawingArea, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(app->drawingArea), on_draw, app, NULL);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), app->drawingArea);

    GtkEventController* scrollCtrl = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(scrollCtrl, "scroll", G_CALLBACK(on_scroll), app);
    gtk_widget_add_controller(app->drawingArea, scrollCtrl);

    GtkGesture* dragGest = gtk_gesture_drag_new();
    g_signal_connect(dragGest, "drag-begin", G_CALLBACK(on_drag_begin), app);
    g_signal_connect(dragGest, "drag-update", G_CALLBACK(on_drag_update), app);
    g_signal_connect(dragGest, "drag-end", G_CALLBACK(on_drag_end), app);
    gtk_widget_add_controller(app->drawingArea, GTK_EVENT_CONTROLLER(dragGest));

    GtkEventController* motionCtrl = gtk_event_controller_motion_new();
    g_signal_connect(motionCtrl, "motion", G_CALLBACK(on_motion), app);
    gtk_widget_add_controller(app->drawingArea, motionCtrl);

    GtkGesture* clickGest = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(clickGest), 0);
    g_signal_connect(clickGest, "pressed", G_CALLBACK(on_click_pressed), app);
    gtk_widget_add_controller(app->drawingArea, GTK_EVENT_CONTROLLER(clickGest));

    GtkWidget* sidebarScroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebarScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_add_css_class(sidebarScroll, "sidebar");
    gtk_widget_set_size_request(sidebarScroll, 460, -1);
    gtk_widget_set_halign(sidebarScroll, GTK_ALIGN_START);
    gtk_widget_set_valign(sidebarScroll, GTK_ALIGN_FILL);
    gtk_widget_set_margin_start(sidebarScroll, 14);
    gtk_widget_set_margin_top(sidebarScroll, 14);
    gtk_widget_set_margin_bottom(sidebarScroll, 14);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), sidebarScroll);

    GtkWidget* sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sidebarScroll), sidebar);

    GtkWidget* headerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* titleVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* lblTitle = gtk_label_new("Navegação GPS");
    gtk_widget_add_css_class(lblTitle, "title");
    gtk_widget_set_halign(lblTitle, GTK_ALIGN_START);
    GtkWidget* lblSub = gtk_label_new("Malha Rodoviária • Algoritmo de Dijkstra");
    gtk_widget_add_css_class(lblSub, "subtitle");
    gtk_widget_set_halign(lblSub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(titleVBox), lblTitle);
    gtk_box_append(GTK_BOX(titleVBox), lblSub);
    gtk_widget_set_hexpand(titleVBox, TRUE);

    app->lblStatus = gtk_label_new("Rota calculada");
    gtk_widget_add_css_class(app->lblStatus, "badge-status");
    gtk_widget_set_valign(app->lblStatus, GTK_ALIGN_CENTER);

    gtk_box_append(GTK_BOX(headerBox), titleVBox);
    gtk_box_append(GTK_BOX(headerBox), app->lblStatus);
    gtk_box_append(GTK_BOX(sidebar), headerBox);

    GtkWidget* lblModo = gtk_label_new("Modo de Transporte");
    gtk_widget_add_css_class(lblModo, "section-label");
    gtk_widget_set_halign(lblModo, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(sidebar), lblModo);

    GtkWidget* modoBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_set_homogeneous(GTK_BOX(modoBox), TRUE);
    GtkWidget* btnCarro = gtk_toggle_button_new_with_label("Carro");
    GtkWidget* btnBike  = gtk_toggle_button_new_with_label("Bicicleta");
    GtkWidget* btnPe    = gtk_toggle_button_new_with_label("A pé");

    gtk_widget_add_css_class(btnCarro, "seg-btn");
    gtk_widget_add_css_class(btnBike,  "seg-btn");
    gtk_widget_add_css_class(btnPe,    "seg-btn");

    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(btnBike), GTK_TOGGLE_BUTTON(btnCarro));
    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(btnPe), GTK_TOGGLE_BUTTON(btnCarro));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btnCarro), TRUE);

    g_object_set_data(G_OBJECT(btnCarro), "modo", GINT_TO_POINTER(MODO_CARRO));
    g_object_set_data(G_OBJECT(btnBike),  "modo", GINT_TO_POINTER(MODO_BICICLETA));
    g_object_set_data(G_OBJECT(btnPe),    "modo", GINT_TO_POINTER(MODO_A_PE));

    g_signal_connect(btnCarro, "toggled", G_CALLBACK(on_modo_toggled), app);
    g_signal_connect(btnBike,  "toggled", G_CALLBACK(on_modo_toggled), app);
    g_signal_connect(btnPe,    "toggled", G_CALLBACK(on_modo_toggled), app);

    gtk_box_append(GTK_BOX(modoBox), btnCarro);
    gtk_box_append(GTK_BOX(modoBox), btnBike);
    gtk_box_append(GTK_BOX(modoBox), btnPe);
    gtk_box_append(GTK_BOX(sidebar), modoBox);

    GtkStringList* listaOrigem = gtk_string_list_new(NULL);
    GtkStringList* listaDestino = gtk_string_list_new(NULL);
    for (int i = 0; i < app->mapa->numCidades; i++) {
        gtk_string_list_append(listaOrigem, app->mapa->cidades[i].nome);
        gtk_string_list_append(listaDestino, app->mapa->cidades[i].nome);
    }

    GtkWidget* lblO = gtk_label_new("Cidade de Origem");
    gtk_widget_add_css_class(lblO, "section-label");
    gtk_widget_set_halign(lblO, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(sidebar), lblO);

    app->dropOrigem = gtk_drop_down_new(G_LIST_MODEL(listaOrigem), NULL);
    gtk_widget_add_css_class(app->dropOrigem, "city-dropdown");
    configurar_dropdown_busca(app->dropOrigem);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(app->dropOrigem), (guint)app->origemId);
    g_signal_connect(app->dropOrigem, "notify::selected", G_CALLBACK(on_origem_changed), app);
    gtk_box_append(GTK_BOX(sidebar), app->dropOrigem);

    GtkWidget* btnInverter = gtk_button_new_with_label("Inverter Origem e Destino");
    gtk_widget_add_css_class(btnInverter, "dark-btn");
    g_signal_connect(btnInverter, "clicked", G_CALLBACK(on_inverter_clicked), app);
    gtk_box_append(GTK_BOX(sidebar), btnInverter);

    GtkWidget* lblD = gtk_label_new("Cidade de Destino");
    gtk_widget_add_css_class(lblD, "section-label");
    gtk_widget_set_halign(lblD, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(sidebar), lblD);

    app->dropDestino = gtk_drop_down_new(G_LIST_MODEL(listaDestino), NULL);
    gtk_widget_add_css_class(app->dropDestino, "city-dropdown");
    configurar_dropdown_busca(app->dropDestino);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(app->dropDestino), (guint)app->destinoId);
    g_signal_connect(app->dropDestino, "notify::selected", G_CALLBACK(on_destino_changed), app);
    gtk_box_append(GTK_BOX(sidebar), app->dropDestino);

    GtkWidget* statsGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(statsGrid), 8);
    gtk_grid_set_column_homogeneous(GTK_GRID(statsGrid), TRUE);

    const char* titulos[3] = {"Distância", "Tempo Est.", "Conexões"};
    GtkWidget** alvos[3] = {&app->lblDistancia, &app->lblTempo, &app->lblParadas};
    for (int i = 0; i < 3; i++) {
        GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_add_css_class(card, "stat-card");
        GtkWidget* t = gtk_label_new(titulos[i]);
        gtk_widget_add_css_class(t, "stat-title");
        *alvos[i] = gtk_label_new("--");
        gtk_widget_add_css_class(*alvos[i], "stat-value");
        gtk_box_append(GTK_BOX(card), t);
        gtk_box_append(GTK_BOX(card), *alvos[i]);
        gtk_grid_attach(GTK_GRID(statsGrid), card, i, 0, 1, 1);
    }
    gtk_box_append(GTK_BOX(sidebar), statsGrid);

    GtkWidget* simBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(simBox, "sim-box");

    GtkWidget* progGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(progGrid), 8);
    gtk_grid_set_column_homogeneous(GTK_GRID(progGrid), TRUE);

    GtkWidget* cardAndado = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* tAnd = gtk_label_new("Percorrido");
    gtk_widget_add_css_class(tAnd, "stat-title");
    app->lblAndado = gtk_label_new("0,0 km (0%)");
    gtk_widget_add_css_class(app->lblAndado, "stat-green");
    gtk_box_append(GTK_BOX(cardAndado), tAnd);
    gtk_box_append(GTK_BOX(cardAndado), app->lblAndado);

    GtkWidget* cardRest = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* tRest = gtk_label_new("Restante");
    gtk_widget_add_css_class(tRest, "stat-title");
    app->lblRestante = gtk_label_new("-- km");
    gtk_widget_add_css_class(app->lblRestante, "stat-blue");
    gtk_box_append(GTK_BOX(cardRest), tRest);
    gtk_box_append(GTK_BOX(cardRest), app->lblRestante);

    gtk_grid_attach(GTK_GRID(progGrid), cardAndado, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(progGrid), cardRest, 1, 0, 1, 1);
    gtk_box_append(GTK_BOX(simBox), progGrid);

    app->progressBar = gtk_progress_bar_new();
    gtk_box_append(GTK_BOX(simBox), app->progressBar);

    GtkWidget* simCtrlBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    app->btnSimular = gtk_button_new_with_label("Iniciar Simulação");
    gtk_widget_add_css_class(app->btnSimular, "primary-btn");
    gtk_widget_set_hexpand(app->btnSimular, TRUE);
    g_signal_connect(app->btnSimular, "clicked", G_CALLBACK(on_simular_clicked), app);

    GtkWidget* btnResetSim = gtk_button_new_with_label("Zerar");
    gtk_widget_add_css_class(btnResetSim, "dark-btn");
    g_signal_connect(btnResetSim, "clicked", G_CALLBACK(on_reiniciar_sim_clicked), app);

    GtkWidget* btn1x  = gtk_toggle_button_new_with_label("1x");
    GtkWidget* btn3x  = gtk_toggle_button_new_with_label("3x");
    GtkWidget* btn10x = gtk_toggle_button_new_with_label("10x");
    gtk_widget_add_css_class(btn1x,  "speed-btn");
    gtk_widget_add_css_class(btn3x,  "speed-btn");
    gtk_widget_add_css_class(btn10x, "speed-btn");

    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(btn3x), GTK_TOGGLE_BUTTON(btn1x));
    gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(btn10x), GTK_TOGGLE_BUTTON(btn1x));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn3x), TRUE);

    g_object_set_data(G_OBJECT(btn1x),  "mult", GINT_TO_POINTER(1));
    g_object_set_data(G_OBJECT(btn3x),  "mult", GINT_TO_POINTER(3));
    g_object_set_data(G_OBJECT(btn10x), "mult", GINT_TO_POINTER(10));
    g_signal_connect(btn1x,  "toggled", G_CALLBACK(on_vel_sim_toggled), app);
    g_signal_connect(btn3x,  "toggled", G_CALLBACK(on_vel_sim_toggled), app);
    g_signal_connect(btn10x, "toggled", G_CALLBACK(on_vel_sim_toggled), app);

    gtk_box_append(GTK_BOX(simCtrlBox), app->btnSimular);
    gtk_box_append(GTK_BOX(simCtrlBox), btnResetSim);
    gtk_box_append(GTK_BOX(simCtrlBox), btn1x);
    gtk_box_append(GTK_BOX(simCtrlBox), btn3x);
    gtk_box_append(GTK_BOX(simCtrlBox), btn10x);
    gtk_box_append(GTK_BOX(simBox), simCtrlBox);

    gtk_box_append(GTK_BOX(sidebar), simBox);

    GtkWidget* guiaHeader = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* lblGuia = gtk_label_new("Itinerário Detalhado");
    gtk_widget_add_css_class(lblGuia, "section-label");
    gtk_widget_set_halign(lblGuia, GTK_ALIGN_START);
    gtk_widget_set_hexpand(lblGuia, TRUE);

    app->lblFontZoom = gtk_label_new("17px");
    gtk_widget_add_css_class(app->lblFontZoom, "font-zoom-lbl");

    GtkWidget* btnFontMenos = gtk_button_new_with_label("A−");
    GtkWidget* btnFontMais  = gtk_button_new_with_label("A+");
    gtk_widget_add_css_class(btnFontMenos, "dark-btn");
    gtk_widget_add_css_class(btnFontMais,  "dark-btn");
    g_signal_connect(btnFontMenos, "clicked", G_CALLBACK(on_font_diminuir), app);
    g_signal_connect(btnFontMais,  "clicked", G_CALLBACK(on_font_aumentar), app);

    gtk_box_append(GTK_BOX(guiaHeader), lblGuia);
    gtk_box_append(GTK_BOX(guiaHeader), app->lblFontZoom);
    gtk_box_append(GTK_BOX(guiaHeader), btnFontMenos);
    gtk_box_append(GTK_BOX(guiaHeader), btnFontMais);
    gtk_box_append(GTK_BOX(sidebar), guiaHeader);

    GtkWidget* scrollRota = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollRota), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_widget_set_size_request(scrollRota, -1, 210);
    gtk_widget_set_vexpand(scrollRota, TRUE);
    gtk_widget_add_css_class(scrollRota, "route-box");

    app->lblRota = gtk_label_new("");
    gtk_widget_add_css_class(app->lblRota, "route-text");
    gtk_widget_set_halign(app->lblRota, GTK_ALIGN_START);
    gtk_widget_set_valign(app->lblRota, GTK_ALIGN_START);
    gtk_label_set_wrap(GTK_LABEL(app->lblRota), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrollRota), app->lblRota);
    gtk_box_append(GTK_BOX(sidebar), scrollRota);

    GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(toolbar, "map-toolbar");
    gtk_widget_set_halign(toolbar, GTK_ALIGN_END);
    gtk_widget_set_valign(toolbar, GTK_ALIGN_END);
    gtk_widget_set_margin_end(toolbar, 18);
    gtk_widget_set_margin_bottom(toolbar, 18);

    app->lblZoom = gtk_label_new("Zoom: 100%");
    gtk_widget_add_css_class(app->lblZoom, "zoom-readout");
    gtk_widget_set_margin_start(app->lblZoom, 6);
    gtk_widget_set_margin_end(app->lblZoom, 6);
    gtk_box_append(GTK_BOX(toolbar), app->lblZoom);

    GtkWidget* btnZOut = gtk_button_new_with_label("−");
    GtkWidget* btnZIn = gtk_button_new_with_label("+");
    GtkWidget* btnReset = gtk_button_new_with_label("Centralizar");
    gtk_widget_add_css_class(btnZOut, "zoom-btn");
    gtk_widget_add_css_class(btnZIn, "zoom-btn");
    gtk_widget_add_css_class(btnReset, "zoom-btn");

    g_signal_connect(btnZOut, "clicked", G_CALLBACK(on_zoom_out), app);
    g_signal_connect(btnZIn, "clicked", G_CALLBACK(on_zoom_in), app);
    g_signal_connect(btnReset, "clicked", G_CALLBACK(on_reset_view), app);
    gtk_box_append(GTK_BOX(toolbar), btnZOut);
    gtk_box_append(GTK_BOX(toolbar), btnZIn);
    gtk_box_append(GTK_BOX(toolbar), btnReset);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), toolbar);

    GtkWidget* hint = gtk_label_new("Clique esq.: Origem  |  Clique dir.: Destino ou Novo Ponto  |  Arrastar/Scroll: Mover e Zoom");
    gtk_widget_add_css_class(hint, "hint-bar");
    gtk_widget_set_halign(hint, GTK_ALIGN_END);
    gtk_widget_set_valign(hint, GTK_ALIGN_START);
    gtk_widget_set_margin_end(hint, 18);
    gtk_widget_set_margin_top(hint, 18);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), hint);

    recalcular_rota(app);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char** argv) {
    AppData app = {0};
    app.zoom = 1.0;
    app.hoverId = -1;
    app.modo = MODO_CARRO;
    app.multiplicadorSim = 3.0;
    app.fontSizeRota = 17;

    char arquivo[] = ARQUIVO_DADOS;
    app.mapa = criarGrafo(400);
    if (!app.mapa || !carregarDados(app.mapa, arquivo)) {
        fprintf(stderr, "Erro ao abrir %s\n", ARQUIVO_DADOS);
        if (app.mapa) liberarGrafo(app.mapa);
        return 1;
    }

    inicializar_coordenadas_mapa(&app);

    char nomeOrigem[] = "Aurora - CE";
    char nomeDestino[] = "Fortaleza - CE";
    int idAurora = buscarCidadePorNome(app.mapa, nomeOrigem);
    int idFortaleza = buscarCidadePorNome(app.mapa, nomeDestino);
    app.origemId = (idAurora >= 0) ? idAurora : 0;
    app.destinoId = (idFortaleza >= 0) ? idFortaleza : (app.mapa->numCidades > 1 ? 1 : 0);

    GtkApplication* gtkApp = gtk_application_new("br.gps.mapa.nacional", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtkApp, "activate", G_CALLBACK(on_activate), &app);
    int status = g_application_run(G_APPLICATION(gtkApp), argc, argv);

    if (app.fontCssProvider) g_object_unref(app.fontCssProvider);
    g_object_unref(gtkApp);
    liberarGrafo(app.mapa);
    return status;
}