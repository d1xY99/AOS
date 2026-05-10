#include "stdlib.h"
#include "pthread.h"
#include "string.h"
#include "sys/mman.h"
#include "../../../core/incl/kernel/kernel_user_shared.h"
#include <types.h>

#define BLOCK_SIZE 4096
#define CANARY "187420"
#define CANARY_LENGTH 7

typedef struct m_block
{
  int freed;
  size_t size;
  void *data;
  struct m_block *next;
} m_block;

static size_t malloc_called = 0;
static size_t used_blocks = 0;
static void *memory_end = NULL;
static m_block *head = NULL;
static m_block *tail = NULL;
static pthread_spinlock_t malloc_lock = {SPIN_INIT, SPIN_UNLOCKED};

#define HEAP_ARENA_PAGES 256
#define HEAP_ARENA_SIZE (HEAP_ARENA_PAGES * BLOCK_SIZE)

static size_t align_up(size_t value, size_t alignment)
{
  return (value + alignment - 1) & ~(alignment - 1);
}

static int size_add_overflow(size_t a, size_t b, size_t *out)
{
  if (a > (size_t)-1 - b)
    return 1;
  *out = a + b;
  return 0;
}

static int size_mul_overflow(size_t a, size_t b, size_t *out)
{
  if (a == 0 || b == 0)
  {
    *out = 0;
    return 0;
  }
  if (a > (size_t)-1 / b)
    return 1;
  *out = a * b;
  return 0;
}

static int ptr_in_userspace(const void *ptr)
{
#if defined(CMAKE_X86_64) || defined(CMAKE_ARMV8_RPI3)
  return (size_t)ptr < (size_t)USER_BREAK;
#else
  return (size_t)ptr < 0x80000000U;
#endif
}

static unsigned char *block_start(m_block *block)
{
  return (unsigned char *)block - CANARY_LENGTH;
}

static unsigned char *block_end(m_block *block)
{
  return block_start(block) + block->size;
}

static int blocks_adjacent(m_block *left, m_block *right)
{
  return block_end(left) == block_start(right);
}

static void coalesce_free_blocks(void)
{
  m_block *curr = head;
  while (curr != NULL && curr->next != NULL)
  {
    if (curr->freed && curr->next->freed && blocks_adjacent(curr, curr->next))
    {
      m_block *next = curr->next;
      curr->size += next->size;
      curr->next = next->next;
      if (tail == next)
        tail = curr;
      continue;
    }
    curr = curr->next;
  }
}

static m_block *ll_find_first_free(size_t size)
{
  m_block *tmp_m_block = head;

  while (tmp_m_block != NULL)
  {
    size_t payload_size = tmp_m_block->size - sizeof(m_block) - CANARY_LENGTH;
    if (tmp_m_block->freed && payload_size >= size)
    {
      return tmp_m_block;
    }
    tmp_m_block = tmp_m_block->next;
  }

  return NULL;
}

static m_block *ll_get_memblock_with_dataptr(void *ptr)
{
  m_block *tmp_m_block = head;

  while (tmp_m_block != NULL)
  {
    if (tmp_m_block->data == ptr)
    {
      return tmp_m_block;
    }

    tmp_m_block = tmp_m_block->next;
  }

  return NULL;
}

static void *m_extend(size_t size)
{
  if (memory_end == NULL)
  {
    size_t arena_size = size > HEAP_ARENA_SIZE ? size : HEAP_ARENA_SIZE;
    if (arena_size > (size_t)-1 - (BLOCK_SIZE - 1))
      return (void *)-1;
    arena_size = align_up(arena_size, BLOCK_SIZE);
    void *base = mmap(NULL, arena_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED)
      return (void *)-1;
    memory_end = (void *)((unsigned char *)base + arena_size);
    return base;
  }

  unsigned char *next_canary =
      (unsigned char *)tail + tail->size - CANARY_LENGTH;
  unsigned char *new_block_end = next_canary + size;
  if ((void *)new_block_end <= memory_end)
  {
    return next_canary;
  }

  size_t arena_size = size > HEAP_ARENA_SIZE ? size : HEAP_ARENA_SIZE;
  if (arena_size > (size_t)-1 - (BLOCK_SIZE - 1))
    return (void *)-1;
  arena_size = align_up(arena_size, BLOCK_SIZE);
  void *base = mmap(NULL, arena_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base == MAP_FAILED)
    return (void *)-1;
  memory_end = (void *)((unsigned char *)base + arena_size);
  return base;
}

