



# Диспетчер параллельной обработки файлов

Пример решения задания: диспетчер запускает несколько процессов, каждый процесс через `execve()` вызывает внешний скрипт обработки одного файла. После завершения обработки всех файлов запускается внешний финальный скрипт сборки результата.

## Состав

- `src/dispatcher.c` — диспетчер процессов.
- `scripts/worker.sh` — пофайловая обработка (`input_file`, `output_dir`).
- `scripts/finalize.sh` — финальная сборка (`output_dir`, `result_file`).
- `scripts/worker_txt.sh` — пример пофайловой обработки `.txt`.
- `scripts/finalize_txt.sh` — сборка всех обработанных `.txt` в один файл.

## Сборка

```bash
gcc -O2 -Wall -Wextra -pedantic src/dispatcher.c -o dispatcher
chmod +x scripts/worker.sh scripts/finalize.sh
```

## Запуск

```bash
./dispatcher <input_dir> <worker_script> <final_script> <parallelism> <output_dir> <result_file> [extension]
```

Пример:

```bash
./dispatcher ./input ./scripts/worker.sh ./scripts/finalize.sh 4 ./work ./book.djvu .tiff
```

Где:

- `parallelism` — максимум одновременно работающих процессов.
- `extension` — расширение входных файлов (по умолчанию `.tiff`).

## Примечания по инструментам

- При наличии `gm` (GraphicsMagick) и `cjb2` (djvulibre) `worker.sh` делает преобразование в `.djvu`.
- При наличии `djvm` (djvulibre) `finalize.sh` собирает итоговый многополосный `.djvu`.
- Если эти утилиты отсутствуют, скрипты используют простой fallback для демонстрации конвейера.

## Пример для `.txt`

```bash
chmod +x scripts/worker_txt.sh scripts/finalize_txt.sh
./dispatcher ./text_input ./scripts/worker_txt.sh ./scripts/finalize_txt.sh 2 ./text_work ./all_texts.txt .txt
```
