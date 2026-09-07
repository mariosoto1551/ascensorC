/* =============================================================================
 * monitor.h -- Visualizacion en tiempo real (ncurses)
 * =============================================================================
 *
 * RESPONSABILIDAD
 *   Hilo que dibuja el estado del sistema con ncurses.
 *   SOLO LEE: pide un Snapshot a elevator y lo pinta. Nunca modifica datos
 *   de dominio ni toca el mutex por su cuenta.
 *
 * Ver: docs/diseno_logico.md, secciones 3.6 y 4.4.
 * ========================================================================== */

#ifndef MONITOR_H
#define MONITOR_H

/* Cada cuanto se repinta la pantalla (seccion 3.6). */
#define MONITOR_REFRESCO_MS 200

/* Cuanto se deja ver la pantalla final antes de devolver la terminal. */
#define MONITOR_PAUSA_FINAL_MS 900

/* Tamano minimo de terminal para que el tablero entre. Por debajo de esto el
   monitor no dibuja el edificio, solo avisa que hay que agrandar la ventana. */
#define MONITOR_MIN_COLUMNAS 66

void *monitor_run(void *arg);

#endif /* MONITOR_H */