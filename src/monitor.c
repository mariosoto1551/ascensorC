/* =============================================================================
 * monitor.c -- Visualizacion ncurses
 * ========================================================================== */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <ncurses.h>

#include "monitor.h"
#include "elevator.h"

/* =============================================================================
 * Disposicion de la pantalla
 * ========================================================================== */

#define FILA_TITULO 0
#define FILA_ENCABEZADO 2
#define FILA_REGLA 3
#define FILA_PISOS 4

// Dimensión de columnas
#define COL_PISO 2
#define COL_SUBEN 8
#define COL_BAJAN 20
#define COL_CABINA 34

/* Cuantas filas se usan por debajo del edificio (resumen + ayuda). */
#define FILAS_PIE 6

/* Marcas maximas que se dibujan en una cola antes de pasar a "+N". */
#define MARCAS_MAX 8

/* Pares de color. Se usan solo si la terminal los soporta. */
enum
{
    CP_TITULO = 1,
    CP_ENCABEZADO,
    CP_SUBEN,
    CP_BAJAN,
    CP_CABINA,
    CP_ALERTA,
    CP_TENUE
};

static bool usar_color = false;

/* =============================================================================
 * Utilidades
 * ========================================================================== */

static void dormir_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;

    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
    {
        /* SIGUSR1 es frecuente: reintentamos con lo que quedaba */
    }
}

static void color_on(int par)
{
    if (usar_color)
    {
        attron(COLOR_PAIR(par));
    }
}

static void color_off(int par)
{
    if (usar_color)
    {
        attroff(COLOR_PAIR(par));
    }
}

/* "^^^ 3", "vvvvvvvv+ 12" o "." si no hay nadie. */
static void formatear_cola(char *dst, size_t n, int cantidad, char simbolo)
{
    if (cantidad <= 0)
    {
        snprintf(dst, n, ".");
        return;
    }

    char marcas[MARCAS_MAX + 1];
    int cuantas = (cantidad > MARCAS_MAX) ? MARCAS_MAX : cantidad;
    memset(marcas, simbolo, (size_t)cuantas);
    marcas[cuantas] = '\0';

    char temp[32];
    snprintf(temp, sizeof(temp), "%s%s %d", marcas,
             (cantidad > MARCAS_MAX) ? "+" : "", cantidad);

    // Truncar a n-1 caracteres (dejando espacio para '\0')
    snprintf(dst, n, "%.*s", (int)n - 1, temp);
}

/* "[###.....] 3/8" */
static void formatear_cabina(char *dst, size_t n, const Snapshot *s)
{
    char barra[CAPACIDAD + 1];
    int cap = (s->capacidad > 0 && s->capacidad <= CAPACIDAD) ? s->capacidad
                                                              : CAPACIDAD;

    for (int i = 0; i < cap; i++)
    {
        barra[i] = (i < s->personas_dentro) ? '#' : '.';
    }
    barra[cap] = '\0';

    snprintf(dst, n, "[%s] %d/%d", barra, s->personas_dentro, cap);
}

/* "2 5 9" con los destinos de quienes van a bordo, o "-" si va vacio. */
static void formatear_destinos(char *dst, size_t n, const Snapshot *s)
{
    size_t usado = 0;

    if (n == 0)
    {
        return;
    }
    dst[0] = '\0';

    for (int i = 0; i < CAPACIDAD; i++)
    {
        if (s->destinos[i] < 0)
        {
            continue;
        }
        int escrito = snprintf(dst + usado, n - usado, "%s%d",
                               (usado > 0) ? " " : "", s->destinos[i]);
        if (escrito < 0 || (size_t)escrito >= n - usado)
        {
            break;
        }
        usado += (size_t)escrito;
    }

    if (usado == 0)
    {
        snprintf(dst, n, "-");
    }
}

/* =============================================================================
 * Dibujo
 * ========================================================================== */

