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

/*
 * Сравнение двух элементов для кучи
 * Сравниваем по времени K-го обращения
 *
 * Параметры:
 *   a - первый элемент
 *   b - второй элемент
 *
 * Возвращает: 1 если a > b, -1 если a < b, 0 если равны
 */
static int
heap_compare(CacheItem* a, CacheItem* b)
{
    if (a->kth_access_time > b->kth_access_time)   /* Если время a больше */
        return 1;
    if (a->kth_access_time < b->kth_access_time)   /* Если время a меньше */
        return -1;
    return 0;                                      /* Времена равны */
}

/*
 * Обмен двух элементов в куче
 * Меняет местами два элемента и обновляет их индексы
 *
 * Параметры:
 *   heap - указатель на кучу
 *   i    - индекс первого элемента
 *   j    - индекс второго элемента
 */
static void
heap_swap(MinHeap* heap, int i, int j)
{
    CacheItem* temp = heap->items[i];  /* Сохраняем первый элемент */
    heap->items[i] = heap->items[j];   /* Заменяем первый на второй */
    heap->items[j] = temp;             /* Заменяем второй на сохранённый */

    /* Обновляем индексы в элементах */
    if (heap->items[i])
        heap->items[i]->heap_index = i;
    if (heap->items[j])
        heap->items[j]->heap_index = j;
}

/*
 * Просеивание вверх (heapify up)
 * Используется после вставки нового элемента для восстановления свойств кучи
 *
 * Параметры:
 *   heap  - указатель на кучу
 *   index - индекс элемента для просеивания
 */
static void
heap_heapify_up(MinHeap* heap, int index)
{
    while (index > 0) {                /* Пока не дошли до корня */
        int parent = (index - 1) / 2;  /* Индекс родительского узла */

        /* Если родитель меньше или равен текущему — всё правильно */
        if (heap_compare(heap->items[parent], heap->items[index]) <= 0)
            break;

        /* Иначе меняем местами */
        heap_swap(heap, parent, index);
        index = parent;                /* Поднимаемся вверх */
    }
}

/*
 * Просеивание вниз (heapify down)
 * Используется после удаления корня или изменения приоритета
 *
 * Параметры:
 *   heap  - указатель на кучу
 *   index - индекс элемента для просеивания
 */
static void
heap_heapify_down(MinHeap* heap, int index)
{
    int smallest = index;              /* Индекс наименьшего элемента */
    int left = 2 * index + 1;          /* Индекс левого потомка */
    int right = 2 * index + 2;         /* Индекс правого потомка */

    /* Находим наименьший среди текущего, левого и правого */
    if (left < heap->size && heap_compare(heap->items[left], heap->items[smallest]) < 0)
        smallest = left;
    if (right < heap->size && heap_compare(heap->items[right], heap->items[smallest]) < 0)
        smallest = right;

    /* Если наименьший не текущий — меняем и продолжаем */
    if (smallest != index) {
        heap_swap(heap, index, smallest);
        heap_heapify_down(heap, smallest);
    }
}

/*
 * Вставка элемента в кучу
 * Добавляет элемент и восстанавливает свойства кучи
 *
 * Параметры:
 *   heap - указатель на кучу
 *   item - элемент для вставки
 */
static void
heap_insert(MinHeap* heap, CacheItem* item)
{
    /* Проверяем, нужно ли увеличить ёмкость кучи */
    if (heap->size >= heap->capacity) {
        heap->capacity *= 2;           /* Удваиваем ёмкость */
        heap->items = (CacheItem**)realloc(heap->items, heap->capacity * sizeof(CacheItem*));
        if (!heap->items)              /* Проверка выделения */
            return;
    }

    heap->items[heap->size] = item;    /* Добавляем элемент в конец */
    item->heap_index = heap->size;     /* Сохраняем индекс элемента */
    heap->size++;                      /* Увеличиваем размер кучи */

    heap_heapify_up(heap, heap->size - 1);  /* Восстанавливаем свойства кучи */
}

/*
 * Извлечение минимального элемента из кучи
 * Удаляет корень и восстанавливает свойства кучи
 *
 * Параметры:
 *   heap - указатель на кучу
 *
 * Возвращает: указатель на минимальный элемент или NULL
 */
