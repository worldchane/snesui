# Guía de ALLESIU

Todo lo que se puede hacer en ALLESIU, pantalla por pantalla. Las capturas son de una biblioteca de demostración:
los nombres de los juegos son inventados y las carátulas están difuminadas a propósito.

- [Primera vez](#primera-vez)
- [La biblioteca](#la-biblioteca)
- [Buscar](#buscar)
- [Opciones de cada juego](#opciones-de-cada-juego)
- [Estilos: interfaces, menús y botones](#estilos-interfaces-menús-y-botones)
- [Configuración](#configuración)
- [Jugando](#jugando)
- [Si algo falla](#si-algo-falla)

Dónde va cada archivo (juegos, carátulas, trucos, bezels): **[CARPETAS.md](CARPETAS.md)**. Las BIOS:
**[BIOS.md](BIOS.md)**.

---

## Primera vez

Al abrir ALLESIU por primera vez aparece un **asistente** que explica dónde poner los juegos, las BIOS y las carátulas,
y te deja elegir lo básico: si jugar desde el USB o copiar los juegos a la consola, si bajar carátulas por internet y el
estilo. Todo se puede cambiar después en *Configuración*, y el asistente se puede volver a abrir desde ahí.

| | |
|---|---|
| ![Asistente: bienvenida](capturas/asistente_1.jpg) | ![Asistente: los juegos que encontró en tu USB](capturas/asistente_2.jpg) |
| ![Asistente: jugar desde el USB o copiar a la consola](capturas/asistente_3.jpg) | ![Asistente: cómo ordenar las carpetas (avisa si tu USB ya está bien)](capturas/asistente_4.jpg) |
| ![Asistente: las BIOS que tienes y las que faltan](capturas/asistente_5.jpg) | |

Si todavía no hay juegos, la biblioteca explica qué hacer:

![Biblioteca vacía](capturas/vacia.jpg)

Cada vez que instalas una versión nueva, ALLESIU muestra sus **novedades**:

![Novedades](capturas/novedades.jpg)

---

## La biblioteca

![Biblioteca](capturas/biblioteca.jpg)

Arriba están las **pestañas**: *Todos*, una por cada consola que tenga juegos, *Recientes*, *Favoritos* y
*Configuración*. Cada tarjeta muestra la carátula, dónde está el juego (el ícono del pendrive es el USB, el del disco
es la consola) y su consola. La estrella marca los favoritos. Abajo, el nombre del juego elegido y su tamaño.

| Botón | En la biblioteca |
|---|---|
| ✕ | Jugar |
| △ | Volver a revisar el USB |
| □ | Agregar o quitar de favoritos |
| OPTIONS | Opciones del juego |
| Panel táctil | Buscar por nombre (○ quita la búsqueda) |
| L1 / R1 | Pestaña anterior / siguiente |
| L2 / R2 | Cambiar la interfaz / la forma del menú |
| ○ | Cambiar el estilo de los botones |
| R3 | Un juego al azar |
| L3 | La canción siguiente (la música se puede apagar en *Configuración*) |
| Stick derecho ← → | Saltar a la letra anterior o siguiente |

| | |
|---|---|
| ![Pestaña de una consola](capturas/pestana_snes.jpg) | ![Favoritos](capturas/favoritos.jpg) |

La pestaña *Recientes* muestra los últimos juegos que abriste, el más nuevo primero:

![Recientes](capturas/recientes.jpg)

---

## Buscar

Con el **panel táctil** se abre un teclado en pantalla. La lista se filtra mientras escribes y arriba a la derecha se
ve cuántos juegos coinciden. El teclado toma la forma y los colores del estilo de botones elegido.

![Buscar](capturas/buscar.jpg)

---

## Opciones de cada juego

**OPTIONS** sobre un juego abre sus opciones:

- **Copiar a la consola** (para jugar sin el USB) o **copiar al USB** (para liberar espacio). Siempre se deja 1 GB
  libre en la consola y cada copia se verifica;
- **Borrar de la consola** (las partidas no se tocan);
- **Elegir la carátula…**: las más parecidas del servidor de RetroArch, para elegir una o quitar la que tiene;
- **Bajar las carátulas que faltan**;
- **Copiar o borrar todos los juegos** de una vez.

![Opciones del juego](capturas/opciones_juego.jpg)

---

## Estilos: interfaces, menús y botones

ALLESIU se puede ver de muchas formas, y se cambia sin entrar a ningún menú: el cambio se guarda solo.

### Interfaces (L2)

21 interfaces: el fondo, los colores y el material de los paneles (vidrio, neón, degradado, papel…). La de fábrica es
**Aero clásico**: cajas de vidrio transparente con un reflejo en los bordes, sobre el paisaje pixel art animado.

![Interfaces](capturas/interfaces.jpg)

### Formas de menú (R2)

12 formas de mostrar la biblioteca: cuadrículas de distintos tamaños, carrusel, escenario y dos listas. **Lista** y
**Lista detallada** muestran la carátula grande y la descripción del juego (de tu `gamelist.xml`, ver
[CARPETAS.md](CARPETAS.md#descripciones-gamelistxml)).

![Formas de menú](capturas/menus.jpg)

### Botones (○)

50 estilos para los botones de las pistas de abajo, la pestaña elegida y el teclado: redondos, píldora, cuadrados,
de vidrio, con brillo, colores clásicos de PlayStation, dorados…

![Estilos de botones](capturas/botones.jpg)

---

## Configuración

La última pestaña. Está ordenada en secciones:

![Configuración](capturas/config.jpg)

**Imagen**

- **Proporción de la imagen:** como la consola original, píxeles cuadrados, píxel exacto o 16:9 estirada.
- **Filtro:** suave, nítido, nítido suavizado, líneas de TV, CRT o **HD** (escalado de bordes al estilo xBR).
- **Imagen por consola:** una proporción y un filtro propios para cada consola (si no, usa la general).
- **Marcos decorativos (bezels):** ver [CARPETAS.md](CARPETAS.md#bezels-marcos-decorativos).

![Imagen por consola](capturas/consolas.jpg)

**Juego**

- **Avance rápido (mantener R2)** y **Rebobinar (mantener L2)**.
- **Continuar donde quedé:** al salir de un juego se guarda su estado y, al volver a abrirlo, sigue en ese punto.
- **Menos retraso de los controles** (run-ahead): el juego reacciona un cuadro antes al mando. Usa más procesador.
- **Velocidad del Super FX:** acelera los juegos de SNES con ese chip, que en la consola original iban lentos.

**Biblioteca**

- **Música en la biblioteca:** diez canciones originales de ALLESIU, sintetizadas por el propio programa.
- **Bajar carátulas automáticamente** cuando hay internet.
- **Copiar juegos nuevos del USB automáticamente** a la consola.

**Almacenamiento**

- **Copiar o borrar todos los juegos.**
- **Respaldar partidas al USB** y **Restaurar el respaldo del USB** (partidas, favoritos, recientes y ajustes, en
  `ALLESIU-respaldo/`).
- **BIOS:** cuáles hay y cuáles faltan (ver [BIOS.md](BIOS.md)).

![Configuración: almacenamiento y sistema](capturas/config_abajo.jpg)

**Sistema**

- **Asistente de inicio:** volver a verlo.
- **Acerca de ALLESIU:** la versión y los créditos de cada emulador.

![Acerca de](capturas/acerca.jpg)

---

## Jugando

| Botón | Jugando |
|---|---|
| ✕ ○ □ △ | Los botones de la consola (en la SNES: B A Y X; en las de dos botones, ✕/□ y ○/△) |
| L1 / R1 | L / R |
| OPTIONS | Start |
| Panel táctil | Select |
| R2 (mantener) | Avance rápido |
| L2 (mantener) | Rebobinar |
| L3 + R3 | Menú de pausa |

### Menú de pausa

- **Guardar estado / Cargar estado:** 4 ranuras por juego, cada una con su miniatura. Con □ una ranura se **bloquea**
  para no sobrescribirla sin querer.
- **Proporción** y **Filtro** solo para ese juego (o **Usar la configuración general**).
- **Trucos:** los del archivo `.cht` del juego, cada uno se activa o desactiva por separado (ver
  [CARPETAS.md](CARPETAS.md#trucos)).
- **Reiniciar** y **Volver a la biblioteca**.

La batería del cartucho (las partidas que el juego guarda solo) se graba automáticamente; en la SNES, en formato
`.srm` compatible con snes9x y bsnes.

### Dos jugadores

Inicia sesión con un segundo usuario de la PS4 en el otro mando: ALLESIU lo detecta solo.

---

## Si algo falla

- **Un juego no abre:** ALLESIU explica el motivo (por ejemplo, qué archivo o BIOS le falta a un juego de arcade) y
  guarda un informe en el USB, en `ALLESIU-diagnostico/`. No incluye datos personales.
- **La app se cerró:** queda anotado dónde en `/data/allesiu/cierre.txt` (y el registro en `ALLESIU-diagnostico/`
  del USB). Si abres un reporte en GitHub, adjunta esos archivos.
- **No aparecen juegos nuevos:** pulsa △ para volver a revisar el USB.
- **Las carátulas no bajan:** revisa que la consola tenga internet; el servidor de RetroArch a veces no responde y
  ALLESIU lo vuelve a intentar en la próxima búsqueda.
