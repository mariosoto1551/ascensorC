# Bitácora del Proyecto

Registro de avance del **Simulador de Ascensor Concurrente**.
Se agrega una entrada nueva **arriba de todo** cada vez que se termina un bloque de
trabajo. Lo viejo no se edita ni se borra.

**Formato de cada entrada:**

```
## AAAA-MM-DD — Título corto
Quién:  nombres
Qué se hizo:  lista
Decisiones:   cosas que se eligieron y por qué (si hubo)
Pendiente:    lo que queda abierto
```

---

## Estado general

| Fase | Descripción | Estado |
|---|---|---|
| 1 | Diseño lógico documentado | ✅ Terminada |
| 2 | Estructura de archivos y compilación | ✅ Terminada |
| 3 | Implementación de `elevator.c` (SCAN) | ✅ Terminada — sin compilar |
| 4 | Implementación de `monitor.c` (ncurses) | ✅ Terminada — sin compilar |
| 5 | Implementación de `shared.c` y `colas.c` | ⬜ Pendiente — **bloquea todo lo demás** |
| 6 | Implementación de `persona.c` | ⬜ Pendiente |
| 7 | Implementación de `signal_handler.c` y `main.c` | ⬜ Pendiente |
| 8 | Primera compilación completa (`make`) | ⬜ Pendiente |
| 9 | Pruebas de concurrencia (`make tsan`) y entrega | ⬜ Pendiente |

Leyenda: ✅ terminada · 🔄 en curso · ⬜ pendiente

---

## 2026-09-04 — Lógica de `elevator` y `monitor`

**Quién:** equipo (con asistencia de Claude Code)

> ⚠️ **Este código NO se compiló todavía.** En la máquina donde se escribió no hay
> `gcc` ni `ncurses`, y además falta implementar `shared.c`, `colas.c` y
> `persona.c`, sin los cuales no puede compilar. La revisión fue manual.
> La primera compilación real es la Fase 8.

**Qué se hizo:**

- `elevator.h` / `elevator.c` completos: SCAN, carga/descarga, evacuación
  ordenada y snapshots.
- `monitor.h` / `monitor.c` completos: tablero ncurses con el edificio dibujado
  de arriba hacia abajo, colas por piso, cabina, y pie con el resumen.

---

### Contrato: qué necesitan `elevator.c` y `monitor.c` de los otros módulos

Esto es lo que hay que implementar **exactamente así** para que estos dos archivos
compilen. Quien tome `shared.c`, `colas.c` o `persona.c` debería arrancar de acá.

**`colas.h`** — 6 funciones. Ninguna toca el mutex: el llamante ya lo tiene.

```c
void       cola_init(cola_t *c);
void       cola_destroy(cola_t *c);
void       cola_encolar(cola_t *c, persona_t *p);
persona_t *cola_desencolar(cola_t *c);      /* NULL si está vacía */
int        cola_cantidad(const cola_t *c);
bool       cola_vacia(const cola_t *c);
```

**`persona.h`** — 3 funciones.

```c
persona_t *persona_create(struct elevator *e, int id, int piso_inicial);
void       persona_destroy(persona_t *p);
void      *persona_run(void *arg);          /* arg = persona_t * */
```

`persona_t` necesita **un campo más** de los que lista la sección 4.3: una
referencia al `elevator_t`, porque `pthread_create` solo pasa un puntero y la
persona necesita llamar a la API del ascensor.

**`shared.h`** — la API de la sección 3.2 **más tres funciones nuevas**:

```c
bool shared_is_terminando_locked(const shared_t *s);
int  shared_get_personas_locked(const shared_t *s);
void shared_set_llamadas_locked(shared_t *s, bool valor);
```

Y una regla que hay que respetar al implementar `shared.c`:

| Grupo | Comportamiento |
|---|---|
| `shared_lock`, `shared_unlock`, `shared_wait`, `shared_signal`, `shared_broadcast` | **Crudas.** Envuelven la llamada `pthread_*` y nada más. No toman el mutex por su cuenta. |
| `shared_set_terminando`, `shared_is_terminando`, `shared_inc_personas`, `shared_dec_personas`, `shared_set_llamadas` | **Toman el mutex ellas mismas.** No se pueden llamar con el candado ya tomado. |
| Las tres `_locked` de arriba | **Asumen que el llamante ya tiene el mutex.** |

