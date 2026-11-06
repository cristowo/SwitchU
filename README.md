<!-- Bloque compartido: es el mismo en cristowo/SwitchU y cristowo/Switify. -->
# SwitchU + Switify (fork de cristowo)

Dos forks que funcionan juntos en la Switch:

| Repositorio | Fork de | Rama | Qué es |
| --- | --- | --- | --- |
| [cristowo/SwitchU](https://github.com/cristowo/SwitchU) | [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU) | `switify-integration` | Menú HOME estilo Wii U, con Spotify integrado |
| [cristowo/Switify](https://github.com/cristowo/Switify) | [DomazinUS/Switify](https://github.com/DomazinUS/Switify) | `switchu` | Servicio en segundo plano de Switify, ajustado para SwitchU |

## Capturas

| HOME | Spotify en los Ajustes rápidos |
| --- | --- |
| ![HOME con el tema Luciérnagas](https://raw.githubusercontent.com/cristowo/SwitchU/switify-integration/screenshots/fork/home.jpg) | ![Ajustes rápidos con Spotify](https://raw.githubusercontent.com/cristowo/SwitchU/switify-integration/screenshots/fork/quick-settings-spotify.jpg) |
| **Una playlist de Spotify** | **Pantalla de bloqueo con Spotify** |
| ![Explorando una playlist de Spotify](https://raw.githubusercontent.com/cristowo/SwitchU/switify-integration/screenshots/fork/spotify-playlist.jpg) | ![Pantalla de bloqueo con barras de espectro](https://raw.githubusercontent.com/cristowo/SwitchU/switify-integration/screenshots/fork/lock-screen-spotify.jpg) |

## Créditos

- **SwitchU**: [PoloNX](https://github.com/PoloNX) y sus colaboradores (GPL-3.0).
  [uLaunch](https://github.com/Xortroll/uLaunch), de [Xortroll](https://github.com/Xortroll),
  inspiró el proyecto.
- **HomeNX**: la pantalla de bloqueo y su contraseña, el zoom al salir de un juego,
  la subida de texturas sin bloqueo, los nombres de juego personalizados, el ajuste
  de efectos de cristal y la temperatura del chip vienen de
  [HomeNX](https://github.com/ad3333m/homenx), de ad3333m (GPL-3.0). Su pantalla de
  bloqueo es de PowerCmptr.
- **Switify**: HiroshiYamauchi ([DomazinUS/Switify](https://github.com/DomazinUS/Switify)).
  La app es GPL-3.0-or-later y el overlay GPL-2.0-only. Usa
  [cspot](https://github.com/feelfreelinux/cspot) y bell, de feelfreelinux y
  colaboradores; el resto de dependencias está en `THIRD_PARTY_NOTICES.md` de Switify.
- **Cambios de estos forks**: cristowo, desarrollados con Claude Code (Anthropic).

Todo el trabajo original pertenece a sus autores. Estos forks mantienen sus licencias.

## Qué cambia respecto a los originales

### SwitchU

- **Música unificada**: selector Canciones / Spotify en los Ajustes rápidos (L).
  Spotify se controla y se explora (cola, playlist, biblioteca, inicio, búsqueda)
  a través del servicio de Switify. Suena una sola fuente a la vez.
- **Widgets de música**: "Reproduciendo" en 1x1 (la portada ocupa todo el
  recuadro, con las barras encima), 2x1 y 2x2 (portada, canción, barras y
  progreso). Además, "Playlists de Spotify": muestra una playlist de tu
  biblioteca; con Más pasas a la siguiente o la anterior, y con A suena ahí
  mismo, sin abrir los Ajustes rápidos.
- **Pantalla de bloqueo** al despertar, con contraseña opcional, controles de música
  y barras de espectro. Muestra "Reconectando..." mientras Spotify vuelve. Con
  "Carátula en la pantalla de bloqueo", la funda del álbum ocupa el centro con un
  disco que asoma y gira en los dos colores principales del álbum, la hora pasa
  arriba a la izquierda y las barras van debajo. La música, la carátula y la
  música con la pantalla apagada se eligen por separado para las canciones del
  menú y para Spotify.
- **Al encender la consola** suena la música del tema desde su canción de HOME
  ("Wii U Home Menu"), aunque lo último haya sido Spotify.
- **Sin suspensión automática con música** (Ajustes, Suspensión; activada por
  defecto): mientras suena música en HOME o en la pantalla de bloqueo, la consola
  no se suspende sola.
- **Cerrar juego**: pide confirmación y cierra al instante, como el HOME original.
  Antes esperaba hasta 15 s a un juego suspendido.
- **Fiabilidad**:
  - el menú elige su memoria según lo que queda libre;
  - se reconecta a la SD si el sistema le corta la sesión;
  - manda su log en vivo al daemon durante el arranque, así un fallo al iniciar queda registrado;
  - los logs viejos del daemon se borran por fecha.

### Switify (solo el servicio en segundo plano)

El detalle está en `SWITCHU-CHANGES.md` de Switify. En resumen:

- **`GetAudioLevels` (comando 38)**: 32 bandas del audio que suena, para las barras
  de SwitchU. Solo se calculan si alguien las pide.
- **`ReloadCurrent` (comando 39)**: tras suspender la consola, espera a que vuelva la
  sesión de Spotify y retoma la misma canción donde iba, en vez de saltarla.
- **Lo cargado sigue sonando al reconectar**: siempre hay unos 5 s de canción
  descargados por adelantado (10 s en calidad normal). Al despertar suena eso y,
  cuando vuelve la conexión, el mismo stream sigue desde el byte donde quedó: sin
  saltos y, si vuelve a tiempo, sin cortes. Si no puede, la canción se recarga en
  la posición exacta del decodificador. El estado lleva el flag
  `SwitifyStatusFlag_Reloading` (bit 8) mientras tanto.
- **Conexión al CDN más robusta**: una respuesta que no sea el rango pedido cierra
  la conexión y abre otra, en vez de arruinar las siguientes y cambiar de canción.
- **Cambiar rápido de canción ya no lo cierra**: una respuesta de Spotify para una
  canción ya saltada se descarta, en vez de escribir en memoria liberada.
- **Memoria a demanda**: el heap ya no es un bloque fijo de 4 MiB. Se toma en pasos
  de 2 MiB según se necesita, hasta el mismo máximo. El servicio cuenta contra la
  memoria del sistema, y con 4 MiB fijos abrir HOME sobre un juego podía agotarla:
  el menú se quedaba en negro.
- **Cuenta como applet** (`application_type: 2` en `switify-sysmodule.json`): su
  memoria sale del espacio de los applets, junto al menú y el daemon de SwitchU,
  que tiene decenas de MiB libres, y no de la memoria del sistema.
- **Errores del CDN**: un hueco en el audio ya no cambia de canción, y una página de
  error del CDN ya no se decodifica como audio.

## Cómo dependen uno del otro

- **SwitchU funciona sin Switify**, pero solo con su música propia. Para Spotify hace
  falta el servicio de Switify en
  `sdmc:/atmosphere/contents/4200000000535754/exefs.nsp`. También hay que haber
  iniciado sesión una vez con la app de Switify (NRO), porque el servicio reutiliza
  esas credenciales en `sdmc:/config/Switify/`.
- **Con el servicio oficial de Switify** Spotify también funciona, pero:
  - no hay barras de espectro;
  - tras suspender, la canción puede saltar o quedarse en silencio;
  - el heap fijo de 4 MiB puede dejar el sistema sin memoria al abrir HOME sobre un juego.
- **Este fork de Switify solo reemplaza el servicio en segundo plano**. El overlay
  y la app NRO oficiales siguen sirviendo tal cual: el servicio les responde igual
  y ellos ignoran los comandos y el flag nuevos.
- **El protocolo IPC está definido en dos sitios que deben coincidir**:
  `phase2/common/include/switify/ipc_protocol.h` en Switify y
  `projects/menu/src/core/SwitifyProtocol.hpp` en SwitchU. Un comando nuevo se añade
  en ambos.

### Compilar e instalar

- **SwitchU**: con xmake y devkitPro (ver "How to build" más abajo). `xmake install -o sd_out`
  deja en `sd_out/` la estructura de la SD.
- **Servicio de Switify**: con devkitA64 r30 (GCC 16), como el oficial. Desde la raíz de Switify:

  ```
  cmake -S phase2/sysmodule -B phase2/build_engine -G Ninja -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release
  ninja -C phase2/build_engine switify-background-package
  ```

  Copia `phase2/build_engine/switify-background.nsp` como
  `sdmc:/atmosphere/contents/4200000000535754/exefs.nsp`.
- **Reinicia la consola** después de cambiar el daemon de SwitchU o el servicio de Switify.

---

<!-- Fin del bloque compartido. Abajo sigue el README original del proyecto. -->

<div align="center">
<img alt="SwitchU" width="125px" src="https://github.com/user-attachments/assets/5d237f16-684b-46ee-a2e8-9a1c71455c5b" />

# SwitchU
### Wii U/3DS style custom home menu for Nintendo Switch

</div>

<p align="center">
  <a href="https://github.com/PoloNX/SwitchU/releases/latest"><img src="https://img.shields.io/github/v/release/PoloNX/SwitchU" alt="Latest release"></a>
  <a href="https://github.com/PoloNX/SwitchU/releases"><img src="https://img.shields.io/github/downloads/PoloNX/SwitchU/total" alt="Downloads"></a>
  <a href="https://github.com/PoloNX/SwitchU/blob/master/LICENSE"><img src="https://img.shields.io/github/license/PoloNX/SwitchU" alt="License"></a>
  <a href="https://github.com/PoloNX/SwitchU/actions"><img src="https://img.shields.io/github/actions/workflow/status/PoloNX/SwitchU/switch.yml?branch=master" alt="Build"></a><br>
  <a href="https://polonx.github.io/SwitchU-Themes/"><img src="https://img.shields.io/badge/Theme%20Studio-polonx.github.io-6364EE" alt="Theme Studio"></a>
</p>

---

<p align="center">
  <a href="#screenshots">Screenshots</a> &nbsp;&middot;&nbsp;
  <a href="#-features">Features</a> &nbsp;&middot;&nbsp;
  <a href="#-installation">Installation</a> &nbsp;&middot;&nbsp;
  <a href="#-themes--icon-workshop">Themes &amp; Icon Workshop</a> &nbsp;&middot;&nbsp;
  <a href="#-switchu-manager">SwitchU Manager</a> &nbsp;&middot;&nbsp;
  <a href="#-how-to-build">How to build</a> &nbsp;&middot;&nbsp;
  <a href="#-known-issues">Known issues</a> &nbsp;&middot;&nbsp;
  <a href="#-contributing">Contributing</a> &nbsp;&middot;&nbsp;
  <a href="#-credits">Credits</a> &nbsp;&middot;&nbsp;
  <a href="#-license">License</a>
</p>

## Screenshots

<img width="1280" height="720" alt="2026110414441100-010000000000100D" src="https://github.com/user-attachments/assets/3d080713-d8f0-467d-bf0e-ba076060d512" />
<img width="1280" height="720" alt="2026110414433200-010000000000100D" src="https://github.com/user-attachments/assets/454a3adb-2b70-4cf2-82cf-af4f1969bfa8" />
<img width="1280" height="720" alt="2026110414440300-010000000000100D" src="https://github.com/user-attachments/assets/6b031e63-5299-429c-bdd2-4d129abb31d3" />

<details>
  <summary><b>More screenshots</b></summary>

Folders in Grid and Single-row views
<img width="1280" height="720" alt="2026110414432400-010000000000100D" src="https://github.com/user-attachments/assets/2c0dbdd5-5379-42a8-87f1-6a92efc7323b" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/d8011e29-11de-429b-95d0-daf7e945262d" />

Hero and Posters displayed for Titles
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/4292389f-a3a1-4bba-93b3-4aaf6aff0218" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/a3f6c8b6-a1a5-48cb-871c-38fb93acb14d" />

Quick Settings Side bar with built in Music player
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/1873f45f-06b9-4f15-863a-e0b300df9d51" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/b0050d53-158c-456a-82f3-b8ef5abb732b" />

Alternate tiles sizes in grid view
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/1b830aca-5f70-4d21-a2fb-e64345537773" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/60530c41-a439-4c15-9f17-d8cbd9de6a0c" />

Themesshop with community submitted themes and icon packs
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/9368fb6e-748c-4ad8-8638-0647841f8718" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/66293182-4cf0-4189-be6c-eae0e2d348fe" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/befe9bd1-ed5b-411a-a434-dc2704a60da3" />

<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/5051b15b-5f5a-4b52-ab5d-eb4858253308" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/d5e3c14e-2b59-49a0-8adb-679cafef4dd6" />
<img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/5c4c67b9-f898-40d4-96dc-504cb0500415" />

</details>

## ✨ Features

| Area | What you get |
|------|----------------|
| **Home menu** | Wii U-inspired browsing, folders, posters / tile sizes, touch + trigger navigation |
| **Theme Shop** | Browse, preview, and install community themes + icon packs (with tint modes) |
| **Theme Studio** | Build and submit themes / icon packs from the web <br> [![Open Theme Studio](https://img.shields.io/badge/Open_Theme_Studio-6364EE?style=for-the-badge)](https://polonx.github.io/SwitchU-Themes/)
| **SteamGridDB** | Covers, heroes, posters - search missing artwork from Settings |
| **Lock Screen** | Clock fonts, transitions, resume from suspended game, Lock from Power / Quick Settings |
| **Music** | Quick Settings player, playlists, fullscreen player - put tracks in `sdmc:/switch/SwitchU/music/` |
| **Battery Saver** | Dial back visuals to save power (also used when Frame Tap viewer needs headroom) |
| **Homebrew suspend** | Homebrew titles can suspend again like retail games |
| **Manager** | Enable / disable SwitchU; in-app updates after you are on 1.4.0+ |

> [!TIP]
> Correct console time/date matters for SteamGridDB HTTPS. Use Settings timezone / NTP sync if artwork search fails.

## 💿 Installation

1. Download the latest **SwitchU** release ZIP from [Releases](https://github.com/PoloNX/SwitchU/releases/latest).
2. Extract it to the **root of your SD card**, merging the `atmosphere/` and `switch/` folders.
3. Eject the SD safely, then **reboot** into Atmosphere.

### Final layout

```text
sdmc:/
├── atmosphere/          ← sysmodule / exosphere pieces from the release
└── switch/
    └── SwitchU/
        ├── SwitchU-Manager.nro
        ├── music/       ← optional local music library
        └── ...
```

## 🎨 Themes & Icon Workshop

Create themes, download packages, and submit icon packs here:

 [![Open Theme Studio](https://img.shields.io/badge/Open_Theme_Studio-6364EE?style=for-the-badge)](https://polonx.github.io/SwitchU-Themes/)

| On the site | On the Switch |
|-------------|----------------|
| Browse the live catalog and download `.zip` themes | Open **Theme Shop** from the HOME menu |
| Live preview editor (colors, backgrounds, fonts, audio) | Apply themes / icon packs from Installed |
| Submit themes or icon packs via GitHub sign-in | Theme Shop pulls the community catalog online |

### Manual theme install

1. Download a theme zip from Theme Studio (or Save draft as zip).
2. Unzip so you have a folder with `theme.json` inside.
3. Copy that folder to:

```text
sdmc:/config/SwitchU/themes/<theme-id>/
```

4. Open **Theme Shop → Installed** and apply it.

> [!NOTE]
> Zips from the site include an `INSTALL.txt` with the same path. Icon packs are different: they install under `sdmc:/switch/SwitchU/icons/`.

Catalog source repo: [PoloNX/SwitchU-Themes](https://github.com/PoloNX/SwitchU-Themes).

## 🧰 SwitchU Manager

`SwitchU-Manager.nro` (under `sdmc:/switch/SwitchU/`) lets you **enable or disable** SwitchU without deleting files.

## 🔨 How to build

### Requirements

- [devkitPro](https://devkitpro.org/wiki/Getting_Started)
- [Xmake](https://xmake.io/#/)

### Clone

```bash
git clone --recursive https://github.com/PoloNX/SwitchU
cd SwitchU
```

### Build (production daemon + external menu mode)

```bash
xmake f -p cross --toolchain=devkita64 --homebrew=n --backend=deko3d
xmake
```

### Build (homebrew .nro mode)

```bash
xmake f -p cross --toolchain=devkita64 --homebrew=y --backend=deko3d
xmake
```

### Clean

```bash
xmake clean
```

Build outputs are generated under `build/cross/aarch64/<mode>/`.

## ⚠️ Known issues

- Some settings are not implemented yet. 
- While sysmodules modules are now supported - some sysmodules or heavy stacks can still cause memory issues that result in failed boots, black screens or other issues like the clock not working. 
- To add more head room for sys-modules you can also try [Born to Honk's Atmosphere fork](https://github.com/borntohonk/Atmosphere/releases/tag/1.12.1+) (when setup correctly it can reclaim more memory on Firmware 20+ and doesn't need sys-patch which can be dropped from the stack) 
- Screenshots in this README may lag the newest UI; see [release notes](https://github.com/PoloNX/SwitchU/releases) for the latest visuals.

## 🤝 Contributing

Bug reports and pull requests are welcome.

- Open an [issue](https://github.com/PoloNX/SwitchU/issues) when something breaks.
- Open a PR when you have a fix or improvement.
- Theme / icon creators: use [Theme Studio](https://polonx.github.io/SwitchU-Themes/) to submit packages.

## 👾 Credits

- [PoloNX](https://github.com/PoloNX) and everyone who ships SwitchU
- [Xortroll](https://github.com/Xortroll) / [uLaunch](https://github.com/Xortroll/uLaunch) for inspiration and help early on
- Theme and icon contributors via [SwitchU-Themes](https://github.com/PoloNX/SwitchU-Themes)
- Community testers and PR authors across the release history

- Lock screen look and password, the zoom out of a game, non-blocking texture uploads, custom game names, the glass effects switch and the chip temperature reading come from [HomeNX](https://github.com/ad3333m/homenx) by ad3333m (GPL-3.0); its lock screen is by PowerCmptr

## 📄 License

This project is licensed under the GNU General Public License v3.0. See [LICENSE](https://github.com/PoloNX/SwitchU/blob/master/LICENSE) for details.
