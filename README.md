# Приложение отслеживания изменений в файле в реальном времени

## Зависимости
 * Система Linux
 * С++11 совместимый компилятор
 * CMake
 
## Архитектура и фичи
Основной поток запускает два потока, один для отслеживания изменений во 
входном файле, другой для просмотра изменений.

Основной поток устанавливает обработчик сигналов и уникальные значения CPU 
Affinity себе и потокам, после чего ожидает завершения порожденных потоков
и обрабатывает это завершение (останавливает детей и удаляет выходной файл)

Поток отслеживания изменений в файле, читает из файл дескриптора inotify 
пришедшие эвенты для файла и обновляет выходной файл, если произошло 
дополнение в отслеживаемый

Увидеть новые изменения можно из другого потока, в котором запущена утилита
"less" (в режиме слежения Shift+F) который просматривает выходной файл

Логгер записывает запуск и завершение(причину) программы в /var/log/syslog
