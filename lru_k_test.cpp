/*
 * lru_k_test.c (тесты и примеры)
 */

#define _CRT_SECURE_NO_WARNINGS

#include "lru_k.h"
#include "lru_k_test.h"      /* Добавляем свой заголовок */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <locale.h>
#endif

 /* ============================================================================
  *                      ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
  * ============================================================================ */

  /* Убираем static — делаем доступными из других файлов */
int passed = 0;
int failed = 0;

/* ============================================================================
 *                      ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ============================================================================ */

void test_check(const char* description, int condition)
{
    if (condition) {
        printf("  УСПЕШНО: %s\n", description);
        passed++;
    }
    else {
        printf("  ОШИБКА: %s\n", description);
        failed++;
    }
}

/* ============================================================================
 *                          ТЕСТЫ
 * ============================================================================ */

void test_cache_create_destroy(void)
{
    printf("\n=== Тест 1: Создание и удаление кэша ===\n");

    LRUKCache* cache = lru_k_cache_create(2, 10);
    test_check("Кэш создан (не NULL)", cache != NULL);
    test_check("Кэш пуст (size=0)", lru_k_cache_size(cache) == 0);
    test_check("K=2", lru_k_cache_k(cache) == 2);
    test_check("Ёмкость=10", lru_k_cache_capacity(cache) == 10);

    lru_k_cache_destroy(cache);
    test_check("Кэш удалён без ошибок", 1);
}

void test_cache_put_get(void)
{
    printf("\n=== Тест 2: Вставка и получение ===\n");

    LRUKCache* cache = lru_k_cache_create(2, 5);

    int data1 = 100, data2 = 200, data3 = 300;

    test_check("Вставка key1", lru_k_cache_put(cache, "key1", &data1, sizeof(int)));
    test_check("Вставка key2", lru_k_cache_put(cache, "key2", &data2, sizeof(int)));
    test_check("Вставка key3", lru_k_cache_put(cache, "key3", &data3, sizeof(int)));
    test_check("Размер = 3", lru_k_cache_size(cache) == 3);

    int* val = (int*)lru_k_cache_get(cache, "key1");
    test_check("Получение key1 -> 100", val != NULL && *val == 100);

    val = (int*)lru_k_cache_get(cache, "key2");
    test_check("Получение key2 -> 200", val != NULL && *val == 200);

    val = (int*)lru_k_cache_get(cache, "key3");
    test_check("Получение key3 -> 300", val != NULL && *val == 300);

    val = (int*)lru_k_cache_get(cache, "key4");
    test_check("Получение key4 -> NULL (промах)", val == NULL);

    lru_k_cache_destroy(cache);
}

void test_cache_eviction(void)
{
    printf("\n=== Тест 3: Вытеснение элементов ===\n");

    LRUKCache* cache = lru_k_cache_create(2, 3);
    int data;

    data = 1;
    lru_k_cache_put(cache, "A", &data, sizeof(int));
    data = 2;
    lru_k_cache_put(cache, "B", &data, sizeof(int));
    data = 3;
    lru_k_cache_put(cache, "C", &data, sizeof(int));

    test_check("Кэш заполнен (size=3)", lru_k_cache_size(cache) == 3);

    for (int i = 0; i < 3; i++) {
        lru_k_cache_get(cache, "A");
        lru_k_cache_get(cache, "B");
    }

    data = 4;
    lru_k_cache_put(cache, "D", &data, sizeof(int));

    test_check("C вытеснен", lru_k_cache_get(cache, "C") == NULL);
    test_check("D присутствует", lru_k_cache_get(cache, "D") != NULL);
    test_check("A присутствует", lru_k_cache_get(cache, "A") != NULL);
    test_check("B присутствует", lru_k_cache_get(cache, "B") != NULL);

    lru_k_cache_destroy(cache);
}

void test_cache_remove(void)
{
    printf("\n=== Тест 4: Удаление элементов ===\n");

    LRUKCache* cache = lru_k_cache_create(2, 5);
    int data;

    for (int i = 0; i < 5; i++) {
        data = i;
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        lru_k_cache_put(cache, key, &data, sizeof(int));
    }

    test_check("Размер = 5", lru_k_cache_size(cache) == 5);
    test_check("Удаление key2", lru_k_cache_remove(cache, "key2"));
    test_check("Размер = 4", lru_k_cache_size(cache) == 4);
    test_check("key2 отсутствует", lru_k_cache_get(cache, "key2") == NULL);
    test_check("Удаление несуществующего key9", !lru_k_cache_remove(cache, "key9"));

    lru_k_cache_destroy(cache);
}

