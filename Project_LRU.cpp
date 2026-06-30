/*
 * ============================================================================
 *                          Project_LRU.cpp
 * ============================================================================
 *
 * Алгоритм вытеснения LRU-K для управления кэш-памятью.
 * ============================================================================
 */

#define _CRT_SECURE_NO_WARNINGS

#include "lru_k.h"           /* Подключаем заголовочный файл */
#include "lru_k_test.h"      /* Подключаем заголовочный файл тестов */
#include <stdio.h>
#include <locale.h>

int main(void)
{
    /* Устанавливаем русскую локаль для вывода в консоли */
    setlocale(LC_ALL, "Russian");

    printf("===============================================================\n");
    printf("           АЛГОРИТМ ВЫТЕСНЕНИЯ LRU-K\n");
    printf("===============================================================\n");
    printf("LRU-K (Least Recently Used with K-th reference)\n");
    printf("Учитывает K последних обращений к элементу\n");
    printf("===============================================================\n");

    /* Запускаем все тесты */
    test_cache_create_destroy();
    test_cache_put_get();
    test_cache_eviction();
    test_cache_remove();
    test_cache_clear();
    test_cache_stats();

    /* Запускаем примеры */
    example_web_cache();
    example_database_cache();

    /* Выводим итоги */
    printf("ИТОГИ ТЕСТОВ:\n");
    printf("  УСПЕШНО: %d\n", passed);
    printf("  ПРОВАЛЕНО: %d\n", failed);
    printf("  ВСЕГО: %d\n", passed + failed);

    if (failed == 0) {
        printf("\n ВСЕ ТЕСТЫ ПРОЙДЕНЫ УСПЕШНО!\n");
    }
    else {
        printf("\nЕСТЬ ПРОВАЛЕННЫЕ ТЕСТЫ!\n");
    }
    return 0;
}