static CacheItem*
heap_extract_min(MinHeap* heap)
{
    if (heap->size == 0)               /* Если куча пуста */
        return NULL;

    CacheItem* min_item = heap->items[0];  /* Сохраняем минимальный элемент */

    /* Перемещаем последний элемент в корень */
    heap->items[0] = heap->items[heap->size - 1];
    heap->items[0]->heap_index = 0;
    heap->size--;                      /* Уменьшаем размер кучи */

    heap_heapify_down(heap, 0);        /* Восстанавливаем свойства кучи */

    return min_item;                   /* Возвращаем минимальный элемент */
}

/*
 * Обновление позиции элемента в куче
 * Используется когда меняется время K-го обращения
 *
 * Параметры:
 *   heap - указатель на кучу
 *   item - элемент для обновления
 */
static void
heap_update(MinHeap* heap, CacheItem* item)
{
    int index = item->heap_index;      /* Получаем индекс элемента */
    heap_heapify_down(heap, index);    /* Сначала просеиваем вниз */
    heap_heapify_up(heap, index);      /* Затем просеиваем вверх */
}

/*
 * Удаление элемента из кучи
 * Удаляет произвольный элемент и восстанавливает свойства кучи
 *
 * Параметры:
 *   heap - указатель на кучу
 *   item - элемент для удаления
 */
static void
heap_remove(MinHeap* heap, CacheItem* item)
{
    int index = item->heap_index;      /* Получаем индекс элемента */

    /* Перемещаем последний элемент на место удаляемого */
    if (index < heap->size - 1) {
        heap->items[index] = heap->items[heap->size - 1];
        heap->items[index]->heap_index = index;
        heap->size--;                  /* Уменьшаем размер кучи */
        /* Восстанавливаем свойства кучи */
        heap_heapify_down(heap, index);
        heap_heapify_up(heap, index);
    }
    else {
        heap->size--;                  /* Просто уменьшаем размер */
    }

    item->heap_index = -1;             /* Помечаем, что элемент не в куче */
}

/*
 * Перемещение элемента в конец LRU-списка (сделать самым новым)
 *
 * Параметры:
 *   cache - указатель на кэш
 *   item  - элемент для перемещения
 */
static void
lru_move_to_tail(struct LRUKCache* cache, CacheItem* item)
{
    /* Если элемент уже в хвосте, ничего не делаем */
    if (item == cache->lru_tail)
        return;

    /* Удаляем из текущего положения */
    if (item->lru_prev)                          /* Если есть предыдущий */
        item->lru_prev->lru_next = item->lru_next;  /* Перекидываем указатель */
    else                                         /* Если элемент в голове */
        cache->lru_head = item->lru_next;        /* Обновляем голову */

    if (item->lru_next)                          /* Если есть следующий */
        item->lru_next->lru_prev = item->lru_prev;  /* Перекидываем указатель */

    /* Вставляем в конец (в хвост) */
    item->lru_prev = cache->lru_tail;            /* Предыдущий — бывший хвост */
    item->lru_next = NULL;                       /* Следующего нет */

    if (cache->lru_tail)                         /* Если был хвост */
        cache->lru_tail->lru_next = item;        /* Бывший хвост указывает на нас */

    cache->lru_tail = item;                      /* Становимся новым хвостом */

    if (!cache->lru_head)                        /* Если голова была пуста */
        cache->lru_head = item;                  /* Становимся головой */
}

/*
 * Добавление нового элемента в LRU-список (в конец)
 *
 * Параметры:
 *   cache - указатель на кэш
 *   item  - элемент для добавления
 */
static void
lru_add_to_tail(struct LRUKCache* cache, CacheItem* item)
{
    item->lru_prev = cache->lru_tail;            /* Предыдущий — бывший хвост */
    item->lru_next = NULL;                       /* Следующего нет */

    if (cache->lru_tail)                         /* Если был хвост */
        cache->lru_tail->lru_next = item;        /* Бывший хвост указывает на нас */

    cache->lru_tail = item;                      /* Становимся новым хвостом */

    if (!cache->lru_head)                        /* Если голова была пуста */
        cache->lru_head = item;                  /* Становимся головой */
}