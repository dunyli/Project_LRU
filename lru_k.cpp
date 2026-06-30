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

/*
 * Удаление элемента из LRU-списка
 *
 * Параметры:
 *   cache - указатель на кэш
 *   item  - элемент для удаления
 */
static void
lru_remove(struct LRUKCache* cache, CacheItem* item)
{
    if (item->lru_prev)                          /* Если есть предыдущий */
        item->lru_prev->lru_next = item->lru_next;  /* Перекидываем указатель */
    else                                         /* Если элемент в голове */
        cache->lru_head = item->lru_next;        /* Обновляем голову */

    if (item->lru_next)                          /* Если есть следующий */
        item->lru_next->lru_prev = item->lru_prev;  /* Перекидываем указатель */
    else                                         /* Если элемент в хвосте */
        cache->lru_tail = item->lru_prev;        /* Обновляем хвост */

    item->lru_prev = NULL;                       /* Очищаем указатели */
    item->lru_next = NULL;
}

/*
 * Поиск элемента в хеш-таблице по ключу
 * Использует линейное пробирование для разрешения коллизий
 *
 * Параметры:
 *   cache - указатель на кэш
 *   key   - ключ для поиска
 *
 * Возвращает: указатель на найденный элемент или NULL
 */
static CacheItem*
hash_table_find(struct LRUKCache* cache, const char* key)
{
    unsigned int index = fnv1a_hash(key, cache->hash_size);  /* Вычисляем хеш */
    CacheItem* item = cache->hash_table[index];   /* Получаем элемент по индексу */

    while (item) {                               /* Пока есть элемент */
        if (strcmp(item->key, key) == 0)         /* Если ключи совпадают */
            return item;                         /* Возвращаем найденный элемент */
        index = (index + 1) % cache->hash_size;  /* Переходим к следующей ячейке */
        item = cache->hash_table[index];         /* Получаем следующий элемент */
    }

    return NULL;                                 /* Ключ не найден */
}

/*
 * Вставка элемента в хеш-таблицу
 * Использует линейное пробирование для разрешения коллизий
 *
 * Параметры:
 *   cache - указатель на кэш
 *   item  - элемент для вставки
 */
static void
hash_table_insert(struct LRUKCache* cache, CacheItem* item)
{
    unsigned int index = fnv1a_hash(item->key, cache->hash_size);  /* Вычисляем хеш */

    /* Ищем свободное место (линейное пробирование) */
    while (cache->hash_table[index] != NULL) {
        index = (index + 1) % cache->hash_size;
    }

    cache->hash_table[index] = item;             /* Вставляем элемент */
}

/*
 * Удаление элемента из хеш-таблицы
 *
 * Параметры:
 *   cache - указатель на кэш
 *   key   - ключ для удаления
 */
static void
hash_table_remove(struct LRUKCache* cache, const char* key)
{
    unsigned int index = fnv1a_hash(key, cache->hash_size);  /* Вычисляем хеш */

    while (cache->hash_table[index] != NULL) {    /* Пока есть элементы */
        if (strcmp(cache->hash_table[index]->key, key) == 0) {  /* Если ключ совпадает */
            cache->hash_table[index] = NULL;      /* Удаляем элемент */
            return;                              /* Выходим */
        }
        index = (index + 1) % cache->hash_size;   /* Переходим к следующей ячейке */
    }
}

/*
 * Создание нового элемента кэша
 * Выделяет память, копирует ключ и данные
 *
 * Параметры:
 *   key       - ключ элемента
 *   data      - указатель на данные
 *   data_size - размер данных
 *
 * Возвращает: указатель на созданный элемент или NULL при ошибке
 */
static CacheItem*
cache_item_create(const char* key, void* data, size_t data_size)
{
    CacheItem* item = (CacheItem*)malloc(sizeof(CacheItem));  /* Выделяем память */
    if (!item)                                               /* Проверка */
        return NULL;

    item->key = (char*)malloc(strlen(key) + 1);  /* Выделяем память под ключ */
    if (!item->key) {                            /* Проверка */
        free(item);                              /* Освобождаем элемент */
        return NULL;
    }
    strcpy(item->key, key);                      /* Копируем ключ */

    item->data = malloc(data_size);              /* Выделяем память под данные */
    if (!item->data) {                           /* Проверка */
        free(item->key);                         /* Освобождаем ключ */
        free(item);                              /* Освобождаем элемент */
        return NULL;
    }
    memcpy(item->data, data, data_size);         /* Копируем данные */
    item->data_size = data_size;                 /* Сохраняем размер данных */

    item->access_history = NULL;                 /* История обращений пуста */
    item->access_count = 0;                      /* Счётчик обращений = 0 */
    item->kth_access_time = 0;                   /* Время K-го обращения = 0 */
    item->lru_next = NULL;                       /* Следующий в LRU-списке */
    item->lru_prev = NULL;                       /* Предыдущий в LRU-списке */
    item->heap_index = -1;                       /* Не в куче */
    item->is_valid = true;                       /* Элемент валиден */

    return item;                                 /* Возвращаем созданный элемент */
}