static void dibujar_titulo(int ancho, const Snapshot *s)
{
    color_on(CP_TITULO);
    attron(A_BOLD);
    mvprintw(FILA_TITULO, COL_PISO, "SIMULADOR DE ASCENSOR");
    attroff(A_BOLD);
    color_off(CP_TITULO);

    char derecha[64];
    snprintf(derecha, sizeof derecha, "PID %ld", (long)getpid());

    int col = ancho - (int)strlen(derecha) - COL_PISO;
    if (col > COL_PISO + 24)
    {
        color_on(CP_TENUE);
        mvprintw(FILA_TITULO, col, "%s", derecha);
        color_off(CP_TENUE);
    }

    if (s->terminando)
    {
        color_on(CP_ALERTA);
        attron(A_BOLD | A_BLINK);
        mvprintw(FILA_TITULO, COL_PISO + 24, "-- EVACUANDO EDIFICIO --");
        attroff(A_BOLD | A_BLINK);
        color_off(CP_ALERTA);
    }
}

static void dibujar_encabezado(int ancho)
{
    color_on(CP_ENCABEZADO);
    mvprintw(FILA_ENCABEZADO, COL_PISO, "PISO");
    mvprintw(FILA_ENCABEZADO, COL_SUBEN, "SUBEN");
    mvprintw(FILA_ENCABEZADO, COL_BAJAN, "BAJAN");
    mvprintw(FILA_ENCABEZADO, COL_CABINA, "CABINA");

    int largo = (ancho - 2 * COL_PISO > 0) ? ancho - 2 * COL_PISO : 0;
    mvhline(FILA_REGLA, COL_PISO, '-', largo);
    color_off(CP_ENCABEZADO);
}

/*
 * Un piso por fila, el mas alto arriba: asi el tablero se lee como un edificio
 * de verdad y no al reves.
 */
static void dibujar_edificio(const Snapshot *s)
{
    char texto_suben[32];
    char texto_bajan[32];
    char texto_cabina[32];
    char texto_destinos[64];

    for (int piso = s->num_pisos - 1; piso >= 0; piso--)
    {
        int fila = FILA_PISOS + (s->num_pisos - 1 - piso);

        /* --- Numero de piso --- */
        if (piso == PISO_PB)
        {
            mvprintw(fila, COL_PISO, "PB");
        }
        else
        {
            mvprintw(fila, COL_PISO, "P%d", piso);
        }

        /* --- Cola de subida --- */
        formatear_cola(texto_suben, sizeof texto_suben, s->colas_subida[piso], '^');
        color_on(CP_SUBEN);
        mvprintw(fila, COL_SUBEN, "%-10s", texto_suben);
        color_off(CP_SUBEN);

        /* --- Cola de bajada --- */
        if (piso == PISO_PB)
        {
            color_on(CP_TENUE);
            mvprintw(fila, COL_BAJAN, "%-10s", "-");
            color_off(CP_TENUE);
        }
        else
        {
            formatear_cola(texto_bajan, sizeof texto_bajan, s->colas_bajada[piso], 'v');
            color_on(CP_BAJAN);
            mvprintw(fila, COL_BAJAN, "%-10s", texto_bajan);
            color_off(CP_BAJAN);
        }

        /* --- La cabina --- */
        if (piso == s->piso_ascensor)
        {
            formatear_cabina(texto_cabina, sizeof texto_cabina, s);
            formatear_destinos(texto_destinos, sizeof texto_destinos, s);

            color_on(CP_CABINA);
            attron(A_BOLD);
            mvprintw(fila, COL_CABINA, "%s  %s  ->  %s",
                     texto_cabina,
                     elevator_estado_texto(s->estado_ascensor),
                     texto_destinos);
            attroff(A_BOLD);
            color_off(CP_CABINA);
        }
    }
}

