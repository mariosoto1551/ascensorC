# Diseño Lógico del Sistema de Ascensor

**Arquitectura Modular con Monitoreo y Sincronización**

**Autores:**  
Josué Gabriel Linares Herrera  
Edgar Alejandro Toro Delgadillo  
Jose Sebastian Rodriguez Choque  

**Docente:** Mgr. Luis Marcel Barrero Mendizabal  
**Fecha:** 6 de septiembre de 2026  

---

## 1. Introducción y Alcance

El presente documento describe la lógica de funcionamiento de un sistema de ascensor concurrente desarrollado como proyecto final para la materia de Sistemas Operativos. El sistema modela un edificio de `n` pisos (`3 ≤ n ≤ 7`) y un ascensor con capacidad para **8 personas**. La comunicación entre hilos se gestiona mediante señales POSIX:

- `SIGUSR1`: Creación de una nueva persona (hilo) que ingresa al edificio.
- `SIGINT` / `SIGTERM`: Finalización ordenada del programa, vaciando el edificio.

La arquitectura es estrictamente modular: se prohíbe el uso de variables globales, y todo el estado compartido reside en una estructura `shared_t` que encapsula el mutex y la variable de condición. El sistema consta de siete módulos independientes, cada uno con una responsabilidad única.

---

## 2. Arquitectura General

El sistema se organiza en siete módulos independientes, cada uno con una responsabilidad única y bien definida. Esta separación permite el trabajo en paralelo del equipo y facilita las pruebas unitarias.

| Archivo | Tipo | Responsabilidad |
|---------|------|-----------------|
| `main.c` | Orquestador | Inicializa `shared_t`, crea los hilos del ascensor, del manejador de señales y del monitor; espera la terminación ordenada. No contiene lógica de sincronización ni de negocio. |
| `shared.c / .h` | Sincronización | Encapsula el mutex global, la variable de condición, las banderas del sistema (`terminando`, `hay_llamadas`) y el contador de personas activas. Provee una API funcional mínima (sin wrappers simples). |
| `elevator.c / .h` | Lógica de Movimiento | Contiene las colas por piso, la estructura del ascensor y la lógica de movimiento (algoritmo SCAN). Usa el mutex de `shared` para proteger sus datos. |
| `signal_handler.c / .h` | Gestor de Interrupciones | Hilo dedicado a esperar señales con `sigwait`. Traduce `SIGUSR1` en creación de persona y `SIGINT` en solicitud de terminación. |
| `persona.c / .h` | Ciclo de Vida del Trabajador | Define la rutina de cada hilo persona: trabajar, elegir destino aleatorio, gestionar la jornada laboral y encolarse/desencolarse. |
| `colas.c / .h` | Estructura de Datos | Implementación de la cola FIFO mediante lista enlazada simple. Usada internamente por `elevator` para gestionar las esperas por piso y dirección. |
| `monitor.c / .h` | Visualización (ncurses) | Hilo que utiliza `ncurses` para mostrar el estado del sistema en tiempo real. Solo lee información a través de snapshots; nunca modifica datos. |

---

## 3. Diseño de los Módulos

### 3.1. `main.c` (Orquestador)

- Inicializa la estructura `shared_t` con `shared_init()`.
- Crea el módulo `elevator` pasándole el puntero a `shared_t` (mediante `elevator_create`, que devuelve un código de error).
- Crea los hilos permanentes: `elevator_run`, `signal_handler_run` y `monitor_run`.
- Espera la terminación de los hilos con `pthread_join`.
- Llama a `shared_destroy()` y `elevator_destroy()` para liberar recursos.

---

### 3.2. `shared.c` (Sincronización y Estado Global)

**Datos privados:** Define una estructura `shared_t` que contiene:

