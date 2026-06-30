// lru_k.cpp(реализация)
#define _CRT_SECURE_NO_WARNINGS

#include "lru_k.h"           /* Подключаем заголовочный файл */
#include <stdio.h>           /* Для printf */
#include <stdlib.h>          /* Для malloc, free, calloc, realloc */
#include <string.h>          /* Для strcpy, strcmp, memcpy */
#include <limits.h>          /* Для INT_MAX */

/*
 * ============================================================================
 *                      ВНУТРЕННИЕ СТРУКТУРЫ
 * ============================================================================
 */

 /*
  * Структура для кучи (минимальная куча)
  * Используется для быстрого нахождения элемента с наименьшим временем K-го обращения
  */
typedef struct {
    CacheItem** items;               /* Массив указателей на элементы */
    int size;                        /* Текущий размер кучи */
    int capacity;                    /* Вместимость кучи */
} MinHeap;

/*
 * Полная структура LRU-K кэша (скрытая реализация)
 */
struct LRUKCache {
    int k;                           /* Параметр K (количество обращений для учёта) */
    int capacity;                    /* Максимальное количество элементов в кэше */
    int size;                        /* Текущее количество элементов в кэше */

    /* Хеш-таблица для быстрого поиска по ключу */
    CacheItem** hash_table;          /* Массив указателей на элементы */
    int hash_size;                   /* Размер хеш-таблицы */

    /* Двусвязный список для LRU-1 порядка */
    CacheItem* lru_head;             /* Голова списка (самый старый) */
    CacheItem* lru_tail;             /* Хвост списка (самый новый) */

    /* Куча для быстрого выбора элемента для вытеснения */
    MinHeap* heap;                   /* Минимальная куча */

    /* Статистика */
    int hits;                        /* Количество попаданий в кэш */
    int misses;                      /* Количество промахов */
};

/*
 * ============================================================================
 *                      ВНУТРЕННИЕ ФУНКЦИИ
 * ============================================================================
 */

 /*
  * Хеш-функция FNV-1a для строковых ключей
  * Используется для индексации в хеш-таблице
  *
  * Параметры:
  *   str        - строка для хеширования
  *   table_size - размер хеш-таблицы
  *
  * Возвращает: индекс в хеш-таблице
  */
static unsigned int
fnv1a_hash(const char* str, int table_size)
{
    unsigned int hash = 2166136261U;   /* Начальное хеш-значение для FNV-1a */
    const unsigned char* ptr = (const unsigned char*)str;  /* Указатель на байты строки */

    while (*ptr) {                     /* Пока не дошли до конца строки */
        hash ^= (unsigned int)*ptr;    /* XOR с текущим байтом */
        hash *= 16777619U;             /* Умножение на простое число */
        ptr++;                         /* Переход к следующему байту */
    }

    return hash % table_size;          /* Возвращаем индекс в таблице */
}

/*
 * Создание узла истории обращения
 * Выделяет память и инициализирует время обращения
 *
 * Параметры:
 *   access_time - время обращения
 *
 * Возвращает: указатель на созданный узел или NULL при ошибке
 */
static AccessNode*
access_node_create(time_t access_time)
{
    AccessNode* node = (AccessNode*)malloc(sizeof(AccessNode));  /* Выделяем память */
    if (!node)                                                  /* Проверка выделения */
        return NULL;

    node->access_time = access_time;   /* Устанавливаем время обращения */
    node->next = NULL;                 /* Следующего пока нет */

    return node;                       /* Возвращаем созданный узел */
}

/*
 * Освобождение истории обращений
 * Рекурсивно освобождает все узлы истории
 *
 * Параметры:
 *   head - указатель на голову списка истории
 */
static void
access_history_free(AccessNode* head)
{
    AccessNode* curr = head;           /* Начинаем с головы списка */
    while (curr) {                     /* Пока есть узлы */
        AccessNode* next = curr->next; /* Сохраняем следующий узел */
        free(curr);                    /* Освобождаем текущий узел */
        curr = next;                   /* Переходим к следующему */
    }
}

/*
 * Добавление обращения в историю элемента
 * Добавляет новое время обращения в начало списка (для быстрой вставки)
 * Если история превышает K, удаляет самые старые записи
 *
 * Параметры:
 *   item        - элемент кэша
 *   access_time - время обращения
 *   k           - параметр K (сколько обращений хранить)
 */
static void
access_history_add(CacheItem* item, time_t access_time, int k)
{
    AccessNode* new_node = access_node_create(access_time);  /* Создаём новый узел */
    if (!new_node)                                           /* Проверка создания */
        return;

    /* Добавляем в начало списка (самые новые в начале) */
    new_node->next = item->access_history;   /* Новый узел указывает на старую голову */
    item->access_history = new_node;         /* Обновляем голову списка */
    item->access_count++;                    /* Увеличиваем счётчик обращений */

    /* Если история превышает K, удаляем самые старые записи */
    int count = 0;                           /* Счётчик узлов */
    AccessNode* curr = item->access_history; /* Начинаем с головы */

    while (curr) {                           /* Считаем количество узлов */
        count++;
        curr = curr->next;
    }

    /* Если больше K, удаляем лишние с конца */
    if (count > k) {
        curr = item->access_history;         /* Начинаем с головы */
        int to_remove = count - k;           /* Сколько узлов нужно удалить */

        /* Находим узел, после которого нужно удалить */
        for (int i = 0; i < count - to_remove - 1; i++) {
            curr = curr->next;
        }

        /* Удаляем все узлы после curr */
        AccessNode* to_delete = curr->next;   /* Узел для удаления */
        curr->next = NULL;                   /* Отсекаем хвост */

        while (to_delete) {                  /* Пока есть узлы для удаления */
            AccessNode* next = to_delete->next; /* Сохраняем следующий */
            free(to_delete);                   /* Освобождаем память */
            to_delete = next;                 /* Переходим к следующему */
        }
    }

    /* Находим время K-го обращения (самое старое из K последних) */
    if (item->access_count >= k) {
        curr = item->access_history;         /* Начинаем с головы */
        for (int i = 0; i < k - 1 && curr; i++) {  /* Идём к K-му узлу */
            curr = curr->next;
        }
        if (curr) {
            item->kth_access_time = curr->access_time;  /* Сохраняем время K-го обращения */
        }
    }
}

/*
 * Создание минимальной кучи
 * Выделяет память и инициализирует структуру кучи
 *
 * Параметры:
 *   capacity - начальная вместимость кучи
 *
 * Возвращает: указатель на кучу или NULL при ошибке
 */
static MinHeap*
heap_create(int capacity)
{
    MinHeap* heap = (MinHeap*)malloc(sizeof(MinHeap));  /* Выделяем память для кучи */
    if (!heap)                                          /* Проверка выделения */
        return NULL;

    heap->items = (CacheItem**)malloc(capacity * sizeof(CacheItem*));  /* Массив элементов */
    if (!heap->items) {                                /* Проверка выделения */
        free(heap);                                    /* Освобождаем кучу */
        return NULL;
    }

    heap->size = 0;                    /* Начальный размер кучи */
    heap->capacity = capacity;         /* Ёмкость кучи */

    return heap;                       /* Возвращаем созданную кучу */
}

/*
 * Освобождение кучи
 *
 * Параметры:
 *   heap - указатель на кучу
 */
static void
heap_destroy(MinHeap* heap)
{
    if (!heap)                         /* Если куча NULL - ничего не делаем */
        return;
    if (heap->items)                   /* Если массив элементов существует */
        free(heap->items);             /* Освобождаем его */
    free(heap);                        /* Освобождаем структуру кучи */
}