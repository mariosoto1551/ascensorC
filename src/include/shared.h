/* =============================================================================
 * shared.h -- Sincronizacion y estado global compartido
 * =============================================================================
 *
 * RESPONSABILIDAD
 *   Unico poseedor del mutex del sistema. Encapsula el mutex global, la
 *   variable de condicion, las banderas del sistema (terminando, hay_llamadas)
 *   y el contador de personas activas.
 *
 *   Ningun otro modulo toca el mutex directamente: todos pasan por esta API.
 *
 * TIPO PRINCIPAL
 *   shared_t { mutex, cond, terminando, personas_activas, hay_llamadas }
 *
 * API PUBLICA PLANIFICADA
 *   void shared_init(shared_t *s);
 *   void shared_destroy(shared_t *s);
 *   void shared_lock(shared_t *s);
 *   void shared_unlock(shared_t *s);
 *   void shared_wait(shared_t *s);              // wrapper de pthread_cond_wait
 *   void shared_broadcast(shared_t *s);
 *   void shared_signal(shared_t *s);
 *   void shared_set_terminando(shared_t *s);
 *   bool shared_is_terminando(shared_t *s);
 *   void shared_inc_personas(shared_t *s);
 *   void shared_dec_personas(shared_t *s);
 *   void shared_set_llamadas(shared_t *s, bool valor);
 *
 * REGLA DE ORO
 *   Nunca mantener el mutex durante operaciones bloqueantes
 *   (sleep, join, I/O, ncurses).
 *
 * Ver: docs/diseno_logico.md, secciones 3.2 y 6.
 * -----------------------------------------------------------------------------
 * TODO: implementar -- ver el estado de fases en docs/BITACORA.md
 * ========================================================================== */