/*
 * Освобождение элемента кэша
 * Освобождает всю связанную память
 *
 * Параметры:
 *   item - элемент для освобождения
 */
static void
cache_item_free(CacheItem* item)
{
    if (!item)                                   /* Если элемент NULL */
        return;

    if (item->access_history)                    /* Если есть история обращений */
        access_history_free(item->access_history);  /* Освобождаем её */
    if (item->key)                               /* Если есть ключ */
        free(item->key);                         /* Освобождаем ключ */
    if (item->data)                              /* Если есть данные */
        free(item->data);                        /* Освобождаем данные */
    free(item);                                  /* Освобождаем сам элемент */
}

/*
 * Выбор элемента для вытеснения из кэша
 *
 * Алгоритм LRU-K:
 * 1. Среди элементов с K и более обращениями выбираем тот,
 *    у которого время K-го обращения наименьшее (самое старое)
 * 2. Если таких элементов нет, выбираем элемент с наименьшим
 *    количеством обращений (LRU-1)
 * 3. Если всё ещё нет, выбираем самый старый по LRU-1
 *
 * Параметры:
 *   cache - указатель на кэш
 *
 * Возвращает: указатель на элемент для вытеснения
 */
static CacheItem*
select_victim(struct LRUKCache* cache)
{
    CacheItem* victim = NULL;

    /* 1. Проверяем элементы с K и более обращениями в куче */
    if (cache->heap->size > 0) {
        victim = heap_extract_min(cache->heap);  /* Берём минимальный из кучи */
        if (victim && victim->is_valid)          /* Если элемент валиден */
            return victim;                       /* Возвращаем его */
    }

    /* 2. Если в куче нет валидных элементов, ищем среди всех */
    CacheItem* curr = cache->lru_head;           /* Начинаем с головы LRU-списка */
    CacheItem* candidate = NULL;                 /* Кандидат на вытеснение */
    int min_access_count = INT_MAX;              /* Минимальное количество обращений */
    time_t oldest_time = 0;                      /* Самое старое время */

    while (curr) {                               /* Обходим все элементы */
        if (!curr->is_valid) {                   /* Если элемент невалиден */
            curr = curr->lru_next;               /* Переходим к следующему */
            continue;                            /* Пропускаем */
        }

        /* Выбираем элемент с наименьшим количеством обращений */
        if (curr->access_count < min_access_count) {
            min_access_count = curr->access_count;  /* Обновляем минимум */
            candidate = curr;                    /* Запоминаем кандидата */
            oldest_time = curr->kth_access_time; /* Запоминаем время */
        }
        else if (curr->access_count == min_access_count) {
            /* При равном количестве обращений выбираем самый старый */
            if (curr->kth_access_time < oldest_time || candidate == NULL) {
                candidate = curr;                /* Запоминаем нового кандидата */
                oldest_time = curr->kth_access_time;  /* Обновляем время */
            }
        }

        curr = curr->lru_next;                   /* Переходим к следующему */
    }

    if (candidate) {                             /* Если кандидат найден */
        /* Удаляем кандидата из кучи, если он там есть */
        if (candidate->heap_index >= 0 && candidate->heap_index < cache->heap->size) {
            heap_remove(cache->heap, candidate);
        }
        return candidate;                        /* Возвращаем кандидата */
    }

    /* 3. Если всё ещё нет — берём самый старый LRU-1 */
    return cache->lru_head;                      /* Возвращаем голову LRU-списка */
}

/*
 * Вытеснение одного элемента из кэша
 * Удаляет выбранный элемент и освобождает память
 *
 * Параметры:
 *   cache - указатель на кэш
 */