```c
typedef struct shared {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int terminando;          // 0 = en marcha, 1 = apagado solicitado
    int personas_activas;    // contador de hilos persona vivos
    bool hay_llamadas;       // true si hay alguien esperando en cola
} shared_t;

# 3. API y Módulos del Sistema

## 3.2. `shared.c` — Sincronización

### API pública

* `shared_status_t shared_init(shared_t *s);`
  Inicializa el mutex, la condición y las banderas. Devuelve un código de error.

* `shared_status_t shared_destroy(shared_t *s);`
  Destruye el mutex y la condición. Devuelve un código de error.

* `void shared_set_terminando(shared_t *s);`
  Activa la bandera de apagado (`terminando = 1`) y hace `pthread_cond_broadcast` para despertar a todos los hilos.

* `bool shared_is_terminando(shared_t *s);`
  Consulta el estado de la bandera de apagado (lectura con mutex).

* `int shared_get_personas_activas(shared_t *s);`
  Devuelve el número de personas activas (lectura con mutex).

### Decisión de diseño

Se eliminaron los wrappers simples (`shared_lock`, `shared_unlock`, `shared_wait`, `shared_signal`, `shared_broadcast`, `shared_inc_personas`, `shared_dec_personas`) para mantener el módulo minimalista.

Cada módulo usa directamente las primitivas de pthreads (`pthread_mutex_lock`, `pthread_cond_wait`, etc.) accediendo a los campos de `shared_t`.

---

## 3.3. `elevator.c` — Lógica del Ascensor

### Datos privados

Define una estructura `elevator_t` que contiene:

```c
typedef struct elevator {
    int num_pisos;
    cola_t *colas_subida;
    cola_t *colas_bajada;
    ascensor_t ascensor;
    shared_t *shared;
    int siguiente_id;
    bool evacuacion_iniciada;
} elevator_t;
```

### API pública

* `elevator_status_t elevator_create(elevator_t **out, int num_pisos, shared_t *shared);`
  Crea el ascensor, reserva memoria para las colas y devuelve un código de error (`elevator_status_t`). Recibe un puntero doble para devolver la instancia.

* `void elevator_destroy(elevator_t *e);`
  Libera las colas y la estructura.

* `void *elevator_run(void *arg);`
  Bucle principal del ascensor (algoritmo SCAN).

* `elevator_status_t elevator_crear_persona(elevator_t *e);`
  Crea una nueva persona y lanza su hilo. Devuelve un código de error (`ELEVATOR_ERR_TERMINATING`, `ELEVATOR_ERR_MEMORY`, etc.).

* `void elevator_encolar_persona(elevator_t *e, persona_t *p, int destino);`
  Encola a la persona en el piso actual según la dirección. Escribe `p->destino` con el mutex tomado.

* `void elevator_esperar_llegada(elevator_t *e, persona_t *p);`
  Bloquea al hilo de la persona hasta que el ascensor la lleve a su destino (cuando `p->piso_actual == p->destino`).

* `Snapshot elevator_get_snapshot(elevator_t *e);`
  Toma una copia del estado del ascensor y las colas (devuelve por valor). Bloquea el mutex solo para copiar y luego lo libera.

* `const char *elevator_estado_texto(int estado);`
  Convierte el estado (`-1`, `0`, `+1`) en cadena (`"BAJANDO"`, `"DETENIDO"`, `"SUBIENDO"`) para el monitor.

### Comportamiento

Usa `pthread_mutex_lock` y `pthread_mutex_unlock` directamente sobre `shared->mutex` para proteger sus datos.

Cuando no hay llamadas, llama a `pthread_cond_wait` para dormir hasta que haya trabajo.

### Manejo de errores

Las funciones que pueden fallar devuelven un código del `enum elevator_status_t`.

Se eliminó el campo `last_error` en favor de retornos directos, haciendo la API más segura y explícita.

---

## 3.4. `signal_handler.c` — Manejo de Señales

* Hilo que ejecuta `signal_handler_run(void *arg)`, donde `arg` es un puntero a `signal_handler_t` que contiene punteros a `shared_t` y `elevator_t`.
* Usa `sigwait()` para esperar de forma síncrona las señales `SIGUSR1`, `SIGINT` y `SIGTERM`.
* Al recibir `SIGUSR1`: llama a `elevator_crear_persona()` y maneja el posible error.
* Al recibir `SIGINT` o `SIGTERM`: llama a `shared_set_terminando()`, lo que inicia la evacuación.
* **Nota:** El hilo principal bloquea las señales antes de crear los hilos, para que solo `signal_handler` las reciba.

---

## 3.5. `persona.c` — Rutina del Trabajador

* Cada persona es un hilo que ejecuta `persona_run(void *arg)`.

### Datos de la persona

```c
typedef struct persona {
    int id;
    int piso_actual;
    int destino;
    int direccion;       // +1 (subir), -1 (bajar)
    int jornada_restante;
    int tiempo_trabajo;
    elevator_t *elevator;
    pthread_t hilo;
} persona_t;
```

### Flujo de vida

1. La persona se crea en PB con una jornada aleatoria (2-4 unidades) y un tiempo de trabajo aleatorio (80-220 ms).
2. Si está en su destino, elige un nuevo destino aleatorio entre `1` y `num_pisos - 1`, a menos que su jornada se haya agotado, en cuyo caso elige PB.
3. Se encola usando `elevator_encolar_persona()` y espera a que el ascensor la lleve al destino usando `elevator_esperar_llegada()`.
4. Al llegar a un piso que no es PB, simula trabajo en pasos de 10 ms, decrementa su jornada en 1 unidad y, si ésta llega a 0, se encola a PB para salir.
5. **Salida inmediata:** Si la persona llega a PB (destino PB), sale del edificio inmediatamente, sin importar si le queda jornada. Decrementa `personas_activas`, libera su memoria y termina.

> **Nota:** Se eliminó el bloque de salida anticipada por apagado (`if (shared_is_terminando(s) && p->piso_actual == PISO_PB)`) para evitar liberar personas que aún estaban en colas. La salida solo ocurre en el paso 4 (llegada a PB).

---

## 3.6. `monitor.c` — Visualización `ncurses`

* Hilo que ejecuta `monitor_run(void *arg)`, donde `arg` es un puntero a `elevator_t`.
* Inicializa `ncurses` (con colores si la terminal lo soporta) y entra en un bucle de refresco cada 200 ms.
* En cada iteración:

  1. Toma un snapshot usando `elevator_get_snapshot()`, que copia el estado con el mutex bloqueado y lo devuelve por valor.
  2. Dibuja el edificio:

     * Cada piso en una fila (el más alto arriba).
     * Colas de subida con `^` y colas de bajada con `v`, truncando a 9 caracteres para evitar desbordamientos.
     * Cabina con `#` para asientos ocupados, mostrando el número de ocupados y la capacidad.
     * Destinos de los pasajeros a bordo.
  3. Muestra información resumida: estado del ascensor, piso, ocupación, total esperando y personas activas.
