/* =============================================================================
 * monitor.c -- Visualizacion ncurses
 * =============================================================================
 *
 * Hilo de solo lectura que pinta snapshots del sistema en pantalla.
 *
 * Contrato publico: ver include/monitor.h
 * Diseno detallado: docs/diseno_logico.md, seccion 3.6
 * -----------------------------------------------------------------------------
 * TODO: implementar -- ver el estado de fases en docs/BITACORA.md
 * ========================================================================== */
#include "monitor.h"
#include <stdio.h>
#include <unistd.h>

void *monitor_run(void *arg)
{
    // Stub: solo imprime que está corriendo
    printf("Monitor (ncurses) iniciado (stub)\n");
    while (1)
    {
        // TODO: ncurses, tomar snapshot, dibujar
        sleep(1);
    }
    return NULL;
}