static void
evict_one(struct LRUKCache* cache)
{
    if (cache->size == 0)                        /* Если кэш пуст */
        return;

    CacheItem* victim = select_victim(cache);    /* Выбираем элемент для вытеснения */
    if (!victim)                                 /* Если элемента нет */
        return;

    lru_remove(cache, victim);                   /* Удаляем из LRU-списка */
    hash_table_remove(cache, victim->key);       /* Удаляем из хеш-таблицы */

    /* Удаляем из кучи, если он там есть */
    if (victim->heap_index >= 0 && victim->heap_index < cache->heap->size) {
        heap_remove(cache->heap, victim);
    }

    victim->is_valid = false;                    /* Помечаем как невалидный */
    cache_item_free(victim);                     /* Освобождаем память */
    cache->size--;                               /* Уменьшаем размер кэша */
}

/*
 * Создание LRU-K кэша
 *
 * Параметры:
 *   k        - количество обращений для учёта (обычно 2)
 *   capacity - максимальное количество элементов в кэше
 *
 * Возвращает: указатель на созданный кэш или NULL при ошибке
 */
LRUKCache*
lru_k_cache_create(int k, int capacity)
{
    LRUKCache* cache = (LRUKCache*)malloc(sizeof(LRUKCache));  /* Выделяем память */
    if (!cache)                                                /* Проверка */
        return NULL;

    cache->k = k;                              /* Сохраняем параметр K */
    cache->capacity = capacity;                /* Сохраняем ёмкость */
    cache->size = 0;                           /* Начальный размер = 0 */
    cache->hits = 0;                           /* Хитов нет */
    cache->misses = 0;                         /* Промахов нет */

    cache->lru_head = NULL;                    /* LRU-список пуст */
    cache->lru_tail = NULL;

    /* Создаём хеш-таблицу (размер в 2 раза больше ёмкости) */
    cache->hash_size = capacity * 2 + 1;
    cache->hash_table = (CacheItem**)calloc(cache->hash_size, sizeof(CacheItem*));
    if (!cache->hash_table) {                  /* Проверка */
        free(cache);
        return NULL;
    }

    /* Создаём кучу */
    cache->heap = heap_create(capacity);
    if (!cache->heap) {                        /* Проверка */
        free(cache->hash_table);
        free(cache);
        return NULL;
    }

    return cache;                              /* Возвращаем созданный кэш */
}

/*
 * Освобождение LRU-K кэша и всей связанной памяти
 *
 * Параметры:
 *   cache - указатель на кэш
 */
void
lru_k_cache_destroy(LRUKCache* cache)
{
    if (!cache)                                /* Если кэш NULL */
        return;

    CacheItem* curr = cache->lru_head;         /* Начинаем с головы LRU-списка */
    while (curr) {                             /* Пока есть элементы */
        CacheItem* next = curr->lru_next;      /* Сохраняем следующий */
        cache_item_free(curr);                 /* Освобождаем текущий */
        curr = next;                           /* Переходим к следующему */
    }

    if (cache->hash_table)                     /* Если есть хеш-таблица */
        free(cache->hash_table);               /* Освобождаем её */
    if (cache->heap)                           /* Если есть куча */
        heap_destroy(cache->heap);             /* Освобождаем кучу */
    free(cache);                               /* Освобождаем кэш */
}

/*
 * Доступ к элементу кэша (чтение)
 * Обновляет историю обращений и LRU-порядок
 *
 * Параметры:
 *   cache - указатель на кэш
 *   key   - ключ элемента
 *
 * Возвращает: указатель на данные или NULL если элемент не найден
 */
void*
lru_k_cache_get(LRUKCache* cache, const char* key)
{
    CacheItem* item = hash_table_find(cache, key);  /* Ищем элемент по ключу */

    if (!item || !item->is_valid) {            /* Если элемент не найден или невалиден */
        cache->misses++;                       /* Увеличиваем счётчик промахов */
        return NULL;                           /* Возвращаем NULL */
    }

    time_t now = time(NULL);                   /* Получаем текущее время */
    access_history_add(item, now, cache->k);   /* Добавляем обращение в историю */
    lru_move_to_tail(cache, item);             /* Перемещаем в конец LRU-списка */

    /* Обновляем позицию в куче */
    if (item->access_count >= cache->k && item->heap_index >= 0) {
        heap_update(cache->heap, item);
    }

    cache->hits++;                             /* Увеличиваем счётчик хитов */
    return item->data;                         /* Возвращаем данные */
}

