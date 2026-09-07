/* =============================================================================
 * colas.h -- Cola FIFO (lista enlazada simple)
 * ============================================================================= */

#ifndef COLAS_H
#define COLAS_H

#include <stdbool.h>

struct persona;
typedef struct persona persona_t;

typedef struct nodo_persona
{
   persona_t *persona;
   struct nodo_persona *siguiente;
} nodo_persona_t;

typedef struct cola
{
   nodo_persona_t *cabeza;
   nodo_persona_t *cola;
   int cantidad;
} cola_t;

void cola_init(cola_t *c);
void cola_destroy(cola_t *c);
void cola_encolar(cola_t *c, persona_t *p);
persona_t *cola_desencolar(cola_t *c);
int cola_cantidad(const cola_t *c);
bool cola_vacia(const cola_t *c);

#endif /* COLAS_H */
