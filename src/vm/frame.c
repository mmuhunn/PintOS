#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include <hash.h>
#include <debug.h>

static struct hash frame_table;  // 전역 프레임 테이블
static struct lock frame_lock;   // 동시 접근 보호

// 해시 함수
static unsigned frame_hash(const struct hash_elem *e, void *aux UNUSED) {
    const struct frame *f = hash_entry(e, struct frame, hash_elem);
    return hash_bytes(&f->kaddr, sizeof f->kaddr);
}

// 비교 함수
static bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    const struct frame *fa = hash_entry(a, struct frame, hash_elem);
    const struct frame *fb = hash_entry(b, struct frame, hash_elem);
    return fa->kaddr < fb->kaddr;
}

// 테이블 초기화
void frame_table_init(void) {
    hash_init(&frame_table, frame_hash, frame_less, NULL);
    lock_init(&frame_lock);
}

// 프레임 할당
struct frame *frame_allocate(enum palloc_flags flags, struct page *page) {
    lock_acquire(&frame_lock);

    void *kaddr = palloc_get_page(flags);
    if (!kaddr) {
        // TODO: 프레임 부족 시 evict 로직 필요 (교체 알고리즘)
        lock_release(&frame_lock);
        return NULL;
    }

    struct frame *f = malloc(sizeof(struct frame));
    if (!f) {
        palloc_free_page(kaddr);
        lock_release(&frame_lock);
        return NULL;
    }
    f->kaddr = kaddr;
    f->page = page;

    hash_insert(&frame_table, &f->hash_elem);

    lock_release(&frame_lock);
    return f;
}

// 프레임 해제
void frame_free(void *kaddr) {
    lock_acquire(&frame_lock);

    struct frame tmp;
    tmp.kaddr = kaddr;
    struct hash_elem *e = hash_find(&frame_table, &tmp.hash_elem);
    if (e) {
        struct frame *f = hash_entry(e, struct frame, hash_elem);
        hash_delete(&frame_table, &f->hash_elem);
        palloc_free_page(f->kaddr);
        free(f);
    }
    lock_release(&frame_lock);
}