* Termina cuando el sistema está en apagado y el edificio está completamente vacío (`terminando`, `personas_activas`, `personas_dentro` y `total_esperando` son 0).
* Al finalizar, restaura la terminal con `endwin()`.
* **Nota:** Si la salida está redirigida a un archivo, no inicializa `ncurses` y espera en silencio.

---

# 4. Modelado de Datos

## 4.1. Estructura del Ascensor

```c
typedef struct {
    int piso_actual;
    int estado;          // DETENIDO, SUBIENDO, BAJANDO
    int personas_dentro;
    persona_t *pasajeros[CAPACIDAD];
} ascensor_t;
```

---

## 4.2. Estructura de la Cola — Lista Enlazada

```c
typedef struct nodo_persona {
    persona_t *persona;
    struct nodo_persona *siguiente;
} nodo_persona_t;

typedef struct {
    nodo_persona_t *cabeza;
    nodo_persona_t *cola;
    int cantidad;
} cola_t;
```

---

## 4.3. Datos de la Persona

```c
typedef struct persona {
    int id;
    int piso_actual;
    int destino;
    int direccion;       // +1 (subir), -1 (bajar)
    int jornada_restante;
    int tiempo_trabajo;
    elevator_t *elevator;
    pthread_t hilo;
} persona_t;
```

---

## 4.4. Snapshot para el Monitor

```c
typedef struct {
    int piso_ascensor;
    int estado_ascensor;
    int personas_dentro;
    int capacidad;
    int num_pisos;
    int personas_activas;
    int total_esperando;
    bool terminando;
    bool evacuando;
    int colas_subida[MAX_PISOS];
    int colas_bajada[MAX_PISOS];
    int destinos[CAPACIDAD];
} Snapshot;
```

---

# 5. Flujo Lógico de Eventos

## 5.1. Ciclo de Vida de una Persona

1. **Creación:** `signal_handler` recibe `SIGUSR1` → `elevator_crear_persona()` → se reserva la estructura `persona_t` y se lanza el hilo.
2. **Inicio:** La persona inicia en PB con una jornada aleatoria (2-4) y un tiempo de trabajo aleatorio (80-220 ms).
3. **Encolamiento:** Se encola en la cola correspondiente a su piso actual (solo subida en PB, solo bajada en el último piso) usando `elevator_encolar_persona`.
4. **Llamada al ascensor:** Se activa `hay_llamadas` y se hace `pthread_cond_signal` para despertar al ascensor.
5. **Espera:** La persona se bloquea en `elevator_esperar_llegada` hasta que su `piso_actual` coincida con su `destino`.
6. **Viaje:** El ascensor carga a la persona y la transporta al destino.
7. **Trabajo:** Al llegar a un piso que no es PB, la persona simula trabajo en pasos de 10 ms, decrementa su jornada en 1 unidad y, si ésta llega a 0, se encola a PB para salir.
8. **Salida:** Si la persona llega a PB (destino PB), decrementa el contador `personas_activas`, libera su memoria (`persona_destroy`) y termina su hilo.

---

## 5.2. Lógica de Movimiento del Ascensor — Elevator