static void m_write_canary(void *c_begin)
{
  char canary[CANARY_LENGTH] = CANARY;
  char *t_canary = (char *)c_begin;

  for (size_t i = 0; i < CANARY_LENGTH; i++)
  {
    t_canary[i] = canary[i];
  }
}

static void m_check_canary()
{
  char canary[CANARY_LENGTH] = CANARY;

  m_block *tmp_m_block = head;

  if (head == NULL && tail == NULL)
    return;

  while (tmp_m_block != NULL)
  {
    char *t_canary = (char *)((unsigned char *)tmp_m_block - CANARY_LENGTH);

    for (size_t i = 0; i < CANARY_LENGTH; i++)
    {
      if (t_canary[i] != canary[i])
        exit(-1);
    }

    tmp_m_block = tmp_m_block->next;
  }
}

void *malloc(size_t size)
{
  if (size == 0)
    return NULL;

  size_t complete_m_size;
  if (size_add_overflow(size, sizeof(m_block) + CANARY_LENGTH, &complete_m_size))
    return NULL;

  pthread_spin_lock(&malloc_lock);

  malloc_called++;
  m_check_canary();
  coalesce_free_blocks();

  m_block *reusable = ll_find_first_free(size);
  if (reusable != NULL)
  {
    unsigned char *reuse_canary = (unsigned char *)reusable - CANARY_LENGTH;
    size_t remaining = reusable->size - complete_m_size;
    size_t min_block = sizeof(m_block) + CANARY_LENGTH + 1;

    if (remaining >= min_block)
    {
      unsigned char *new_canary = reuse_canary + complete_m_size;
      m_block *new_block = (m_block *)(new_canary + CANARY_LENGTH);
      m_write_canary(new_canary);
      new_block->freed = 1;
      new_block->data = (void *)((unsigned char *)new_block + sizeof(m_block));
      new_block->size = remaining;
      new_block->next = reusable->next;
      reusable->next = new_block;
      if (tail == reusable)
        tail = new_block;
      reusable->size = complete_m_size;
    }

    m_write_canary(reuse_canary);
    reusable->freed = 0;
    reusable->data = (void *)((unsigned char *)reusable + sizeof(m_block));
    used_blocks++;
    pthread_spin_unlock(&malloc_lock);
    return reusable->data;
  }

  void *canary_begin = m_extend(complete_m_size);
  if (canary_begin == (void *)-1)
  {
    pthread_spin_unlock(&malloc_lock);
    return NULL;
  }

  m_block *address_to_write;
  void *data_ptr;
  address_to_write = (m_block *)((unsigned char *)canary_begin + CANARY_LENGTH);
  data_ptr = (void *)((unsigned char *)address_to_write + sizeof(m_block));
  m_write_canary(canary_begin);
  address_to_write->freed = 0;
  address_to_write->data = data_ptr;
  address_to_write->next = NULL;
  address_to_write->size = complete_m_size;
  if (head == NULL)
  {
    head = address_to_write;
    tail = address_to_write;
  }
  else
  {
    tail->next = address_to_write;
    tail = address_to_write;
  }

  used_blocks++;
  pthread_spin_unlock(&malloc_lock);
  return data_ptr;
}

void free(void *ptr)
{
  if (ptr == NULL)
    return;

  pthread_spin_lock(&malloc_lock);
  m_check_canary();

  if (!ptr_in_userspace(ptr))
  {
    pthread_spin_unlock(&malloc_lock);
    exit(-1);
  }

  if (head == NULL && tail == NULL)
  {
    pthread_spin_unlock(&malloc_lock);
    exit(-1);
  }

  m_block *m_block_to_free = ll_get_memblock_with_dataptr(ptr);
  if (m_block_to_free == NULL || m_block_to_free->freed == 1)
  {
    pthread_spin_unlock(&malloc_lock);
    exit(-1);
  }

  m_block_to_free->freed = 1;
  used_blocks--;

  pthread_spin_unlock(&malloc_lock);
}