static void dibujar_pie(const Snapshot *s)
{
    int fila = FILA_PISOS + s->num_pisos + 1;

    mvprintw(fila, COL_PISO,
             "Ascensor: %-8s  Piso: %-2d  A bordo: %d/%d",
             elevator_estado_texto(s->estado_ascensor),
             s->piso_ascensor, s->personas_dentro, s->capacidad);
    clrtoeol();

    mvprintw(fila + 1, COL_PISO,
             "Esperando: %-4d  En el edificio: %-4d",
             s->total_esperando, s->personas_activas);

    clrtoeol();

    color_on(CP_TENUE);
    if (s->terminando)
    {
        mvprintw(fila + 3, COL_PISO,
                 "Apagado en curso: esperando a que el edificio quede vacio...");
    }
    else
    {
        mvprintw(fila + 3, COL_PISO,
                 "[kill -USR1 %ld] entra una persona    [Ctrl+C] salida ordenada",
                 (long)getpid());
    }
    clrtoeol();
    color_off(CP_TENUE);
}

static void dibujar_pantalla_chica(int alto, int ancho, int necesario)
{
    mvprintw(0, 0, "Terminal demasiado chica para el tablero.");
    mvprintw(1, 0, "Se necesitan %d filas x %d columnas.", necesario,
             MONITOR_MIN_COLUMNAS);
    mvprintw(2, 0, "Actual: %d x %d. Agranda la ventana.", alto, ancho);
}

static void dibujar(const Snapshot *s)
{
    int alto = 0;
    int ancho = 0;

    getmaxyx(stdscr, alto, ancho);
    erase();

    int necesario = FILA_PISOS + s->num_pisos + FILAS_PIE;

    if (alto < necesario || ancho < MONITOR_MIN_COLUMNAS)
    {
        dibujar_pantalla_chica(alto, ancho, necesario);
    }
    else
    {
        dibujar_titulo(ancho, s);
        dibujar_encabezado(ancho);
        dibujar_edificio(s);
        dibujar_pie(s);
    }

    refresh();
}

/* =============================================================================
 * Arranque y cierre de ncurses
 * ========================================================================== */

static void iniciar_ncurses(void)
{
    initscr();

    /* cbreak() y no raw(): raw() desactiva ISIG y con eso Ctrl+C dejaria de
       generar SIGINT, que es justamente como se apaga el simulador. */
    cbreak();
    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    usar_color = (has_colors() == TRUE);
    if (usar_color)
    {
        start_color();
        use_default_colors();
        init_pair(CP_TITULO, COLOR_CYAN, -1);
        init_pair(CP_ENCABEZADO, COLOR_GREEN, -1);
        init_pair(CP_SUBEN, COLOR_CYAN, -1);
        init_pair(CP_BAJAN, COLOR_MAGENTA, -1);
        init_pair(CP_CABINA, COLOR_YELLOW, -1);
        init_pair(CP_ALERTA, COLOR_RED, -1);
        init_pair(CP_TENUE, COLOR_BLUE, -1);
    }
}

/* =============================================================================
 * Bucle del hilo
 * ========================================================================== */

/*
 * Termina cuando el edificio quedo vacio DESPUES de un pedido de apagado.
 * Se verifica que no haya personas activas, ni personas_dentro, ni colas.
 */
static bool termino_todo(const Snapshot *s)
{
    return s->terminando &&
           s->personas_activas == 0 &&
           s->personas_dentro == 0 &&
           s->total_esperando == 0; /* <-- CORRECCIÓN: también esperamos colas vacías */
}

void *monitor_run(void *arg)
{
    elevator_t *e = (elevator_t *)arg;

    if (e == NULL)
    {
        return NULL;
    }

    /* Sin terminal no hay nada que dibujar: se acompana la simulacion en
       silencio en vez de reventar ncurses contra un archivo o una tuberia. */
    if (!isatty(STDOUT_FILENO))
    {
        for (;;)
        {
            Snapshot s = elevator_get_snapshot(e);
            if (termino_todo(&s))
            {
                break;
            }
            dormir_ms(MONITOR_REFRESCO_MS);
        }
        return NULL;
    }

    iniciar_ncurses();

    for (;;)
    {
        Snapshot s = elevator_get_snapshot(e);
        dibujar(&s);

        if (termino_todo(&s))
        {
            break;
        }
        dormir_ms(MONITOR_REFRESCO_MS);
    }

    /* Un momento con la pantalla final antes de devolver la terminal. */
    dormir_ms(MONITOR_PAUSA_FINAL_MS);

    curs_set(1);
    endwin();

    return NULL;
}