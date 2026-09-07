# Bitácora de Diseño – Sistema de Ascensor Concurrente

**Fecha:** 2 de septiembre de 2026

**Participante:** Josue Linares, Alejandro Toro, Jose Rodriguez

**Materia:** Sistemas Operativos

**Docente:** Mgr. Luis Marcel Barrero Mendizabal

---

## 1. Inicio de la sesión de diseño

Comenzamos la jornada con el objetivo de definir la lógica de funcionamiento del sistema de ascensor concurrente, que será implementado como proyecto final de la materia. El problema planteado involucra:

* Un edificio de `n` pisos (3 ≤ n ≤ 10).
* Un ascensor con capacidad `c` (0 < c < 8).
* Hilos que representan personas que se mueven entre pisos.
* Señales POSIX (`SIGUSR1` para crear personas, `SIGINT`/`SIGTERM` para terminar).
* Sincronización mediante mutex y variables de condición.

---

## 2. Primer acercamiento: Lógica del ascensor

Inicialmente, nos enfocamos en comprender el flujo de eventos. Definimos:

* **Ciclo de vida de una persona:** Creación en PB → encolado → viaje → trabajo → nuevo destino → salida.
* **Comportamiento del ascensor:** Estados (DETENIDO, SUBIENDO, BAJANDO), algoritmo SCAN (prioridad de dirección), carga/descarga en pisos.
* **Terminación:** Apagado ordenado con vaciado completo del edificio.

En esta fase, también acordamos el uso de **colas dinámicas** (listas enlazadas) para manejar las esperas por piso, y un **arreglo dinámico de colas** (usando `calloc`) para acceder eficientemente a cada piso.

**Decisión clave:** Usar dos colas por piso (subida y bajada) para alinear la prioridad de dirección del ascensor.

---

## 3. Debate sobre la arquitectura modular

El equipo discutió la necesidad de separar responsabilidades. Surgieron varias preguntas:

* ¿El `main` debe contener lógica de sincronización? → **No**, debe ser solo un orquestador.
* ¿Quién maneja el mutex y la condición? → Se propuso un módulo central.
* ¿Cómo se llama ese módulo? → En mi primera propuesta, lo llamé "monitor", siguiendo el concepto clásico de Hoare.

### 3.1. Confusión terminológica

Aquí cometí un error conceptual: en mi experiencia previa (y en el proyecto anterior de simulación de SO), el **monitor** era el módulo de visualización con ncurses. Sin embargo, al hablar de "monitor" en sincronización, me refería al patrón de exclusión mutua.

Esto generó confusión hasta que revisé el documento de mi proyecto anterior y aclaré:

* **Módulo de sincronización:** Debe llamarse `shared.c` (o `core.c`), porque contiene el mutex, la condición y las banderas globales.
* **Módulo de visualización:** Debe llamarse `monitor.c` y ser exclusivamente ncurses, sin tocar la lógica de negocio.

Esta corrección fue fundamental para alinear el diseño con el enfoque del equipo.

---

## 4. Definición de la estructura de módulos

Tras varias iteraciones, acordamos la siguiente arquitectura:

| Módulo                    | Responsabilidad                                                                                                                                                |
| ------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `main.c`                  | Orquestador: inicializa `shared`, crea hilos (`elevator`, `signal_handler`, `monitor`) y espera join.                                                          |
| `shared.c` / `.h`         | Sincronización: mutex, condición, banderas (`terminando`, `hay_llamadas`), contador de personas activas. API funcional para lock/unlock/wait/signal/broadcast. |
| `elevator.c` / `.h`       | Lógica de movimiento: colas por piso, estructura del ascensor, algoritmo SCAN. Usa el mutex de `shared` para protegerse.                                       |
| `signal_handler.c` / `.h` | Manejo síncrono de señales con `sigwait`. Traduce `SIGUSR1` y `SIGINT` en acciones sobre `shared`/`elevator`.                                                  |
| `persona.c` / `.h`        | Ciclo de vida de cada trabajador (viajar, trabajar, elegir destino, gestionar jornada).                                                                        |
| `colas.c` / `.h`          | Implementación de lista enlazada para colas FIFO. Usada internamente por `elevator`.                                                                           |
| `monitor.c` / `.h`        | Visualización con ncurses: toma snapshots (solo lectura) y dibuja el estado del sistema.                                                                       |