void test_cache_clear(void)
{
    printf("\n=== Тест 5: Очистка кэша ===\n");

    LRUKCache* cache = lru_k_cache_create(2, 5);
    int data;

    for (int i = 0; i < 5; i++) {
        data = i;
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        lru_k_cache_put(cache, key, &data, sizeof(int));
    }

    test_check("Размер = 5", lru_k_cache_size(cache) == 5);
    lru_k_cache_clear(cache);
    test_check("Размер = 0", lru_k_cache_size(cache) == 0);
    test_check("key0 отсутствует", lru_k_cache_get(cache, "key0") == NULL);
    test_check("key4 отсутствует", lru_k_cache_get(cache, "key4") == NULL);

    lru_k_cache_destroy(cache);
}

void test_cache_stats(void)
{
    printf("\n=== Тест 6: Статистика кэша ===\n");

    LRUKCache* cache = lru_k_cache_create(2, 10);
    int data;

    for (int i = 0; i < 10; i++) {
        data = i;
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        lru_k_cache_put(cache, key, &data, sizeof(int));
    }

    for (int i = 0; i < 5; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        lru_k_cache_get(cache, key);
    }

    for (int i = 10; i < 15; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        lru_k_cache_get(cache, key);
    }

    int hits, misses, size, capacity;
    lru_k_cache_stats(cache, &hits, &misses, &size, &capacity);

    test_check("Хиты = 5", hits == 5);
    test_check("Промахи = 5", misses == 5);
    test_check("Размер = 10", size == 10);

    lru_k_cache_destroy(cache);
}

/* ============================================================================
 *                          ПРИМЕРЫ ПРИМЕНЕНИЯ
 * ============================================================================ */

void example_web_cache(void)
{
    printf("\n=== Пример: Кэш веб-страниц (LRU-2) ===\n");

    LRUKCache* cache = lru_k_cache_create(2, 5);

    struct {
        const char* url;
        int size;
    } pages[] = {
        {"/index.html", 1024},
        {"/about.html", 512},
        {"/contact.html", 256},
        {"/blog/post1.html", 2048},
        {"/blog/post2.html", 1536},
        {"/blog/post3.html", 3072},
        {"/api/users", 4096},
    };

    printf("Загрузка страниц в кэш:\n");
    for (int i = 0; i < 5; i++) {
        lru_k_cache_put(cache, pages[i].url, &pages[i].size, sizeof(int));
        printf("  Добавлена: %s (размер=%d)\n", pages[i].url, pages[i].size);
    }

    printf("\nДоступ к страницам (создание истории):\n");
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            int* size = (int*)lru_k_cache_get(cache, pages[j].url);
            if (size) {
                printf("  Доступ к %s: размер=%d\n", pages[j].url, *size);
            }
        }
    }

    printf("\nДобавление новой страницы (вытеснение):\n");
    lru_k_cache_put(cache, pages[5].url, &pages[5].size, sizeof(int));

    lru_k_cache_print(cache);
    lru_k_cache_destroy(cache);
}

void example_database_cache(void)
{
    printf("\n=== Пример: Кэш запросов к базе данных ===\n");

    LRUKCache* cache = lru_k_cache_create(2, 4);

    const char* queries[] = {
        "SELECT * FROM users WHERE id=1",
        "SELECT * FROM users WHERE id=2",
        "SELECT * FROM products WHERE id=1",
        "SELECT * FROM users WHERE id=1",
        "SELECT * FROM users WHERE id=2",
        "SELECT * FROM orders WHERE id=1",
        "SELECT * FROM products WHERE id=1",
        "SELECT * FROM users WHERE id=3",
        "SELECT * FROM users WHERE id=1",
        "SELECT * FROM users WHERE id=2",
    };

    int result_size = 1024;
    int hit_count = 0, miss_count = 0;

    printf("Выполнение запросов:\n");
    for (int i = 0; i < sizeof(queries) / sizeof(queries[0]); i++) {
        void* cached = lru_k_cache_get(cache, queries[i]);

        if (cached) {
            hit_count++;
            printf("  ПОПАДАНИЕ: %s\n", queries[i]);
        }
        else {
            miss_count++;
            printf("  ПРОМАХ: %s (загружаем из БД)\n", queries[i]);
            lru_k_cache_put(cache, queries[i], &result_size, sizeof(int));
        }
    }

    printf("\nСтатистика кэша БД:\n");
    printf("  Попаданий: %d\n", hit_count);
    printf("  Промахов: %d\n", miss_count);
    printf("  Hit rate: %.2f%%\n", (float)hit_count / (hit_count + miss_count) * 100);
    printf("  Размер: %d/%d\n", lru_k_cache_size(cache), lru_k_cache_capacity(cache));

    lru_k_cache_print(cache);
    lru_k_cache_destroy(cache);
}