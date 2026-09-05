# Simulador de Ascensor Concurrente

Simulador de un edificio con un ascensor, escrito en **C** con hilos POSIX
(`pthreads`), señales, mutex, variables de condición y una interfaz de monitoreo en
tiempo real con `ncurses`.

Proyecto de la materia **Sistemas Operativos** — Universidad Privada Boliviana (UPB),
Facultad de Ingenierías y Arquitectura.

> **Estado actual:** `elevator` y `monitor` están implementados; `shared`, `colas`,
> `persona`, `signal_handler` y `main` todavía no. **El proyecto no compila
> todavía** — falta el resto de los módulos y aún no hubo una primera compilación.
> El contrato exacto que hay que implementar está en
> [`docs/BITACORA.md`](docs/BITACORA.md).

---

## Qué hace

Cada **persona** que entra al edificio es un hilo independiente con su propia jornada
laboral: sube a un piso, trabaja un rato, elige otro destino, y al agotar su jornada
baja a planta baja y se va. El **ascensor** es otro hilo que las atiende usando el
algoritmo **SCAN** (mientras sube, solo atiende llamadas hacia arriba; cuando se le
acaban, invierte la dirección).

Todo el estado compartido vive detrás de un único mutex, encapsulado en el módulo
`shared`. **No hay variables globales en ninguna parte del proyecto** — es una
restricción explícita del diseño.

### Parámetros del modelo

| Parámetro | Rango | Valor de diseño |
|---|---|---|
| Pisos del edificio (`n`) | 3 ≤ n ≤ X | X = 10 |
| Capacidad del ascensor (`c`) | 0 < c < Y | Y = 8 |

---

## Cómo se controla: señales

El simulador no se maneja por teclado, sino por **señales POSIX** enviadas desde otra
terminal:

| Señal | Qué provoca | Comando |
|---|---|---|
| `SIGUSR1` | Entra una persona nueva al edificio (se crea un hilo). | `kill -SIGUSR1 <pid>` |
| `SIGINT` | Apagado ordenado: se vacía el edificio y luego termina. | `Ctrl+C` |
| `SIGTERM` | Igual que `SIGINT`. | `kill -SIGTERM <pid>` |

Para obtener el PID mientras corre:

```bash
pgrep ascensor
```

Ejemplo: meter 5 personas de golpe.

```bash
for i in $(seq 5); do kill -SIGUSR1 $(pgrep ascensor); sleep 0.3; done
```

> El apagado **no es abrupto**: al recibir `SIGINT` todas las personas fuerzan su
> destino a planta baja, el ascensor las baja, y recién cuando el edificio queda vacío
> el programa termina y libera sus recursos.

---

## Requisitos

- Compilador **GCC** con soporte C11.
- **pthreads** (incluido en glibc).
- **ncurses** (biblioteca de desarrollo).
- **make**.

Instalación de dependencias en Debian/Ubuntu:

```bash
sudo apt install build-essential libncurses-dev
```

En Fedora:

```bash
sudo dnf install gcc make ncurses-devel
```

> **Nota para Windows:** el proyecto usa APIs POSIX (`pthread`, `sigwait`) que no
> existen en Windows nativo. Se compila y ejecuta bajo **WSL** o una máquina virtual
> Linux.

---

## Compilación y ejecución

```bash
make            # compila -> bin/ascensor
make run        # compila y ejecuta
make debug      # compila con -O0 -g3 (para gdb)
make tsan       # compila con ThreadSanitizer (detector de data races)
make clean      # borra build/ y bin/
make help       # lista los objetivos disponibles
```

`make tsan` no es opcional en la práctica: es la forma de verificar que la estrategia
de sincronización del capítulo 6 del diseño realmente se cumple.

---

## Estructura del proyecto

```
ascensorC/
├── Makefile                 # compilación (build/, bin/, tsan, debug)
├── README.md                # este archivo
├── docs/
│   ├── diseno_logico.md     # documento de diseño completo (el contrato)
│   └── BITACORA.md          # registro de avance del equipo
├── src/
│   ├── include/
│   │   ├── shared.h         # mutex, condición, banderas
│   │   ├── elevator.h       # ascensor + colas + SCAN
│   │   ├── signal_handler.h # sigwait
│   │   ├── persona.h        # hilo trabajador
│   │   ├── colas.h          # FIFO lista enlazada
│   │   └── monitor.h        # ncurses
│   ├── main.c               # orquestador
│   ├── shared.c
│   ├── elevator.c
│   ├── signal_handler.c
│   ├── persona.c
│   ├── colas.c
│   └── monitor.c
└── logs/
    └── simulador.log        # generado en ejecución
```

---

## Los siete módulos

| Módulo | Responsabilidad en una línea |
|---|---|
| `main.c` | Orquestador: inicializa, lanza los 3 hilos permanentes y hace `join`. Sin lógica de negocio. |
| `shared.c` | **Único poseedor del mutex.** Condición, banderas y contador de personas. |
| `elevator.c` | Dueño de los datos del dominio: colas por piso, estado del ascensor, algoritmo SCAN. |
| `signal_handler.c` | Hilo que espera señales con `sigwait()` y las traduce a operaciones del dominio. |
| `persona.c` | Rutina del hilo trabajador (efímero, uno por persona). |
| `colas.c` | Cola FIFO con lista enlazada. Sin concurrencia propia. |
| `monitor.c` | Dibuja el estado con `ncurses`. Solo lee snapshots; nunca escribe. |

Las dos reglas que sostienen todo el diseño:

1. **Nadie toca el mutex directamente** — todo pasa por la API de `shared`.
2. **Nunca se retiene el mutex durante una operación bloqueante** (`sleep`, `join`,
   I/O, `ncurses`).

El detalle completo —estructuras de datos, API de cada módulo, flujos de eventos y el
diagrama de arquitectura— está en [`docs/diseno_logico.md`](docs/diseno_logico.md).

---

## Documentación

| Documento | Contenido |
|---|---|
| [`docs/diseno_logico.md`](docs/diseno_logico.md) | Diseño lógico completo: módulos, estructuras, flujos, sincronización y diagrama. **Es el contrato del proyecto.** |
| [`docs/BITACORA.md`](docs/BITACORA.md) | Qué se hizo, cuándo y qué falta. Se actualiza en cada sesión de trabajo. |

---

## Equipo

| Integrante |
|---|
| Josué Gabriel Linares Herrera |
| Edgar Alejandro Toro Delgadillo |
| Jose Sebastian Rodriguez Choque |

**Docente:** Mgr. Luis Marcel Barrero Mendizabal
**Fecha de entrega:** 5 de septiembre de 2026