### 4.1. Justificación de la separación `shared`/`elevator`

Uno de los momentos más importantes fue decidir si `shared` y `elevator` debían estar separados o unificados. Argumenté a favor de la separación basándome en:

* **Modularidad:** `shared` se enfoca en sincronización; `elevator` en lógica de negocio.
* **Reutilización:** `shared` podría usarse en otros simuladores.
* **Pruebas:** Se puede probar `elevator` sin inicializar el mutex (usando mocks).
* **Coherencia:** En el proyecto anterior, `system_shared_t` estaba separado de `proc_scheduler.c`.

El equipo aceptó la propuesta, y quedó claro que **el mutex vive en `shared`, pero protege los datos de `elevator`**. Es decir, `elevator` usa `shared_lock()` para acceder a sus propias colas.

---

## 5. Repaso de primitivas de sincronización

Para asegurar que todos comprendamos el uso correcto de pthreads, repasamos:

* **`pthread_mutex_t`:** Candado único que protege todo el estado compartido.
* **`pthread_cond_t`:** Permite que el ascensor se duerma cuando no hay llamadas.
* **Regla de oro:** Nunca mantener el mutex durante operaciones bloqueantes (`sleep`, `join`, I/O, ncurses).
* **`pthread_cond_wait`:** Libera el mutex automáticamente, duerme el hilo, y al despertar lo vuelve a bloquear. **Siempre usar `while`** para proteger contra señales espurias y robos de trabajo.
* **`pthread_cond_signal` vs `broadcast`:** `signal` para despertar al ascensor; `broadcast` para apagado (despierta a todos).

Este repaso fue clave para evitar errores comunes de concurrencia.

---

## 6. Generación de documentación LaTeX

Redacté el documento de diseño en LaTeX, incluyendo:

* Portada con el logo de la UPB y los datos del equipo.
* Índice y estructura clara.
* Descripción detallada de cada módulo.
* Explicación de la sincronización con mutex y condición.
* Diagrama de arquitectura (generado con Mermaid y exportado a PNG).
* Conclusión final.

**Precaución:** Añadí `\usetikzlibrary{babel}` y `\shorthandoff{<>}` para evitar conflictos entre TikZ y el español de Babel.

---

## 7. Creación del diagrama de arquitectura

Para el diagrama, utilicé **Mermaid.js**, que es mucho más preciso que DALL-E o Midjourney para diagramas técnicos.