Sin esta separación hay *deadlock*: `elevator_run` necesita leer `terminando`
mientras tiene el candado, y si `shared_is_terminando()` intentara tomarlo otra
vez el programa se cuelga. (Un mutex recursivo **no** sirve como atajo:
`pthread_cond_wait` sobre un mutex tomado dos veces solo libera un nivel y
también se cuelga.)

---

### Problemas del diseño que aparecieron al implementar

**1. `shared_signal()` es inseguro acá — no se usa en ningún lado.**

La sección 6.2 dice que `shared_signal()` despierta al ascensor cuando alguien se
encola. Pero sobre esa **única** variable de condición esperan cosas distintas: el
ascensor *y* cada persona que viaja. `pthread_cond_signal` despierta a **uno
cualquiera**, así que puede despertar a una persona en vez de al ascensor y el
aviso se pierde: el ascensor se queda dormido con gente esperando.

`elevator.c` usa `shared_broadcast()` en todos los casos. Con 8 o 20 hilos el
costo es irrelevante y es la única opción correcta mientras haya una sola
condición. La alternativa seria es tener dos variables de condición separadas
(una del ascensor, otra de las personas), pero eso cambia `shared_t`.

**2. La terminación, tal como está escrita en la sección 5.3, se cuelga.**

El punto 3 dice que el ascensor "deja de recoger nuevas personas". El punto 4 dice
que `main` espera a que `personas_activas == 0`. Si el ascensor deja de recoger
gente, los que están en las colas nunca bajan, `personas_activas` nunca llega a
cero y el programa no termina nunca.

Se implementó la lectura que sí cierra: **durante el apagado no entra gente nueva
al edificio** (`SIGUSR1` se ignora), pero el ascensor sigue atendiendo a todos
hasta vaciarlo. Es lo que hace que el punto 4 se cumpla.

**3. Había una condición de carrera en `p->destino`.**

El ascensor lee `p->destino` con el mutex tomado (en el SCAN y al descargar). Si
`persona.c` lo escribiera por su cuenta antes de encolarse, sería una escritura
sin candado contra una lectura con candado: *data race*, y ThreadSanitizer lo
marcaría.

Por eso `elevator_encolar_persona()` recibe el destino **como parámetro** y lo
escribe con el candado ya tomado. Los únicos campos que `persona.c` puede tocar
libremente son `jornada_restante` y `tiempo_trabajo`, que nadie más mira.

---

### Decisiones tomadas

- **`MAX_PISOS` y `CAPACIDAD` van en `elevator.h`** (cierra el punto 1 de la
  entrada anterior). No hace falta un `config.h`: los únicos que los usan son
  `ascensor_t` y `Snapshot`, que son de este módulo, y `monitor.c` los recibe al
  incluir el header.

- **Dos funciones nuevas en la API del ascensor**, ambas para `persona.c`:
  `elevator_encolar_persona()` y `elevator_esperar_llegada()`. La sección 3.5
  decía "usa la API de elevator o shared" sin concretar cuál.

- **Protocolo de llegada:** una persona sabe que llegó cuando
  `p->piso_actual == p->destino`. El ascensor escribe `piso_actual` **solo** al
  descargarla. Así no hizo falta agregar un campo `estado` a `persona_t`.

- **El ascensor atiende el piso actual *antes* de decidir a dónde moverse.** Al
  revés, arrancaría en PB, se iría al piso 1 y recién ahí cargaría, dejando
  plantada a la gente de PB.

- **La evacuación la ejecuta el ascensor**, no cada persona. Es la única parte del
  programa donde el ascensor escribe campos de una persona que no sean
  `piso_actual`. Se hace así porque esa gente está guardada *dentro de las colas*,
  que son datos privados del ascensor: `persona.c` no puede sacarlas de ahí sin
  romper el encapsulamiento.

- **Los hilos persona van con `pthread_detach`.** Nadie les hace `join`; la
  sincronización del apagado es el contador `personas_activas`. Cada persona
  libera su propia estructura al salir.

- **El monitor sigue dibujando hasta que el edificio queda vacío**, no hasta que
  se pide el apagado (la sección 3.6 dice lo segundo). Si saliera apenas llega el
  `SIGINT`, la evacuación —que es lo más interesante de mirar— pasaría a ciegas.

- **Todo el dibujo del monitor es ASCII de 7 bits.** El `Makefile` enlaza
  `-lncurses`, no `-lncursesw`: los caracteres de dibujo Unicode saldrían como
  basura en muchas terminales.

- **El monitor usa `cbreak()` y no `raw()`.** `raw()` desactiva `ISIG`, y con eso
  `Ctrl+C` dejaría de generar `SIGINT`, que es justamente como se apaga el
  simulador.

