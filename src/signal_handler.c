/* =============================================================================
 * signal_handler.c -- Gestor sincrono de senales
 * =============================================================================
 *
 * Hilo que espera SIGUSR1 / SIGINT / SIGTERM con sigwait() y las traduce a operaciones del dominio.
 *
 * Contrato publico: ver include/signal_handler.h
 * Diseno detallado: docs/diseno_logico.md, seccion 3.4
 * -----------------------------------------------------------------------------
 * TODO: implementar -- ver el estado de fases en docs/BITACORA.md
 * ========================================================================== */
