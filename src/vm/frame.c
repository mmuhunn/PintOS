#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"   
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

struct frame *frame_allocate(enum palloc_flags flags, struct page *page) {
    lock_acquire(&frame_lock);

    void *kaddr = palloc_get_page(flags);
    if (!kaddr) {
        struct frame *evicted = frame_evict();
        ASSERT(evicted != NULL);
        kaddr = evicted->kaddr;
        free(evicted);  // 이전 프레임 구조체는 메모리 해제
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

struct frame *frame_evict(void) {
    struct hash_iterator i;

    // 2바퀴까지 순회 시도
    int round;
    for (round = 0; round < 2; round++) {
        hash_first(&i, &frame_table);

        while (hash_next(&i)) {
            struct frame *f = hash_entry(hash_cur(&i), struct frame, hash_elem);
            struct page *p = f->page;

            if (p == NULL) continue;

            // accessed 비트 확인
            if (pagedir_is_accessed(thread_current()->pagedir, p->vaddr)) {
                pagedir_set_accessed(thread_current()->pagedir, p->vaddr, false);
                continue;
            }

            // evict 조건 충족
            if (p->type == VM_ANON) {
                swap_out(p, f->kaddr);
            } else if (p->type == VM_FILE) {
                if (pagedir_is_dirty(thread_current()->pagedir, p->vaddr)) {
                    // (선택 구현) write-back 필요 시 수행
                }
            }

            pagedir_clear_page(thread_current()->pagedir, p->vaddr);
            p->frame = NULL;
            f->page = NULL;

            hash_delete(&frame_table, &f->hash_elem);
            return f;
        }
    }

    PANIC("No frame could be evicted after two passes!");
}