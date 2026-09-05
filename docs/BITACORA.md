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
| 3 | Implementación de `shared.c` y `colas.c` | ⬜ Pendiente |
| 4 | Implementación de `elevator.c` y `persona.c` | ⬜ Pendiente |
| 5 | Implementación de `signal_handler.c` y `main.c` | ⬜ Pendiente |
| 6 | Implementación de `monitor.c` (ncurses) | ⬜ Pendiente |
| 7 | Pruebas de concurrencia (`make tsan`) y entrega | ⬜ Pendiente |

Leyenda: ✅ terminada · 🔄 en curso · ⬜ pendiente

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