Generé el código y lo exporté desde [mermaid.live](https://mermaid.live) como PNG.

El diagrama muestra:

* Capas: Orquestador, Núcleo del Sistema, Hilos Permanentes, Hilos Dinámicos.
* Módulos con sus responsabilidades.
* Flechas con etiquetas que describen la interacción (ej. `shared_lock/unlock/wait`).
* Colores diferenciados por tipo de módulo.

El resultado es muy similar al diagrama de mi proyecto anterior de simulador de SO, limpio y profesional.

---

## 8. Aprendizajes y reflexiones

* **Concepto de "monitor":** Aprendí que el término tiene dos significados (patrón de sincronización vs. módulo de visualización). Es crucial aclarar la terminología con el equipo.
* **Separación de responsabilidades:** Entendí que el `main` debe ser lo más tonto posible; toda la lógica debe vivir en módulos especializados.
* **Uso de Mermaid:** Herramienta excelente para diagramas de arquitectura; muy superior a dibujar a mano o usar editores gráficos.
* **Importancia de la documentación:** Escribir el diseño antes de codificar ayuda a detectar inconsistencias y alinear al equipo.

---

## 9. Próximos pasos (al 2 de septiembre)

* Compartir el documento LaTeX y el diagrama con el equipo para revisión.
* Asignar responsabilidades de codificación según los módulos definidos.
* Iniciar la implementación del esqueleto del proyecto (estructura de archivos, Makefile, funciones vacías).
* Definir una estrategia de pruebas unitarias (especialmente para `shared` y `elevator`).

---

## 10. Herramientas utilizadas

* **Editor de texto:** Overleaf / VS Code (para LaTeX).
* **Diagramas:** Mermaid.js ([mermaid.live](https://mermaid.live)).
* **Asistente de diseño:** Deepseek (IA) para refinar la lógica y la estructura.
* **Comunicación:** Reuniones virtuales con el equipo.

---

*Bitácora registrada para seguimiento del proyecto.*

---

# Bitácora de Diseño – Sesión del 4 de septiembre de 2026

**Participante:** Josue Linares, Alejandro Toro, Jose Rodriguez

**Módulo a cargo:** `shared.c` (sincronización) y `main.c` (orquestador)

---

## 1. Contexto de la sesión

Hoy definimos la arquitectura final del sistema de ascensor concurrente. Partimos del documento LaTeX de diseño lógico y nos enfocamos en concretar el módulo de sincronización (`shared`).

El objetivo era tener una API clara, segura y alineada con el estilo de código de proyectos anteriores (como el simulador de SO), evitando variables globales y priorizando la modularidad.

---

## 2. Definición de la arquitectura de módulos

Acordamos la siguiente estructura definitiva, separando responsabilidades al máximo:

| Módulo             | Responsabilidad                                                                                                                                                                      |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `main.c`           | Orquestador: inicializa `shared`, crea los hilos (`elevator`, `signal_handler`, `monitor`) y espera su terminación. Sin lógica de negocio.                                           |
| `shared.c`         | **Corazón de la sincronización.** Contiene el mutex, la condición, las banderas (`terminando`, `hay_llamadas`) y el contador de personas activas. Provee una API mínima pero segura. |
| `elevator.c`       | Dueño de los datos de dominio: colas por piso, estructura del ascensor, lógica de movimiento (SCAN). Usa el mutex de `shared` para protegerse.                                       |
| `signal_handler.c` | Hilo que usa `sigwait()` para esperar señales (`SIGUSR1`, `SIGINT`) y traducirlas en acciones sobre `shared` y `elevator`.                                                           |
| `persona.c`        | Rutina de vida de cada trabajador (viajar, trabajar, gestionar jornada).                                                                                                             |
| `colas.c`          | Implementación de lista enlazada para las colas FIFO.                                                                                                                                |
| `monitor.c`        | Visualización exclusiva con `ncurses`. Toma snapshots de solo lectura.                                                                                                               |

**Decisión personal clave:** Opté por mantener `shared` y `elevator` estrictamente separados, siguiendo la filosofía del proyecto anterior (`system_shared_t` vs `proc_scheduler.c`). Esto permite reutilizar `shared` en otros contextos y facilita las pruebas unitarias.

---

## 3. Diseño del API de `shared.h` (versión final)

Tras varias iteraciones y análisis, definí la interfaz pública del módulo `shared`.

El criterio principal fue:

> **"Mantener lo crítico, eliminar lo redundante".**

### 3.1. Estructura `shared_t`

```c
typedef struct shared {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int terminando;          // 0 = en marcha, 1 = apagado solicitado
    int personas_activas;    // Contador de hilos persona vivos
    bool hay_llamadas;       // true si hay alguien esperando en cola
} shared_t;
```

**Decisión:** No usé `volatile` porque todas las lecturas/escrituras se hacen con el mutex bloqueado, lo que garantiza visibilidad y atomicidad.

### 3.2. Funciones de ciclo de vida

```c
shared_status_t shared_init(shared_t *s);

shared_status_t shared_destroy(shared_t *s);
```

**Decisión:** Definí un `enum shared_status_t` con códigos de error específicos (siguiendo el patrón de `my_thread.h` de proyectos anteriores), en lugar de usar `errno` directamente.

Esto hace que el manejo de errores sea más semántico.

### 3.3. Funciones de control — las que sí valen la pena

```c
void shared_set_terminando(shared_t *s);

void shared_set_hay_llamadas(shared_t *s, bool valor);

int shared_get_personas_activas(shared_t *s);
```

### ¿Por qué solo estas?

`shared_set_terminando` y `shared_set_hay_llamadas` son indispensables. No son simples "setters"; encapsulan lógica crítica:

* `set_terminando` hace `lock → terminando = 1 → broadcast` (despierta a todos).
* `set_hay_llamadas(true)` hace `lock → hay_llamadas = true → signal` (despierta al ascensor).

Si permitiera que otros módulos modificaran estas banderas directamente, el riesgo de olvidar el `broadcast` o el `signal` sería altísimo, causando deadlocks.

Centralizar esta lógica en una función es la única forma de garantizar que el sistema no se bloquee.

`shared_get_personas_activas` la mantuve porque el `main` y el monitor necesitan leer este valor de forma segura. Es una operación de solo lectura, pero la encapsulación es consistente.

### 3.4. Funciones que eliminé — y por qué

`shared_lock` / `shared_unlock` / `shared_wait` / `shared_signal` / `shared_broadcast`:

Eran wrappers simples que solo llamaban a pthreads. Los eliminé para que el código sea más directo y menos verboso.

Ahora cada módulo usa directamente:

```c
pthread_mutex_lock(&shared->mutex);
pthread_cond_wait(&shared->cond, &shared->mutex);
```

`shared_inc_personas` / `shared_dec_personas`:

Consideré que incrementar/decrementar el contador es una operación tan simple que no necesita un wrapper.

Quien lo hace (`elevator` al crear, `persona` al morir) puede simplemente bloquear el mutex, modificar el entero y desbloquear.

Esto reduce el número de funciones exportadas.

`shared_is_terminando` / `shared_hay_llamadas`:

Decidí que los módulos que necesitan leer estas banderas (el ascensor, las personas) pueden bloquear el mutex y leer `shared->terminando` o `shared->hay_llamadas` directamente.

Esto elimina dos funciones "getter" que eran meramente boilerplate y mantiene el API más pequeño.

---

## 4. Revisión de Carga, Descarga y Evacuación

Continuamos la revisión con las funciones de movimiento de personas.

### 4.1. `descargar()` y `cargar()`

**Análisis:** Ambas son correctas.

`descargar` implementa el protocolo de llegada escribiendo `p->piso_actual = piso`, lo que despierta al hilo de la persona en `elevator_esperar_llegada`.

`cargar` respeta la dirección actual y solo sube si hay espacio (`personas_dentro < CAPACIDAD`).

**Decisión:** El orden `descargar → ajustar rumbo → cargar` es crítico y está bien implementado.

Primero se liberan asientos, luego se ajusta la dirección si el ascensor quedó vacío, y finalmente se llenan los asientos libres.

```c
static int descargar(elevator_t *e) {
    int piso = e->ascensor.piso_actual;
    int bajaron = 0;

    for (int i = 0; i < CAPACIDAD; i++) {
        persona_t *p = e->ascensor.pasajeros[i];

        if (p != NULL && p->destino == piso) {
            p->piso_actual = piso; /* protocolo de llegada */
            e->ascensor.pasajeros[i] = NULL;
            e->ascensor.personas_dentro--;
            bajaron++;
        }
    }

    return bajaron;
}

static int cargar(elevator_t *e) {
    int piso = e->ascensor.piso_actual;
    int dir = e->ascensor.estado;

    if (dir == ASCENSOR_DETENIDO)
        return 0;

    cola_t *cola = cola_de(e, piso, dir);
    int subieron = 0;

    while (e->ascensor.personas_dentro < CAPACIDAD && !cola_vacia(cola)) {
        persona_t *p = cola_desencolar(cola);

        if (p == NULL)
            break;

        for (int i = 0; i < CAPACIDAD; i++) {
            if (e->ascensor.pasajeros[i] == NULL) {
                e->ascensor.pasajeros[i] = p;
                break;
            }
        }

        e->ascensor.personas_dentro++;
        subieron++;
    }

    return subieron;
}
```

### 4.2. `iniciar_evacuacion()` y `fin_de_jornada()`

Detecté dos fallos críticos:

1. No se forzaba `p->jornada_restante = 0`. Las personas, al llegar a PB con jornada > 0, intentarían subir de nuevo, rompiendo el apagado.
2. No se procesaban las colas de **BAJADA**. Las personas que ya esperaban para bajar antes del apagado no eran tocadas, y al llegar a PB también intentaban seguir trabajando.

**Acción:** Propuse una corrección que:

* Fuerza `jornada_restante = 0` en todos los pasajeros a bordo y en todas las colas.
* Mueve las colas de **SUBIDA** a **BAJADA** (forzando destino PB).
* Recorre las colas de **BAJADA**, desencola a cada persona, le pone destino = PB y jornada = 0, y la vuelve a encolar (para mantener el orden FIFO).

---

# 5. Revisión del Ciclo de Vida y Gestión de Errores

Revisé `elevator_create`, `elevator_destroy` y el manejo de errores.

## 5.1. El problema de `last_error`

**Detección:** Me di cuenta de que `elevator.c` usaba `e->last_error`, pero el campo no estaba declarado en la estructura `elevator_t` de `elevator.h`. El compilador marcaría error.

**Debate:** Decidimos si mantener `last_error` o eliminarlo.

Tras analizar, optamos por una refactorización mayor para eliminar el estado mutable y hacer el manejo de errores más directo.

## 5.2. Refactorización de la API — Decisión de diseño importante

* **Cambio de firma de `elevator_create`:** Ahora devuelve `elevator_status_t` y recibe un puntero doble (`elevator_t **out`). Esto permite retornar códigos de error específicos (`ELEVATOR_ERR_MEMORY`, `ELEVATOR_ERR_INVALID_PISOS`, etc.) sin necesidad de `last_error`.

* **Cambio de firma de `elevator_crear_persona`:** Ahora devuelve `elevator_status_t` para indicar si falló por `ERR_TERMINATING`, `ERR_THREAD_CREATE`, etc.

* **Eliminación de `elevator_get_last_error`:** Ya no tiene sentido.

* **`elevator_destroy`:** Se mantiene idéntica (no necesita devolver error).

**Conclusión:** La API es ahora más robusta, segura y explícita. El error se maneja en el mismo lugar donde ocurre, evitando estados globales o mutables.

---

# 6. Revisión del Bucle Principal y API de Personas — ¡Bug crítico encontrado!

Analicé `elevator_run`, `elevator_encolar_persona` y `elevator_esperar_llegada`.

## 6.1. `elevator_run` — El bucle SCAN

**Estado:** Correcto.

Cumple la Regla de Oro (nunca dormir con mutex), usa `while` en `pthread_cond_wait` para proteger contra señales espurias, y maneja la evacuación de forma segura.

**Observación:** Se escribe `s->hay_llamadas` directamente, sin wrappers.

## 6.2. `elevator_encolar_persona` — ¡Deadlock!

**Problema crítico:** Esta función toma el mutex:

```c
pthread_mutex_lock(&s->mutex);
```

y luego llama a `shared_set_hay_llamadas(s, true)`.

**Análisis:** `shared_set_hay_llamadas` internamente vuelve a bloquear el mutex. Esto provoca un **deadlock**, porque el hilo intenta bloquear un mutex que ya posee.

**Decisión:** Eliminar `shared_set_hay_llamadas` por completo de `shared.h` y `shared.c`, ya que:

* Ya no se usa en ningún otro lugar (`elevator_run` escribe directamente).
* Su existencia es un riesgo para futuros desarrolladores.
* Se alinea con el diseño minimalista de `shared`.

**Corrección aplicada:** Reemplazar la llamada por acceso directo:

```c
s->hay_llamadas = true;
pthread_cond_signal(&s->cond);
```

## 6.3. `elevator_esperar_llegada`

**Estado:** Correcto.

Usa `while` en `pthread_cond_wait`, protegiendo contra señales espurias. La persona se duerme hasta que `piso_actual == destino`.

---

# 7. Revisión del Snapshot y Utilidades

Revisé `elevator_get_snapshot` y `elevator_estado_texto`.

### `elevator_get_snapshot`

**Perfecto.** Toma el mutex, copia todo a una estructura local (`Snapshot`) por valor, y suelta el mutex inmediatamente.

El monitor puede dibujar sin retener el bloqueo. Es el patrón correcto.

### `elevator_estado_texto`

Función utilitaria simple y segura. Devuelve literales de cadena.

---

# 8. Estado Actual del Código y Próximos Pasos

### Módulo `elevator`

* Todas las funciones han sido revisadas y corregidas.
* La evacuación ahora fuerza `jornada_restante = 0` y procesa todas las colas.
* La API de creación de errores es ahora por retorno directo (`elevator_status_t`), sin `last_error`.
* Se eliminó el deadlock en `elevator_encolar_persona` y se eliminó `shared_set_hay_llamadas` de `shared`.

### Pendientes inmediatos — al 4 de septiembre

* Actualizar `elevator.h` con las nuevas firmas (eliminar `last_error`, actualizar `elevator_create` y `elevator_crear_persona`).
* Eliminar `shared_set_hay_llamadas` de `shared.h` y `shared.c`.
* Integrar estos módulos con el `main.c` y `signal_handler.c` (que aún no han sido escritos, pero ahora tenemos una API clara para hacerlo).
* Actualizar el diagrama de arquitectura y la documentación LaTeX con las decisiones de diseño tomadas hoy (eliminación de `last_error`, nuevo manejo de errores, corrección de la evacuación).

---

# Bitácora Oficial – Sesión del 6 de septiembre de 2026

**Participante:** Josue Linares, Alejandro Toro

**Proyecto:** Simulador de Ascensor Concurrente (Sistemas Operativos)

**Objetivo:** Integración final, depuración y ajustes de interfaz

---

## 1. Contexto de la sesión

Tras recibir los módulos de mis compañeros (`persona.c`, `colas.c`) y tener listos los míos (`shared.c`, `elevator.c`), el objetivo del día fue integrar todo el sistema, compilarlo, probarlo y corregir los errores que surgieran.

Además, se realizaron ajustes de interfaz en el monitor ncurses para mejorar la legibilidad y la experiencia visual.

---

## 2. Actividades realizadas

### 2.1. Integración de módulos de compañeros

* Recibí y revisé `persona.c/h` y `colas.c/h`.
* Verifiqué compatibilidad con `shared` y `elevator`.
* Detecté que `persona.c` usa `shared_is_terminando`, por lo que la añadí a `shared.c/h` para no modificar el código de mis compañeros.

### 2.2. Creación de módulos faltantes

* Redacté `main.c` y `signal_handler.c` siguiendo el diseño acordado.
* Ajusté `shared.c` para incluir `shared_is_terminando` y eliminé `shared_set_hay_llamadas` (redundante y peligrosa).

### 2.3. Compilación y depuración inicial

**Primer error:**

```text
undefined reference to shared_is_terminando
```

**Solución:** Añadí la función en `shared.c` y su prototipo en `shared.h`.

Compilación exitosa con el Makefile.

### 2.4. Pruebas funcionales

* Ejecución con `./bin/ascensor 5`.
* Creación de personas con `kill -USR1 <PID>`.
* Verificación de movimiento del ascensor y salida de personas.
* Terminación con `Ctrl+C`: evacuación completa y apagado ordenado.

---

# 3. Ajustes de interfaz — Monitor `ncurses`

| Ajuste                | Descripción                                                                      |
| --------------------- | -------------------------------------------------------------------------------- |
| **Colores**           | Cambié `CP_TENUE` de azul a blanco para mejorar legibilidad.                     |
| **Columnas**          | Reduje ancho de SUBEN y BAJAN de 16 a 10 caracteres.                             |
| **Alineación**        | Ajusté posiciones de columnas (`COL_SUBEN`, `COL_BAJAN`, `COL_CABINA`).          |
| **Ayuda**             | Cambié el comando mostrado de SIGUSR1 a USR1 por brevedad.                       |
| **Líneas divisorias** | Probé y descarté líneas entre pisos (no se ajustaban bien).                      |
| **Truncamiento**      | Modifiqué `formatear_cola` para truncar a 9 caracteres y evitar desbordamientos. |
| **Limpieza de línea** | Añadí `clrtoeol()` en `dibujar_pie` para evitar sobrescritura de texto.          |

---

# 4. Corrección de lógica en `persona.c`

## Problema detectado

Una persona que llegaba a PB con `jornada_restante > 0` no salía del edificio.

Esto ocurría porque el Paso 4 exigía `jornada_restante <= 0`.

## Solución aplicada

Modifiqué la condición del Paso 4 para eliminar el chequeo de jornada:

```c
if (p->piso_actual == PISO_PB && p->destino == PISO_PB) {
    // salir del edificio
}
```

PB es ahora la salida del edificio, sin importar la jornada restante.

---

# 5. Depuración de errores críticos

## 5.1. Violación de segmento durante evacuación

**Causa:** El Punto 1 de `persona_run` liberaba a una persona que aún estaba en la cola de subida de PB.

**Solución:** Eliminé el primer `if` de `persona_run` (el que verificaba `shared_is_terminando` y PB). Dejé que el flujo normal (Paso 4) maneje la salida.

## 5.2. Ascensor en piso -11

**Causa:** El ascensor se movía por debajo del piso 0.

**Solución:** Añadí verificación de límites en el movimiento del ascensor:

```c
if (dir == ASCENSOR_SUBIENDO &&
    e->ascensor.piso_actual < e->num_pisos - 1)
    e->ascensor.piso_actual += dir;

else if (dir == ASCENSOR_BAJANDO &&
         e->ascensor.piso_actual > 0)
    e->ascensor.piso_actual += dir;
```

---

# 6. Estado final del sistema

| Módulo               | Estado                                   |
| -------------------- | ---------------------------------------- |
| `shared.c/h`         | ✅ Funcional y sincronizado               |
| `elevator.c/h`       | ✅ Funcional, con verificación de límites |
| `persona.c/h`        | ✅ Funcional, con salida inmediata en PB  |
| `colas.c/h`          | ✅ Funcional                              |
| `signal_handler.c/h` | ✅ Funcional                              |
| `monitor.c/h`        | ✅ Funcional, con mejoras visuales        |
| `main.c`             | ✅ Funcional                              |

---

# 7. Pruebas realizadas

**Compilación:** `make clean && make` — Exitosa.

**Ejecución inicial:** `./bin/ascensor 5` — Interfaz ncurses mostrada correctamente.

**Creación de personas:** `kill -USR1 <PID>` — La persona aparece en PB y el ascensor la recoge.

**Múltiples personas:** Envié varias señales; el ascensor las va atendiendo y el monitor actualiza correctamente.

**Apagado:** `Ctrl+C` — El ascensor evacúa a todas las personas y el monitor muestra `"EVACUANDO EDIFICIO"` hasta que el edificio queda vacío, luego termina sin errores.

---

# 8. Comandos útiles utilizados

```bash
# Compilar
make clean && make

# Ejecutar con 5 pisos
./bin/ascensor 5

# Crear una persona (desde otra terminal)
kill -USR1 $(pgrep ascensor)

# Terminar el simulador (Ctrl+C en la terminal del simulador)
# o desde otra terminal:
kill -SIGINT $(pgrep ascensor)

# Ejecutar con ThreadSanitizer
make tsan

./bin/ascensor 5
```

---

# 9. Decisiones de diseño clave

| Decisión                                     | Justificación                                                                                 |
| -------------------------------------------- | --------------------------------------------------------------------------------------------- |
| **Añadir `shared_is_terminando`**            | Necesaria para `persona.c` y `signal_handler.c`. Evita modificar el código de los compañeros. |
| **Eliminar `shared_set_hay_llamadas`**       | Ya no se usaba; su presencia podía inducir a deadlock.                                        |
| **Salida inmediata en PB**                   | Cualquier persona que llegue a PB debe salir del edificio, sin importar su jornada.           |
| **Verificación de límites en ascensor**      | Evita que el ascensor salga del rango `[0, num_pisos-1]`.                                     |
| **Eliminación del Punto 1 en `persona_run`** | Evita liberar personas que aún están en colas durante el apagado.                             |

---

# 10. Próximos pasos

* Verificar con ThreadSanitizer (`make tsan`) para asegurar que no hay data races.
* Pruebas de estrés con muchas personas (20+) para validar estabilidad.
* Documentación final: Actualizar LaTeX y bitácora con todas las decisiones de hoy.
* Preparar entrega final: Empaquetar código y documentación.

---

**Fin de la bitácora.**

Todas las decisiones y correcciones fueron validadas mediante pruebas funcionales y revisión de código.
