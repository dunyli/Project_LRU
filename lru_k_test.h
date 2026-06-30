/*
 * ============================================================================
 *                          lru_k_test.h
 * ============================================================================
 *
 * Заголовочный файл для тестов LRU-K кэша
 * ============================================================================
 */

#ifndef LRU_K_TEST_H
#define LRU_K_TEST_H

 /* Глобальные переменные для статистики тестов */
extern int passed;
extern int failed;

/* Тесты */
void test_cache_create_destroy(void);
void test_cache_put_get(void);
void test_cache_eviction(void);
void test_cache_remove(void);
void test_cache_clear(void);
void test_cache_stats(void);

/* Примеры применения */
void example_web_cache(void);
void example_database_cache(void);

/* Вспомогательные функции */
void test_check(const char* description, int condition);

#endif /* LRU_K_TEST_H */