- **Si la salida no es una terminal**, el monitor no inicializa ncurses y
  acompaña la simulación en silencio, en vez de romper contra un archivo o una
  tubería.

**Pendiente:**

- Compilar por primera vez (Fase 8). Nada de esto está verificado por el
  compilador todavía.
- De los 5 huecos de la entrada anterior: el **1** queda cerrado y el **4** queda
  definido por el contrato de arriba. Siguen abiertos el **2** (contexto de
  `signal_handler`), el **3** (quién escribe `logs/simulador.log`) y el **5**
  (cómo se configuran `n` y `c`; por ahora `elevator_create(num_pisos, ...)` lo
  recibe, así que lo decide `main.c`).
- `persona.c` debe revisar `shared_is_terminando()` después de cada rato de
  trabajo, y dormir en tramos cortos: si duerme 10 segundos de un saque, el
  apagado tarda 10 segundos en notarse.

---

## 2026-09-04 — Arquitectura inicial del repositorio

**Quién:** equipo (con asistencia de Claude Code)

**Qué se hizo:**

- Se separó el `README.md`, que hasta ahora contenía el documento de diseño completo.
  Ahora hay dos archivos con propósitos distintos:
  - `README.md` → cómo instalar, compilar, ejecutar y controlar el simulador.
  - `docs/diseno_logico.md` → el documento de diseño, que es el contrato del proyecto.
- Se reconstruyeron las tablas del documento de diseño, que habían quedado rotas al
  convertirse desde el PDF original (el Cuadro 1 era ilegible).
- Se dibujó la **Figura 1** (diagrama de arquitectura), que en el documento original
  aparecía referenciada pero sin imagen.
- Se creó el árbol de archivos completo: `src/`, `src/include/`, `docs/`, `logs/`.
- Se crearon los 7 módulos (`.c` + `.h`). **Cada archivo está vacío de código**: solo
  lleva un comentario de cabecera con su responsabilidad, la API que le toca
  implementar y la sección del diseño donde está especificada.
- Se escribió el `Makefile` con los objetivos `all`, `run`, `debug`, `tsan`, `clean`
  y `help`.
- Se agregó `.gitignore` (ignora `build/`, `bin/`, `*.o`, `logs/*.log`).
- Se creó esta bitácora.

**Decisiones:**

- **El archivo de diseño se llama `diseno_logico.md`, sin la ñ.** El documento
  original lo listaba como `diseño_logico.md`, pero los nombres de archivo con
  acentos dan problemas al mezclar Windows, WSL y Git. El contenido no cambió.
- **Se agregó el objetivo `make tsan`** (ThreadSanitizer). No estaba en el diseño,
  pero es la forma práctica de comprobar que las reglas del capítulo 6 se cumplen de
  verdad y no solo en el papel.
- **El binario se compila en `bin/ascensor`** y los objetos en `build/`, en vez de
  dejar todo suelto en la raíz del repo.
- **Se escribió un `Makefile` funcional, no un archivo vacío.** Es configuración de
  compilación, no código del simulador; si el equipo prefiere escribirlo a mano, se
  puede vaciar sin tocar nada más.

**Pendiente — huecos del diseño que hay que cerrar antes de programar:**

1. **`MAX_PISOS` y `CAPACIDAD`** se usan en las estructuras de las secciones 4.1 y
   4.4, pero el documento no dice en qué header se definen. Hay que decidirlo:
   ¿van en `shared.h`, o se crea un `config.h`?
2. **El contexto de `signal_handler`.** El hilo necesita `shared_t` *y* `elevator_t`,
   pero `pthread_create` solo admite un puntero. La sección 3.4 lo menciona sin
   resolverlo. Hay que definir la estructura de contexto.
3. **`logs/simulador.log`.** Aparece en el árbol de archivos, pero ningún módulo del
   diseño tiene la responsabilidad de escribirlo. Falta decidir quién loguea y qué.
4. **Nombres exactos de la API de `colas.c`.** El diseño describe qué hace la cola
   pero no fija las firmas. Conviene acordarlas antes de que dos personas
   implementen contra nombres distintos.
5. **Cómo se configuran `n` (pisos) y `c` (capacidad)** en tiempo de ejecución:
   ¿argumentos de línea de comandos, constantes de compilación, o entrada por
   teclado al arrancar?

**Qué sigue:** resolver los 5 puntos de arriba y arrancar la Fase 3 (`shared.c` y
`colas.c`), que son los módulos de los que dependen todos los demás.
