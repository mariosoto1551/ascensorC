/* =============================================================================
 * colas.c -- Cola FIFO
 * ============================================================================= */

#include <stdlib.h>

#include "colas.h"

void cola_init(cola_t *c)
{
    if (c == NULL)
        return;

    c->cabeza = NULL;
    c->cola = NULL;
    c->cantidad = 0;
}

void cola_destroy(cola_t *c)
{
    if (c == NULL)
        return;

    nodo_persona_t *n = c->cabeza;
    while (n != NULL)
    {
        nodo_persona_t *sig = n->siguiente;
        free(n);
        n = sig;
    }

    c->cabeza = NULL;
    c->cola = NULL;
    c->cantidad = 0;
}

void cola_encolar(cola_t *c, persona_t *p)
{
    if (c == NULL || p == NULL)
        return;

    nodo_persona_t *n = calloc(1, sizeof *n);
    if (n == NULL)
        return;

    n->persona = p;
    n->siguiente = NULL;

    if (c->cola == NULL)
    {
        c->cabeza = n;
        c->cola = n;
    }
    else
    {
        c->cola->siguiente = n;
        c->cola = n;
    }

    c->cantidad++;
}

persona_t *cola_desencolar(cola_t *c)
{
    if (c == NULL || c->cabeza == NULL)
        return NULL;

    nodo_persona_t *n = c->cabeza;
    persona_t *p = n->persona;

    c->cabeza = n->siguiente;
    if (c->cabeza == NULL)
        c->cola = NULL;

    free(n);
    c->cantidad--;

    return p;
}

int cola_cantidad(const cola_t *c)
{
    return (c != NULL) ? c->cantidad : 0;
}

bool cola_vacia(const cola_t *c)
{
    return (c == NULL) || (c->cantidad == 0);
}