/*
 * Вставка элемента в кэш
 * Если элемент уже существует — обновляет данные
 * Если кэш заполнен — вытесняет один элемент
 *
 * Параметры:
 *   cache     - указатель на кэш
 *   key       - ключ элемента
 *   data      - указатель на данные
 *   data_size - размер данных в байтах
 *
 * Возвращает: true если успешно, false если ошибка
 */
bool
lru_k_cache_put(LRUKCache* cache, const char* key, void* data, size_t data_size)
{
    CacheItem* existing = hash_table_find(cache, key);  /* Проверяем, есть ли уже элемент */

    if (existing && existing->is_valid) {      /* Если элемент уже существует */
        /* Обновляем данные */
        if (existing->data) {                  /* Если старые данные есть */
            free(existing->data);              /* Освобождаем их */
        }
        existing->data = malloc(data_size);    /* Выделяем память под новые данные */
        if (!existing->data)                   /* Проверка */
            return false;
        memcpy(existing->data, data, data_size);  /* Копируем новые данные */
        existing->data_size = data_size;       /* Сохраняем размер */

        time_t now = time(NULL);               /* Получаем текущее время */
        access_history_add(existing, now, cache->k);  /* Добавляем обращение */
        lru_move_to_tail(cache, existing);     /* Перемещаем в конец LRU-списка */

        /* Обновляем позицию в куче */
        if (existing->access_count >= cache->k && existing->heap_index >= 0) {
            heap_update(cache->heap, existing);
        }

        return true;                           /* Успешно обновлено */
    }

    /* Если кэш заполнен, вытесняем один элемент */
    if (cache->size >= cache->capacity) {
        evict_one(cache);
    }

    /* Создаём новый элемент */
    CacheItem* new_item = cache_item_create(key, data, data_size);
    if (!new_item)                             /* Проверка */
        return false;

    time_t now = time(NULL);                   /* Получаем текущее время */
    access_history_add(new_item, now, cache->k);  /* Добавляем первое обращение */

    hash_table_insert(cache, new_item);        /* Вставляем в хеш-таблицу */
    lru_add_to_tail(cache, new_item);          /* Добавляем в LRU-список */

    /* Добавляем в кучу */
    if (new_item->access_count >= cache->k) {
        heap_insert(cache->heap, new_item);
    }

    cache->size++;                             /* Увеличиваем размер кэша */
    return true;                               /* Успешно вставлено */
}

/*
 * Удаление элемента из кэша
 *
 * Параметры:
 *   cache - указатель на кэш
 *   key   - ключ элемента
 *
 * Возвращает: true если элемент найден и удалён, false если не найден
 */
bool
lru_k_cache_remove(LRUKCache* cache, const char* key)
{
    CacheItem* item = hash_table_find(cache, key);  /* Ищем элемент */
    if (!item || !item->is_valid)              /* Если элемент не найден */
        return false;

    lru_remove(cache, item);                   /* Удаляем из LRU-списка */
    hash_table_remove(cache, key);             /* Удаляем из хеш-таблицы */

    /* Удаляем из кучи, если он там есть */
    if (item->heap_index >= 0 && item->heap_index < cache->heap->size) {
        heap_remove(cache->heap, item);
    }

    cache_item_free(item);                     /* Освобождаем память */
    cache->size--;                             /* Уменьшаем размер кэша */
    return true;                               /* Успешно удалено */
}

/*
 * Очистка кэша (удаление всех элементов)
 *
 * Параметры:
 *   cache - указатель на кэш
 */
void
lru_k_cache_clear(LRUKCache* cache)
{
    CacheItem* curr = cache->lru_head;         /* Начинаем с головы LRU-списка */
    while (curr) {                             /* Пока есть элементы */
        CacheItem* next = curr->lru_next;      /* Сохраняем следующий */
        cache_item_free(curr);                 /* Освобождаем текущий */
        curr = next;                           /* Переходим к следующему */
    }

    cache->lru_head = NULL;                    /* LRU-список пуст */
    cache->lru_tail = NULL;
    cache->size = 0;                           /* Размер = 0 */
    cache->hits = 0;                           /* Сбрасываем хиты */
    cache->misses = 0;                         /* Сбрасываем промахи */

    /* Очищаем хеш-таблицу */
    for (int i = 0; i < cache->hash_size; i++) {
        cache->hash_table[i] = NULL;
    }

    cache->heap->size = 0;                     /* Очищаем кучу */
}