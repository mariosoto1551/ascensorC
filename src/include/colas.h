/* =============================================================================
 * colas.h -- Cola FIFO (lista enlazada simple)
 * =============================================================================
 *
 * RESPONSABILIDAD
 *   Estructura de datos auxiliar, sin logica de concurrencia propia.
 *   La usa internamente elevator para gestionar las esperas por piso y
 *   direccion. Quien la llama ya debe tener tomado el mutex de shared.
 *
 * TIPOS PRINCIPALES
 *   nodo_persona_t { persona, siguiente }
 *   cola_t         { cabeza, cola, cantidad }
 *
 * API PUBLICA PLANIFICADA
 *   - inicializar / destruir una cola
 *   - encolar al final
 *   - desencolar de la cabeza
 *   - consultar cantidad y si esta vacia
 *   (nombres exactos a definir al implementar)
 *
 * NOTA
 *   La cola no es duena de las personas: guarda punteros, no copia ni libera
 *   los persona_t. Esa liberacion la decide el ciclo de vida del hilo persona.
 *
 * Ver: docs/diseno_logico.md, seccion 4.2.
 * -----------------------------------------------------------------------------
 * TODO: implementar -- ver el estado de fases en docs/BITACORA.md
 * ========================================================================== */