1. **Espera pasiva:** Si no hay llamadas ni pasajeros, el ascensor se duerme en `pthread_cond_wait` (nunca espera activa).
2. **Decisión de dirección:** Usa el algoritmo SCAN: mantiene la dirección mientras haya trabajo en esa dirección; solo cambia cuando se agota (función `decidir_direccion`).
3. **Recorrido:** Simula el tiempo de viaje entre pisos (30 ms) y el tiempo de puertas (5 ms si hubo parada) con `dormir_ms` **fuera del mutex** (regla de oro).
4. **Carga/Descarga en piso:** Bloquea el mutex, descarga a los pasajeros cuyo destino coincide, ajusta el rumbo si el ascensor quedó vacío, y carga a quienes esperan en la dirección actual (si hay espacio).
5. **Ascensor lleno:** Omite la carga y continúa el recorrido.

---

## 5.3. Terminación del Sistema

1. `signal_handler` recibe `SIGINT` → `shared_set_terminando()` → activa la bandera y hace `broadcast`.
2. El ascensor, al detectar la bandera, ejecuta `iniciar_evacuacion()` (una sola vez), que:

   * Fuerza el destino de todos los pasajeros a bordo a PB y pone su jornada a 0.
   * Mueve a todas las personas de las colas de subida a las colas de bajada, forzando destino PB y jornada 0.
   * Recorre las colas de bajada forzando destino PB y jornada 0 en todas las personas.
3. Las personas, al llegar a PB, salen del edificio.
4. El ascensor continúa moviéndose hasta que el edificio esté vacío (`personas_activas == 0`, `personas_dentro == 0`, `total_esperando == 0`).
5. El `main` hace `pthread_join` de los hilos y destruye los recursos.

---

# 6. Sincronización con Mutex y Variables de Condición

## 6.1. Estrategia de Protección

* **Mutex único:** `shared_t.mutex` protege **todos los datos compartidos** (colas, ascensor, banderas, contadores). Los módulos acceden al mutex directamente mediante `pthread_mutex_lock` y `pthread_mutex_unlock`.
* **Propiedad:** Ningún módulo accede al mutex indirectamente; cada uno usa las funciones de pthreads sobre `shared_t.mutex`.
* **Regla de oro:** Nunca mantener el mutex durante operaciones bloqueantes (`sleep`, `join`, I/O, `ncurses`). Por eso, el ascensor libera el mutex antes de simular el viaje con `dormir_ms`.

---

## 6.2. Variable de Condición

* `shared_t.cond` permite que el ascensor se duerma cuando no hay trabajo, evitando espera activa.
* **Uso correcto:** Siempre con un bucle `while` para proteger contra señales espurias y robos de trabajo:

```c
pthread_mutex_lock(&s->mutex);

while (!hay_trabajo(e) && !fin_de_jornada(e)) {
    pthread_cond_wait(&s->cond, &s->mutex);
}

pthread_mutex_unlock(&s->mutex);
```

### `Signal` vs `Broadcast`

* `pthread_cond_signal`: Se usa para despertar al ascensor cuando una persona se encola.
* `pthread_cond_broadcast`: Se usa durante el apagado para despertar a todos los hilos (ascensor y personas).

---

# 7. Diagrama de Arquitectura del Sistema

El siguiente diagrama muestra la interacción entre los módulos y el flujo de datos.

[Diagrama de Arquitectura](https://diagrama%2520arch%25201.png/)

### Leyenda

* **Línea sólida:** Llamada a la API (con o sin mutex).
* **Línea discontinua:** Interacción lógica entre hilos.
* **SHARED:** Único poseedor del mutex.

---

# 8. Conclusión

La arquitectura propuesta separa de manera clara y profesional las responsabilidades del sistema:

* `main.c`: Orquestador sin lógica de negocio.
* `shared.c`: Corazón de la sincronización (mutex, condición, banderas).
* `elevator.c`: Dueño de los datos de dominio y lógica de movimiento.
* `signal_handler.c`: Gestor síncrono de señales.
* `persona.c`: Unidad de trabajo efímera.
* `colas.c`: Estructura de datos auxiliar.
* `monitor.c`: Visualización exclusiva con `ncurses`.

Esta modularidad permite al equipo trabajar en paralelo sin pisar el código de los demás, cumpliendo estrictamente con la prohibición de variables globales y centralizando la complejidad de la concurrencia en el módulo `shared`.

El uso de un mutex global simplifica la detección de deadlocks, mientras que el bucle `while` en la condición garantiza robustez ante señales espurias.

El resultado es un sistema predecible, escalable dentro de los límites del problema y fácil de depurar.
