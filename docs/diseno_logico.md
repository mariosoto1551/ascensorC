# Diseño Lógico del Sistema de Ascensor

### Arquitectura Modular con Monitoreo y Sincronización

**Universidad Privada Boliviana** — Facultad de Ingenierías y Arquitectura
**Materia:** Sistemas Operativos
**Docente:** Mgr. Luis Marcel Barrero Mendizabal
**Fecha de entrega:** 5 de septiembre de 2026

**Autores:**

- Josué Gabriel Linares Herrera
- Edgar Alejandro Toro Delgadillo
- Jose Sebastian Rodriguez Choque

> Documento de diseño conceptual. Define el contrato entre módulos; no contiene
> implementación específica.

---

## Índice

1. [Introducción y Alcance](#1-introducción-y-alcance)
2. [Arquitectura General](#2-arquitectura-general)
3. [Diseño de los Módulos](#3-diseño-de-los-módulos)
4. [Modelado de Datos](#4-modelado-de-datos)
5. [Flujo Lógico de Eventos](#5-flujo-lógico-de-eventos)
6. [Sincronización con Mutex y Variables de Condición](#6-sincronización-con-mutex-y-variables-de-condición)
7. [Diagrama de Arquitectura del Sistema](#7-diagrama-de-arquitectura-del-sistema)
8. [Conclusión](#8-conclusión)

---

## 1. Introducción y Alcance

El presente documento describe la lógica de funcionamiento de un sistema de ascensor
concurrente, desarrollado como proyecto para la materia de Sistemas Operativos.

El sistema modela:

- Un edificio de **n pisos** (3 ≤ n ≤ X, con X = 10 para diseño).
- Un ascensor con capacidad de **c personas** (0 < c < Y, con Y = 8 para diseño).

La comunicación entre los hilos se gestiona mediante **señales POSIX**:

| Señal | Efecto |
|---|---|
| `SIGUSR1` | Creación de una nueva persona (hilo) que ingresa al edificio. |
| `SIGINT` / `SIGTERM` | Finalización ordenada del programa, vaciando el edificio. |

Para garantizar la robustez y el orden se adopta una **arquitectura modular estricta**
que separa la inicialización, la sincronización, el control de movimiento, el manejo
de interrupciones y la visualización.

Restricciones de diseño autoimpuestas:

- Se priorizan las **estructuras dinámicas**.
- Se **prohíbe explícitamente el uso de variables globales**.
- Todo el estado compartido se centraliza en el módulo `shared`, que encapsula el
  mutex y la variable de condición.

---

## 2. Arquitectura General

El sistema se organiza en **siete módulos independientes**, cada uno con una
responsabilidad única y bien definida. Esta separación permite el trabajo en paralelo
del equipo y facilita las pruebas unitarias.

| Archivo | Tipo | Responsabilidad |
|---|---|---|
| `main.c` | Orquestador | Inicializa `shared_t`, crea los hilos del ascensor, del manejador de señales y del monitor; espera la terminación ordenada. **No contiene lógica de sincronización ni de negocio.** |
| `shared.c` / `.h` | Sincronización | Encapsula el mutex global, la variable de condición, las banderas del sistema (`terminando`, `hay_llamadas`) y el contador de personas activas. Provee una API funcional para bloquear, desbloquear y esperar. |
| `elevator.c` / `.h` | Lógica de movimiento | Contiene las colas por piso, la estructura del ascensor y la lógica de movimiento (algoritmo SCAN). Usa el mutex de `shared` para proteger sus datos. |
| `signal_handler.c` / `.h` | Gestor de interrupciones | Hilo dedicado a esperar señales con `sigwait`. Traduce `SIGUSR1` en creación de persona y `SIGINT` en solicitud de terminación. |
| `persona.c` / `.h` | Ciclo de vida del trabajador | Define la rutina de cada hilo persona: trabajar, elegir destino aleatorio, gestionar la jornada laboral y encolarse/desencolarse. |
| `colas.c` / `.h` | Estructura de datos | Implementación de la cola FIFO mediante lista enlazada simple. Usada internamente por `elevator` para gestionar las esperas por piso y dirección. |
| `monitor.c` / `.h` | Visualización (ncurses) | Hilo que utiliza `ncurses` para mostrar el estado del sistema en tiempo real. **Solo lee** información a través de snapshots; nunca modifica datos. |

*Cuadro 1: Estructura de archivos y responsabilidades modulares.*

---

## 3. Diseño de los Módulos

### 3.1. `main.c` — Orquestador

- Inicializa la estructura `shared_t` con `shared_init()`.
- Crea el módulo `elevator` pasándole el puntero a `shared_t`.
- Crea los hilos permanentes: `elevator_run`, `signal_handler_run` y `monitor_run`.
- Espera la terminación de los hilos con `pthread_join`.
- Llama a `shared_destroy()` y `elevator_destroy()` para liberar recursos.

### 3.2. `shared.c` — Sincronización y estado global

**Datos privados.** Define una estructura `shared_t`:

```c
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    volatile int    terminando;
    int             personas_activas;
    bool            hay_llamadas;
} shared_t;
```

**API pública:**

```c
void shared_init(shared_t *s);
void shared_destroy(shared_t *s);
void shared_lock(shared_t *s);
void shared_unlock(shared_t *s);
void shared_wait(shared_t *s);                    /* wrapper de cond_wait */
void shared_broadcast(shared_t *s);
void shared_signal(shared_t *s);
void shared_set_terminando(shared_t *s);
bool shared_is_terminando(shared_t *s);
void shared_inc_personas(shared_t *s);
void shared_dec_personas(shared_t *s);
void shared_set_llamadas(shared_t *s, bool valor);
```

### 3.3. `elevator.c` — Lógica del ascensor

**Datos privados.** Define una estructura `elevator_t`:

```c
typedef struct elevator {
    int         num_pisos;
    cola_t     *colas_subida;
    cola_t     *colas_bajada;
    ascensor_t  ascensor;
    shared_t   *shared;
} elevator_t;
```

**API pública:**

```c
elevator_t *elevator_create(int num_pisos, shared_t *shared);
void        elevator_destroy(elevator_t *e);
void        elevator_crear_persona(elevator_t *e);
void       *elevator_run(void *arg);              /* bucle principal */
Snapshot    elevator_get_snapshot(elevator_t *e);
```

**Comportamiento.** Usa `shared_lock()` y `shared_unlock()` para proteger sus datos.
Cuando no hay llamadas, llama a `shared_wait()` para dormir hasta que haya trabajo.

### 3.4. `signal_handler.c` — Manejo de señales

- Hilo que ejecuta `signal_handler_run(void *arg)`, donde `arg` da acceso a `shared_t`.
- Usa `sigwait()` para esperar `SIGUSR1`, `SIGINT` y `SIGTERM`.
- Al recibir `SIGUSR1`: llama a `elevator_crear_persona()` (requiere acceso al
  `elevator`, que se pasa como argumento adicional o se almacena en el contexto).
- Al recibir `SIGINT` o `SIGTERM`: llama a `shared_set_terminando()` y hace
  `shared_broadcast()`.

### 3.5. `persona.c` — Rutina del trabajador

- Cada persona es un hilo que ejecuta `persona_run(void *arg)`.
- Datos de la persona: piso actual, destino, jornada laboral restante, tiempo de
  trabajo actual.

**Flujo:**

1. Se encola en el piso actual (usa la API de `elevator` o `shared`).
2. Espera a ser recogida por el ascensor.
3. Viaja al destino (el ascensor la mueve).
4. Trabaja un tiempo aleatorio.
5. Reduce su jornada laboral.
6. Elige nuevo destino (o PB si la jornada se agotó).
7. Repite hasta salir.

> Solo modifica sus propios atributos (destino, jornada). **No toca el mutex
> directamente.**

### 3.6. `monitor.c` — Visualización con ncurses

- Hilo que ejecuta `monitor_run(void *arg)`, donde `arg` es un puntero a `elevator_t`.
- Inicializa `ncurses` y entra en un bucle de refresco (ej. cada 200 ms).
- En cada iteración:
  1. Toma un snapshot llamando a `elevator_get_snapshot()` (esta función bloquea el
     mutex de `shared` internamente).
  2. Dibuja la información en pantalla: pisos, colas de espera, posición del
     ascensor, ocupación, etc.
- Cuando detecta que `shared_is_terminando()` es verdadero, sale del bucle y restaura
  la terminal.

---

## 4. Modelado de Datos

### 4.1. Estructura del ascensor

```c
typedef struct {
    int        piso_actual;
    int        estado;               /* DETENIDO, SUBIENDO, BAJANDO */
    int        ocupacion;
    persona_t *pasajeros[CAPACIDAD];
} ascensor_t;
```

### 4.2. Estructura de la cola (lista enlazada)

```c
typedef struct nodo_persona {
    persona_t           *persona;
    struct nodo_persona *siguiente;
} nodo_persona_t;

typedef struct {
    nodo_persona_t *cabeza;
    nodo_persona_t *cola;
    int             cantidad;
} cola_t;
```

### 4.3. Datos de la persona

```c
typedef struct persona {
    int       id;
    int       piso_actual;
    int       destino;
    int       direccion;             /* +1 (subir), -1 (bajar) */
    int       jornada_restante;
    int       tiempo_trabajo;
    pthread_t hilo;
} persona_t;
```

### 4.4. Snapshot para el monitor

```c
typedef struct {
    int piso_ascensor;
    int estado_ascensor;
    int ocupacion;
    int colas_subida[MAX_PISOS];     /* personas esperando por piso */
    int colas_bajada[MAX_PISOS];
    /* ... otros datos relevantes */
} Snapshot;
```

---

## 5. Flujo Lógico de Eventos

### 5.1. Ciclo de vida de una persona

1. **Creación.** `signal_handler` recibe `SIGUSR1` → `elevator_crear_persona()` → se
   reserva un PCB (estructura persona) y se lanza el hilo.
2. **Inicio.** La persona inicia en PB (piso 0) con destino aleatorio y jornada
   laboral aleatoria.
3. **Encolamiento.** Se encola en la cola correspondiente a su piso actual (solo
   subida en PB, solo bajada en el último piso).
4. **Llamada al ascensor.** Activa la bandera `hay_llamadas` y despierta al ascensor
   mediante `shared_signal()`.
5. **Espera.** La persona se bloquea esperando que el ascensor la recoja.
6. **Viaje.** El ascensor la carga y la transporta al destino.
7. **Trabajo.** En el destino, si la jornada no ha terminado, trabaja un tiempo
   aleatorio, resta de la jornada y elige nuevo destino (PB si jornada ≤ 0).
8. **Salida.** Si llega a PB con la jornada agotada, sale del edificio, decrementa el
   contador de personas activas y el hilo finaliza.

### 5.2. Lógica de movimiento del ascensor

1. **Espera pasiva.** Si no hay llamadas ni pasajeros, llama a `shared_wait()`
   (dormido sobre la condición).
2. **Decisión de dirección.** Prioriza la dirección actual (**algoritmo SCAN**).
   Mientras sube, solo atiende llamadas de pisos superiores; si no hay, invierte.
3. **Recorrido.** Simula el tiempo de viaje con `sleep()` **fuera del mutex**.
4. **Carga/descarga en piso.** Bloquea el mutex, descarga pasajeros cuyo destino
   coincide con el piso, carga pasajeros que esperan en la misma dirección (si hay
   espacio), libera el mutex.
5. **Ascensor lleno.** Omite la carga y continúa el recorrido.

### 5.3. Terminación del sistema

1. `signal_handler` recibe `SIGINT` → `shared_set_terminando()` y `shared_broadcast()`.
2. Todas las personas fuerzan destino a PB y se encolan para bajar.
3. El ascensor deja de recoger nuevas personas; solo transporta a los pasajeros
   actuales a PB.
4. El `main` espera a que `personas_activas == 0` y `ascensor.ocupacion == 0`
   (vía `shared` y `elevator`).
5. Se hace `pthread_join` de todos los hilos y se destruyen los recursos.

---

## 6. Sincronización con Mutex y Variables de Condición

### 6.1. Estrategia de protección

- **Mutex único.** `shared_t.mutex` protege *todos* los datos compartidos: colas,
  ascensor, banderas y contadores.
- **Propiedad.** Ningún módulo accede al mutex directamente; todos usan las funciones
  de `shared` (`shared_lock`, `shared_unlock`, etc.).
- **Regla de oro.** Nunca mantener el mutex durante operaciones bloqueantes
  (`sleep`, `join`, I/O, `ncurses`).

### 6.2. Variable de condición

`shared_t.cond` permite que el ascensor se duerma cuando no hay trabajo, evitando la
espera activa.

**Uso correcto.** Siempre con un bucle `while`, para protegerse de despertares
espurios y del robo de trabajo por otro hilo:

```c
shared_lock(s);
while (!hay_llamadas && !terminando) {
    shared_wait(s);          /* libera el mutex y duerme */
}
shared_unlock(s);
```

**Signal vs. broadcast:**

| Función | Cuándo se usa |
|---|---|
| `shared_signal()` | Despierta al ascensor cuando una persona se encola. |
| `shared_broadcast()` | Despierta a **todos** los hilos durante el apagado. |

---

## 7. Diagrama de Arquitectura del Sistema

```text
                    ┌─────────────────────────────────────────┐
                    │                 main.c                  │
                    │             (orquestador)               │
                    │    shared_init() -> elevator_create()   │
                    │    -> pthread_create x3 -> join         │
                    └───────┬──────────┬──────────────┬───────┘
                     crea   │          │ crea         │  crea
              ┌─────────────┘          │              └─────────────┐
              ▼                        ▼                            ▼
   ┌────────────────────┐   ┌────────────────────┐   ┌────────────────────┐
   │  signal_handler.c  │   │     elevator.c     │   │      monitor.c     │
   │   hilo: sigwait()  │   │  hilo: bucle SCAN  │   │   hilo: ncurses    │
   └─────────┬──────────┘   └───┬────────────┬───┘   └─────────┬──────────┘
             │                  │            │                 │
             │ SIGUSR1:         │ lock /     │ usa             │ get_snapshot()
             │ crear_persona()  │ unlock /   │                 │ (solo lectura)
             │                  │ wait       ▼                 │
             │                  │      ┌───────────┐           │
             │                  │      │  colas.c  │           │
             │                  │      │ FIFO por  │           │
             │                  │      │  piso y   │           │
             │                  │      │ dirección │           │
             │                  │      └───────────┘           │
             │                  ▼                              │
             │        ┌──────────────────────┐                 │
             └───────►│       shared.c       │◄────────────────┘
                      │   mutex + cond +     │
                      │  banderas + contador │
                      │                      │
                      │   ÚNICO POSEEDOR     │
                      │      DEL MUTEX       │
                      └──────────▲───────────┘
                                 │ lock / wait
                      ┌──────────┴───────────┐
                      │      persona.c       │
                      │   N hilos efímeros   │
                      │ (uno por trabajador) │
                      └──────────────────────┘
```

*Figura 1: Arquitectura del sistema.*

**Leyenda:**

| Símbolo | Significado |
|---|---|
| `──►` | Llamada a la API (con o sin mutex). |
| `crea` | Creación de hilo con `pthread_create`. |
| `shared.c` | Único poseedor del mutex: todo acceso a estado compartido pasa por aquí. |

---

## 8. Conclusión

La arquitectura propuesta separa de manera clara y profesional las responsabilidades
del sistema:

| Módulo | Rol en una línea |
|---|---|
| `main.c` | Orquestador sin lógica de negocio. |
| `shared.c` | Corazón de la sincronización (mutex, condición, banderas). |
| `elevator.c` | Dueño de los datos de dominio y de la lógica de movimiento. |
| `signal_handler.c` | Gestor síncrono de señales. |
| `persona.c` | Unidad de trabajo efímera. |
| `colas.c` | Estructura de datos auxiliar. |
| `monitor.c` | Visualización exclusiva con ncurses. |

Esta modularidad permite al equipo trabajar en paralelo sin pisar el código de los
demás, cumpliendo estrictamente con la prohibición de variables globales y
centralizando la complejidad de la concurrencia en el módulo `shared`.

El uso de un mutex global simplifica la detección de deadlocks, mientras que el bucle
`while` en la condición garantiza robustez ante despertares espurios. El resultado es
un sistema predecible, escalable dentro de los límites del problema y fácil de
depurar.
