/*
 *                          lru_k_test.c (тесты и примеры)
*/

#define _CRT_SECURE_NO_WARNINGS

#include "lru_k.h"           /* Подключаем заголовочный файл */
#include <stdio.h>           /* Для printf, snprintf */
#include <stdlib.h>          /* Для rand, srand */
#include <string.h>          /* Для strcmp */
#include <time.h>            /* Для time */

/* Устанавливаем русскую локаль для вывода в консоли */
#ifdef _WIN32
#include <locale.h>          /* Для setlocale в Windows */
#else
#include <locale.h>          /* Для setlocale в Linux/macOS */
#endif