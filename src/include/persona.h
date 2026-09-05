/* =============================================================================
 * persona.h -- Ciclo de vida del trabajador
 * =============================================================================
 *
 * RESPONSABILIDAD
 *   Define la rutina de cada hilo persona: trabajar, elegir destino aleatorio,
 *   gestionar la jornada laboral y encolarse / desencolarse.
 *
 *   Una persona solo modifica sus propios atributos (destino, jornada).
 *   No toca el mutex directamente.
 *
 * TIPO PRINCIPAL
 *   persona_t { id, piso_actual, destino, direccion, jornada_restante,
 *               tiempo_trabajo, hilo }
 *
 * API PUBLICA PLANIFICADA
 *   void *persona_run(void *arg);   // rutina del hilo
 *   (constructor / destructor de persona_t a definir al implementar)
 *
 * FLUJO
 *   1. Se encola en el piso actual
 *   2. Espera a ser recogida por el ascensor
 *   3. Viaja al destino
 *   4. Trabaja un tiempo aleatorio
 *   5. Reduce su jornada laboral
 *   6. Elige nuevo destino (o PB si la jornada se agoto)
 *   7. Repite hasta salir del edificio
 *
 * Ver: docs/diseno_logico.md, secciones 3.5, 4.3 y 5.1.
 * -----------------------------------------------------------------------------
 * TODO: implementar -- ver el estado de fases en docs/BITACORA.md
 * ========================================================================== */
