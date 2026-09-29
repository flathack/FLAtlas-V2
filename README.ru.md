# FLAtlas

![Windows](https://img.shields.io/badge/Windows-Desktop-0078D4?style=for-the-badge&logo=windows&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-Omarchy%20%2F%20Arch-1793D1?style=for-the-badge&logo=linux&logoColor=white)
![Freelancer](https://img.shields.io/badge/Freelancer-Modding-00A3FF?style=for-the-badge)
![2D и 3D](https://img.shields.io/badge/2D%20%2B%203D-System%20Editor-7B61FF?style=for-the-badge)
![Community](https://img.shields.io/badge/Discord-Community-5865F2?style=for-the-badge&logo=discord&logoColor=white)

[Read in English](README.md) | [Deutsch lesen](README.de.md)

**FLAtlas** — настольный инструмент для Windows и Linux, созданный для сообщества моддинга Freelancer.

Он помогает просматривать, редактировать и понимать данные Freelancer с помощью практичных редакторов, 2D/3D-предпросмотра систем, моддинг-утилит и более безопасных рабочих процессов.

![Предпросмотр FLAtlas](https://github.com/flathack/FLAtlas-V2/releases/download/v0.8.9/PREVIEW.png)

## Скачать

Самый простой способ получить FLAtlas — скачать актуальный релиз на GitHub.

[![Скачать актуальный релиз](https://img.shields.io/badge/Download-Latest%20Release-2EA44F?style=for-the-badge&logo=github&logoColor=white)](https://github.com/flathack/FLAtlas-V2/releases/latest)
[![Открыть на ModDB](https://img.shields.io/badge/View%20on-ModDB-F28C28?style=for-the-badge)](https://www.moddb.com/games/freelancer/downloads/flatlas-v2-windows-2d3d-system-editor-idseditor-trade-data-npc)
[![Присоединиться к Discord](https://img.shields.io/badge/Join-Discord-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/fY9qweRWGn)

Все опубликованные версии, контрольные суммы и заметки к релизам доступны здесь:

[Все релизы GitHub](https://github.com/flathack/FLAtlas-V2/releases)

## Что можно делать

FLAtlas создан для моддеров, которые хотят работать напрямую с данными Freelancer без постоянного переключения между множеством мелких инструментов.

- Редактировать системы, базы, данные вселенной, INI-файлы, IDS-записи, infocards, торговые данные, NPC-данные и ресурсы модов.
- Использовать интерактивный 2D- и 3D-предпросмотр для систем, объектов, зон, баз, docking rings, tradelanes и соединений.
- Проверять и настраивать игровые данные, сохраняя понятный обзор связей между файлами.
- Использовать вспомогательные инструменты для патчей, поиска путей, скриншотов, обновлений и запуска модифицированной установки.

## Включенные инструменты

- **Mod Manager** - управление установками Freelancer, модами и активным контекстом редактирования.
- **Universe Editor** - просмотр и редактирование системных данных и соединений на уровне вселенной.
- **System Editor** - визуальная работа с объектами системы, зонами, базами, lanes, полями, rings и связанными системными данными.
- **File Editor** - просмотр и редактирование INI-файлов Freelancer прямо внутри FLAtlas.
- **Trade Routes** - анализ товарных маршрутов, цен на базах и торговых возможностей.
- **IDS Editor** - редактирование IDS-названий и infocards, используемых ресурсами Freelancer.
- **NPC Editor** - редактирование mbases NPCs, bribes, rumors, outfits, voices и связанных данных населения баз.
- **Faction Editor** - редактирование фракций и данных отношений.
- **News Editor** - редактирование назначений текстов новостей и слухов.
- **3D Model Viewer** - просмотр моделей Freelancer.
- **Equipment Visualizer** - просмотр моделей оружия, снарядов, эффектов и параметров стрельбы.
- **Audio Manager** - просмотр, экспорт, замена и преобразование аудиоресурсов Freelancer.
- **WAV Converter** - пакетное преобразование PCM WAV-файлов в проверенный профиль, совместимый с Freelancer.
- **BINI Converter** - преобразование BINI-кодированных INI-файлов в редактируемые текстовые INI-файлы.
- **Translator Tool** - сканирование IDS-записей и, только после явного согласия, перевод выбранного текста.
- **Plugin Catalog** - просмотр внешних инструментов Freelancer со ссылками на поставщиков и заметками о лицензиях.
- **Field Creator** - создание шаблонов астероидных и туманных полей с размещенными объектами.

## Первые шаги

1. Скачайте ZIP для Windows или Linux со страницы релиза.
2. Распакуйте ZIP в удобную папку.
3. Запустите `FLAtlas.exe` в Windows или `./run-flatlas.sh` в Linux.
4. Выберите установку Freelancer или установку мода.
5. Откройте нужный редактор или инструмент и начните изучать данные.

FLAtlas не заменяет установку Freelancer. Он работает с уже существующей папкой игры или мода.

## Системные требования

- **Операционная система:** Windows 10 или новее либо Omarchy/Arch Linux
- **Архитектура:** x64
- **Игровые данные:** установка Freelancer или мода
- **Runtime:** ZIP для Windows содержит Qt 6.8.3 и MinGW; ZIP для Linux использует системные пакеты `qt6-base`, `qt6-3d`, `qt6-multimedia` и `qt6-multimedia-ffmpeg`
- **Резервные копии:** делайте резервные копии важных файлов мода перед крупными изменениями

Windows 7, Windows 8/8.1 и 32-битная Windows не поддерживаются. Для Qt 6.8.3 рекомендуется Windows 10 версии 1809 или новее. В Linux установите `wine` для запуска Freelancer и инструментов Windows, а также `unzip` для обновлений ZIP. Linux ZIP собран для Omarchy/Arch и не является автономным пакетом для других дистрибутивов.

## Помощь и сообщество

Вопросы, сообщения об ошибках, идеи, отзывы и обсуждение разработки приветствуются в Discord:

[![Присоединиться к Discord FLAtlas](https://img.shields.io/badge/FLAtlas-Discord%20Community-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/fY9qweRWGn)

Если что-то работает не так, как ожидалось, укажите, что именно вы редактировали, на что нажали и какую папку Freelancer или мода использовали. Так проблему намного легче воспроизвести.

## Примечания

Этот репозиторий GitHub является публичным центром загрузки и релизов для пользователей FLAtlas.

Freelancer — игра Digital Anvil и Microsoft. FLAtlas является инструментом сообщества для моддинга и не связан с Microsoft и не одобрен Microsoft.
