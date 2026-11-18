# linux-wallpaperengine-moloko

Сборка linux-wallpaperengine для одной машины: Ryzen 9 7900X, niri на Wayland. Основа — ветка
`main` проекта [Almamu/linux-wallpaperengine](https://github.com/Almamu/linux-wallpaperengine).
Ветка `moloko` добавляет три изменения:

- Обои встают на слой `background` протокола wlr-layer-shell, а не на `bottom`. niri держит фон
  на этом слое: обои видны в обзоре рабочих столов и под размытием окон. Слой меняется флагом
  `--layer` (`background`, `bottom`, `top`, `overlay`).
- Параллакс рабочих столов: при переключении рабочего стола обои немного сдвигаются вслед за ним,
  медленнее окон. Флаги описаны в разделе «Параллакс рабочих столов».
- Сборка идёт через clang с `-march=native`, `-O3` и полным LTO, компоновка через lld. Флаги CEF больше не перебивают
  `-march` и уровень оптимизации. Пакет ставит только программу и CEF, без утилит и заголовков
  вложенных библиотек.

Пакет заменяет `linux-wallpaperengine`, `linux-wallpaperengine-git` и
`niri-wallpaperengine-git`. Программа ставится в `/opt/linux-wallpaperengine`, команда
`linux-wallpaperengine` запускает её оттуда.

## Установка через paru

Добавьте репозиторий в `/etc/paru.conf`:

```ini
[linux-wallpaperengine-moloko]
Url = ssh://git@github.com-powersemmi/powersemmi/linux-wallpaperengine.git
Path = pkg
```

Затем:

```sh
paru -Sy --pkgbuilds
paru -S linux-wallpaperengine-moloko
```

Сборка занимает несколько минут. CMake скачивает CEF (около 350 МБ).

Пакет собирает ветку `moloko` с GitHub в момент установки. Чтобы взять новые коммиты ветки,
снова выполните `paru -S linux-wallpaperengine-moloko`.

Сборка из рабочей копии: `cd pkg && makepkg -si`. Она тоже берёт ветку `moloko` с GitHub, а не
локальные файлы: сначала отправьте изменения.

## Флаги сборки

Флаги задаются переменными окружения, например `_lto=no paru -S linux-wallpaperengine-moloko`.

| Флаг | Значения | По умолчанию | Что делает |
|---|---|---|---|
| `_lto` | `full`, `thin`, `no` | `full` | LTO. `thin` работает только с clang. |
| `_o3` | `yes`, `no` | `yes` | Сборка с `-O3`, иначе с `-O2`. |
| `_cc` | `clang`, `gcc` | `clang` | Компилятор. С `gcc` LTO идёт через `-flto=auto` и ld.bfd. |

Неверное значение или сочетание `_cc=gcc _lto=thin` останавливает сборку в начале.

## Запуск на niri

Ресурсы Wallpaper Engine программа находит сама в
`~/.local/share/Steam/steamapps/common/wallpaper_engine`. Обои из мастерской Steam лежат в
`~/.local/share/Steam/steamapps/workshop/content/431960`, имя папки — номер обоев.

```sh
linux-wallpaperengine --screen-root DP-3 2305352822
```

Разные обои на два монитора:

```sh
linux-wallpaperengine --screen-root DP-3 --bg 2305352822 --screen-root DP-2 --bg <номер>
```

`--silent` выключает звук обоев, `--fps 60` поднимает предел кадров (по умолчанию 30).

Чтобы обои оставались на месте в обзоре niri, добавьте правило в конфиг niri:

```kdl
layer-rule {
    match namespace="^linux-wallpaperengine$"
    place-within-backdrop true
}
```

Автозапуск вместе с niri:

```kdl
spawn-at-startup "linux-wallpaperengine" "--silent" "--screen-root" "DP-3" "2305352822"
```

## Параллакс рабочих столов

В niri рабочие столы каждого монитора стоят друг над другом. С флагом `--workspace-parallax niri`
обои следят за активным рабочим столом своего монитора. При переходе на стол ниже картинка
уходит вверх. Весь путь от верхнего стола до нижнего равен доле экрана из
`--workspace-parallax-strength`. Обои двигаются пружиной с параметрами пружины переключения
столов niri, поэтому движение идёт вместе с окнами.

```sh
linux-wallpaperengine --screen-root DP-3 --workspace-parallax niri 2305352822
```

| Флаг | Значения | По умолчанию | Что делает |
|---|---|---|---|
| `--workspace-parallax` | `niri`, `stdin` | выключен | Откуда брать положение стола. `niri` читает сокет из `$NIRI_SOCKET`. `stdin` читает строки `ВЫХОД X Y`, где X и Y от 0 до 1. |
| `--workspace-parallax-strength` | от 0 до 0.25 | 0.04 | Весь путь обоев в долях экрана. |
| `--workspace-parallax-mode` | `auto`, `image`, `layers` | `auto` | `image` чуть увеличивает картинку и сдвигает её целиком. `layers` сдвигает слои сцены по их глубине: дальние слои двигаются меньше ближних. `auto` выбирает `layers` для сцен с параллаксом камеры, иначе `image`. |
| `--workspace-parallax-stiffness` | число | 1000 | Жёсткость пружины. Ставьте как в `workspace-switch` конфига niri. |
| `--workspace-parallax-damping-ratio` | число | 1.0 | Затухание пружины. Ставьте как в `workspace-switch` конфига niri. |
| `--workspace-parallax-log` | — | выключен | Пишет в журнал положение обоев на каждом кадре движения. |

Положение стола считается по его номеру среди столов монитора: первый стол — верх, последний —
низ. В niri в конце всегда есть пустой стол. Когда он появляется или пропадает, обои плавно
переходят на новое место, даже без переключения.

Видео и веб-обои двигаются целиком. Слои двигаются только у сцен с параллаксом камеры. Слой,
у которого глубина вместе с силой параллакса сцены больше единицы, может показать край у
границы экрана.

Если обои стоят на паузе из-за полноэкранного окна, после паузы они сразу встают на место
активного стола.

Если оболочка (например, Noctalia) сама рисует обои на слое `background`, её обои могут
закрыть обои Wallpaper Engine. Тогда выключите обои в оболочке.
