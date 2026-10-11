# ArpGen – etapa 1

Plugin VST3 (instrumento) para Windows hecho con C++17 y JUCE 8. Genera melodías y arpegios, los reproduce con un sintetizador integrado y exporta MIDI editable.

## Qué incluye la etapa 1
- Motor de composición (`Source/Engine`): 11 escalas/modos, 7 progresiones diatónicas, motivos con variación, frases de 4 compases (motivo / variación / motivo / cadencia), acordes en tiempos fuertes, silencios, cromatismo controlado (notas de aproximación por semitono, solo con Complexity alta), swing, síncopa, humanización.
- Dos capas independientes: **Melody** y **Arpeggio** (6 patrones: Up, Down, Up-Down, Outside-In, Skips, Random Walk).
- 3 alternativas (A/B/C) por generación y por capa.
- Bloqueo de notas: clic en una nota del piano roll → naranja = bloqueada. **Regenerate (keep locks)** conserva las bloqueadas y regenera el resto (las notas nuevas que solapan con una bloqueada se descartan).
- Sintetizador (`Source/Audio`): Piano, Pluck, Bass, Lead, Keys, Pad; 8 voces; volumen, filtro, attack, release.
- Reproducción: botón **Play** (interno) o seguimiento del tempo y transporte del host (*Follow host*). El patrón se repite en bucle alineado al compás del host.
- Exportación MIDI (`Source/Midi`): arrastrar `.mid` al Piano Roll, y **Save .mid**. 960 PPQ, notas, duraciones y velocidades reales. Cada capa se exporta por separado.
- El estado (parámetros + patrones generados) se guarda con el proyecto.

## Qué NO incluye todavía (no está simulado en la interfaz)
Estilos musicales (trap, house, jazz…), melodía complementaria, efectos (reverb/delay), edición de notas (solo bloqueo), arrastre de varias capas a la vez, salida MIDI en tiempo real hacia otros plugins.

## Límites técnicos
1. **Arrastrar y soltar:** VST3 no define nada para esto. Se usa un arrastre de archivo del sistema (`.mid` temporal). Debería funcionar en el Piano Roll y el Channel Rack de FL Studio, pero no he podido probarlo en FL. Si no funciona: **Save .mid** y arrastra el archivo desde el Explorador.
2. **MIDI hacia Serum/Kontakt:** FL Studio no enruta de forma fiable la salida MIDI de un VST3 en vivo. Solución: el MIDI exportado, que sí funciona en cualquier instrumento.
3. Si el host no informa del tempo, usa **Manual BPM** (desactiva *Follow host*).
4. Los acordes se construyen apilando terceras dentro de la escala; en las pentatónicas suenan más abiertos.

## Requisitos (Windows 10/11, x64)
1. **Visual Studio 2022** (Community) con la carga de trabajo *Desarrollo para el escritorio con C++*.
2. **CMake 3.22 o superior** (https://cmake.org, marca "Add to PATH").
3. **Git** (https://git-scm.com).
4. Conexión a internet en la primera configuración: CMake descarga JUCE 8.0.6.

## Compilar
En *x64 Native Tools Command Prompt for VS 2022* o PowerShell, dentro de la carpeta del proyecto:

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

El resultado queda en `build\ArpGen_artefacts\Release\VST3\ArpGen.vst3`.

## Instalar y cargar en FL Studio
1. Copia la carpeta `ArpGen.vst3` a `C:\Program Files\Common Files\VST3\` (requiere permisos de administrador).
2. FL Studio → *Options → Manage plugins → Find installed plugins* (activa *Verify plugins* si lo pide).
3. Añade el plugin como generador: *Channel rack → + → ArpGen* (aparece en la lista de generadores VST3).
4. Pulsa **Play** en el plugin o el Play de FL con *Follow host* activado. Para llevar el MIDI: arrastra "Drag Melody MIDI" o "Drag Arp MIDI" al Piano Roll de otro canal (por ejemplo, Serum).

## Licencia
JUCE se distribuye bajo AGPLv3 o licencia comercial; revísalo antes de distribuir el plugin.

## Estructura
```
Source/Engine   composición (sin dependencias de JUCE)
Source/Audio    sintetizador
Source/Midi     exportación .mid
Source/PluginProcessor.*  parámetros, transporte, estado
Source/PluginEditor.*     interfaz
```
