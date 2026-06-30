
/* lru_k.h (заголовочный файл) */

#ifndef LRU_K_H
#define LRU_K_H

#include <stdbool.h>     /* Для типа bool (true/false) */
#include <stddef.h>      /* Для size_t (размер данных) */
#include <time.h>        /* Для time_t (время обращения) */


 /*
  * Узел истории обращений к элементу
  * Хранит время одного обращения к элементу
  */
typedef struct AccessNode {
    time_t access_time;              /* Время обращения (timestamp) */
    struct AccessNode* next;         /* Следующий узел в истории */
} AccessNode;


/*
 * Элемент кэша (кэш-строка)
 * Содержит данные и историю обращений
 */
typedef struct CacheItem {
    char* key;                       /* Ключ элемента (уникальный идентификатор) */
    void* data;                      /* Указатель на данные (может быть любого типа) */
    size_t data_size;                /* Размер данных в байтах */
    AccessNode* access_history;      /* История обращений (список) */
    int access_count;                /* Общее количество обращений */
    time_t kth_access_time;          /* Время K-го обращения (для LRU-K) */
    struct CacheItem* lru_next;      /* Следующий элемент в LRU-списке */
    struct CacheItem* lru_prev;      /* Предыдущий элемент в LRU-списке */
    int heap_index;                  /* Индекс в куче (для быстрого обновления) */
    bool is_valid;                   /* Флаг: валидный ли элемент (не удалён) */
} CacheItem;

/*
 * Основная структура LRU-K кэша (скрытая реализация)
 */
typedef struct LRUKCache LRUKCache;

/*
 *  ФУНКЦИИ УПРАВЛЕНИЯ КЭШЕМ
 */

 /*
  * Создание LRU-K кэша
  * Параметры:
  *   k - количество обращений для учёта (обычно 2)
  *   capacity - максимальное количество элементов в кэше
  * Возвращает: указатель на созданный кэш или NULL при ошибке
  */
LRUKCache* lru_k_cache_create(int k, int capacity);

/*
 * Освобождение LRU-K кэша и всей связанной памяти
 */
void lru_k_cache_destroy(LRUKCache* cache);

/* 
  ОПЕРАЦИИ С КЭШЕМ
 */

 /*
  * Доступ к элементу кэша (чтение)
  * Параметры:
  *   cache - указатель на кэш
  *   key   - ключ элемента
  * Возвращает: указатель на данные или NULL если элемент не найден
  */
void* lru_k_cache_get(LRUKCache* cache, const char* key);

/*
 * Вставка элемента в кэш
 * Параметры:
 *   cache     - указатель на кэш
 *   key       - ключ элемента
 *   data      - указатель на данные
 *   data_size - размер данных в байтах
 * Возвращает: true если успешно, false если ошибка
 */
bool lru_k_cache_put(LRUKCache* cache, const char* key, void* data, size_t data_size);

/*
 * Удаление элемента из кэша
 * Параметры:
 *   cache - указатель на кэш
 *   key   - ключ элемента
 * Возвращает: true если элемент найден и удалён, false если не найден
 */
bool lru_k_cache_remove(LRUKCache* cache, const char* key);

/*
 * Очистка кэша (удаление всех элементов)
 */
void lru_k_cache_clear(LRUKCache* cache);

/*
 *  СТАТИСТИКА И ОТЛАДКА
 */

 /*
  * Получение статистики кэша
  *
  * Параметры:
  *   cache    - указатель на кэш
  *   hits     - указатель для сохранения количества попаданий (может быть NULL)
  *   misses   - указатель для сохранения количества промахов (может быть NULL)
  *   size     - указатель для сохранения текущего размера (может быть NULL)
  *   capacity - указатель для сохранения ёмкости (может быть NULL)
  */
void lru_k_cache_stats(LRUKCache* cache, int* hits, int* misses, int* size, int* capacity);

/*
 * Получение текущего размера кэша
 */
int lru_k_cache_size(LRUKCache* cache);

/*
 * Получение ёмкости кэша
 */
int lru_k_cache_capacity(LRUKCache* cache);

/*
 * Получение параметра K
 */
int lru_k_cache_k(LRUKCache* cache);

/*
 * Печать содержимого кэша (для отладки)
 */
void lru_k_cache_print(LRUKCache* cache);

/*
 * Печать статистики кэша
 */
void lru_k_cache_print_stats(LRUKCache* cache);

#endif