int atexit(void (*function)(void))
{
  return -1;
}

void *calloc(size_t nmemb, size_t size)
{
  if (nmemb == 0 || size == 0)
    return NULL;

  size_t total;
  if (size_mul_overflow(nmemb, size, &total))
    return NULL;

  void *ptr = malloc(total);
  if (ptr == NULL)
    return NULL;

  memset(ptr, 0, total);
  return ptr;
}

void *realloc(void *ptr, size_t size)
{
  if (ptr == NULL)
    return malloc(size);

  if (size == 0)
  {
    free(ptr);
    return NULL;
  }

  pthread_spin_lock(&malloc_lock);
  m_check_canary();
  if (!ptr_in_userspace(ptr))
  {
    pthread_spin_unlock(&malloc_lock);
    exit(-1);
  }

  m_block *old_block = ll_get_memblock_with_dataptr(ptr);
  if (old_block == NULL || old_block->freed)
  {
    pthread_spin_unlock(&malloc_lock);
    exit(-1);
  }

  coalesce_free_blocks();

  size_t overhead = sizeof(m_block) + CANARY_LENGTH;
  size_t old_data_size = old_block->size - overhead;

  if (size <= old_data_size)
  {
    pthread_spin_unlock(&malloc_lock);
    return ptr;
  }

  size_t new_total_size;
  if (size_add_overflow(size, overhead, &new_total_size))
  {
    pthread_spin_unlock(&malloc_lock);
    return NULL;
  }

  // Try to grow in place if this is the tail block and space is available
  if (old_block->next == NULL)
  {
    unsigned char *old_canary = (unsigned char *)old_block - CANARY_LENGTH;
    unsigned char *new_block_end = old_canary + new_total_size;
    if ((void *)new_block_end <= memory_end)
    {
      old_block->size = new_total_size;
      m_write_canary(old_canary);
      pthread_spin_unlock(&malloc_lock);
      return ptr;
    }
  }

  // Try to merge with a following free block
  if (old_block->next && old_block->next->freed &&
      blocks_adjacent(old_block, old_block->next))
  {
    m_block *next = old_block->next;
    size_t combined_size = old_block->size + next->size;
    size_t combined_payload = combined_size - overhead;
    if (combined_payload >= size)
    {
      // First, merge the blocks
      old_block->size = combined_size;
      old_block->next = next->next;
      if (tail == next)
        tail = old_block;

      size_t remaining = combined_size - new_total_size;
      size_t min_block = sizeof(m_block) + CANARY_LENGTH + 1;
      if (remaining >= min_block)
      {
        // Split the merged block
        // Layout: [canary][m_block][data...]
        unsigned char *old_canary = (unsigned char *)old_block - CANARY_LENGTH;
        unsigned char *new_canary = old_canary + new_total_size;
        m_block *new_block = (m_block *)(new_canary + CANARY_LENGTH);
        m_write_canary(new_canary);
        new_block->freed = 1;
        new_block->data = (void *)((unsigned char *)new_block + sizeof(m_block));
        new_block->size = remaining;
        new_block->next = old_block->next;
        old_block->next = new_block;
        if (tail == old_block)
          tail = new_block;
        old_block->size = new_total_size;
      }
      else
      {
        // No splitting needed, just update canary at the end
        unsigned char *old_canary = (unsigned char *)old_block - CANARY_LENGTH;
        m_write_canary(old_canary);
      }

      pthread_spin_unlock(&malloc_lock);
      return ptr;
    }
  }

  pthread_spin_unlock(&malloc_lock);

  void *new_ptr = malloc(size);
  if (new_ptr == NULL)
    return NULL;

  size_t copy_size = old_data_size < size ? old_data_size : size;
  memcpy(new_ptr, ptr, copy_size);
  free(ptr);
  return new_ptr;
}
