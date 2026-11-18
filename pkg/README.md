# linux-wallpaperengine-moloko

Сборка linux-wallpaperengine для одной машины: Ryzen 9 7900X, niri на Wayland. Основа — ветка
`main` проекта [Almamu/linux-wallpaperengine](https://github.com/Almamu/linux-wallpaperengine).
Ветка `moloko` добавляет два изменения:

- Обои встают на слой `background` протокола wlr-layer-shell, а не на `bottom`. niri держит фон
  на этом слое: обои видны в обзоре рабочих столов и под размытием окон. Слой меняется флагом
  `--layer` (`background`, `bottom`, `top`, `overlay`).
- Сборка идёт через GCC с `-march=native`, `-O3` и LTO. Флаги CEF больше не перебивают
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
| `_lto` | `yes`, `no` | `yes` | LTO через GCC. |
| `_o3` | `yes`, `no` | `yes` | Сборка с `-O3`, иначе с `-O2`. |

Неверное значение останавливает сборку в начале.

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

Если оболочка (например, Noctalia) сама рисует обои на слое `background`, её обои могут
закрыть обои Wallpaper Engine. Тогда выключите обои в оболочке.
