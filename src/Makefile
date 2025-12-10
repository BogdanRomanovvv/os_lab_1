# Компилятор и флаги компиляции
CC = gcc                                      # Используем компилятор GCC
CFLAGS = -std=gnu11 -Wall -Wextra -O2         # Стандарт GNU C11, все предупреждения, оптимизация O2
TARGETS = parent child1 child2                # Список исполняемых файлов для сборки

# Цель по умолчанию: собрать все программы
all: $(TARGETS)

# Правило сборки родительского процесса
parent: parent.c
	$(CC) $(CFLAGS) -o parent parent.c

# Правило сборки первого дочернего процесса
child1: child1.c
	$(CC) $(CFLAGS) -o child1 child1.c

# Правило сборки второго дочернего процесса
child2: child2.c
	$(CC) $(CFLAGS) -o child2 child2.c

# Очистка: удаляет все сгенерированные файлы
clean:
	rm -f $(TARGETS)                          # Удаляем исполняемые файлы
	find . -maxdepth 1 -type f ! -name '*.c' ! -name 'Makefile' ! -name 'SCHEMA.txt' ! -name '.*' -delete
	# find удаляет все файлы кроме исходников (.c), Makefile, SCHEMA.txt и скрытых файлов
