#include "slimcc.h"

#if USE_ASAN
# include <sanitizer/asan_interface.h>

__attribute__((visibility("default"))) const char *__asan_default_options(void) {
  return "detect_leaks=0";
}
#endif

#define FREE_THRESHOLD (100 * 1024)
#define ARENA_POOL_SIZE 8168

struct Pool {
  char buf[ARENA_POOL_SIZE];
  Pool *next;
};

bool check_mem_usage(void) {
#if USE_ASAN || defined(__FILC__)
  return true;
#else
  struct rusage stat;
  getrusage(RUSAGE_SELF, &stat);
  return stat.ru_maxrss > FREE_THRESHOLD;
#endif
}

static Pool *new_pool(SlimccCtx *sctx) {
  Pool *p;
  if (sctx->pool_freelist) {
    p = sctx->pool_freelist;
    sctx->pool_freelist = p->next;
  } else {
    p = malloc(sizeof(Pool));
  }

#if USE_ASAN
  __asan_poison_memory_region(&p->buf, ARENA_POOL_SIZE);
#endif
  p->next = NULL;
  return p;
}

static void *allocate(SlimccCtx *sctx, Arena *arena, size_t sz, bool clear) {
  size_t aligned_sz = (sz + 15) & -16LL;

  void *ptr;
  if ((arena->used + aligned_sz) <= ARENA_POOL_SIZE) {
    ptr = &arena->cur->buf[arena->used];
    arena->used += aligned_sz;
  } else {
    arena->cur = arena->cur->next = new_pool(sctx);
    ptr = &arena->cur->buf;
    arena->used = aligned_sz;
  }

#if USE_ASAN
  __asan_unpoison_memory_region(ptr, sz);
  if (clear)
    memset(ptr, 0, sz);
#else
  if (clear)
    for (int i = 0; i < (aligned_sz >> 3); i++)
      ((int64_t *)ptr)[i] = 0;
#endif
  return ptr;
}

void arena_on(SlimccCtx *sctx, Arena *arena) {
  arena->head = arena->cur = new_pool(sctx);
  arena->used = 0;
}

void arena_off(SlimccCtx *sctx, Arena *arena) {
  if (sctx->free_alloc) {
    for (Pool *p = arena->head; p;) {
      Pool *tmp = p;
      p = p->next;
      free(tmp);
    }
  } else {
    arena->cur->next = sctx->pool_freelist;
    sctx->pool_freelist = arena->head;
  }
  arena->cur = NULL;
}

void *arena_malloc(SlimccCtx *sctx, Arena *a, size_t sz) {
  return allocate(sctx, a, sz, false);
}

void *arena_calloc(SlimccCtx *sctx, Arena *a, size_t sz) {
  return allocate(sctx, a, sz, true);
}

void *ast_arena_malloc(SlimccCtx *sctx, size_t sz) {
  if (!sctx->ast_arena.cur)
    return malloc(sz);
  return allocate(sctx, &sctx->ast_arena, sz, false);
}

void *ast_arena_calloc(SlimccCtx *sctx, size_t sz) {
  if (!sctx->ast_arena.cur)
    return calloc(1, sz);
  return allocate(sctx, &sctx->ast_arena, sz, true);
}
