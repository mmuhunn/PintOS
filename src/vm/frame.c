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
/* vm/frame.c */


static struct list_elem *clock_ptr = NULL;

void frame_init(void) {

    hash_init(&frame_hash, frame_hash_func, frame_less_func, NULL);

    list_init(&frame_list);
  
    lock_init(&frame_lock);
 
    clock_ptr = NULL;
    
    printf("Frame table initialized\n");
}

void frame_do_free(struct frame_table_entry *fte, bool free_memory) {
    ASSERT(fte != NULL);
    
    lock_acquire(&frame_lock);

    if (fte->upage != NULL && fte->owner != NULL) {

        pagedir_clear_page(fte->owner->pagedir, fte->upage);
    }

    hash_delete(&frame_hash, &fte->hash_elem);
    list_remove(&fte->list_elem);

    if (clock_ptr == &fte->list_elem) {
        clock_ptr = list_next(clock_ptr);
        if (clock_ptr == list_end(&frame_list)) {
            clock_ptr = list_begin(&frame_list);
        }
    }
    
    if (free_memory) {

        palloc_free_page(fte->kpage);
        free(fte);
    } else {

        fte->upage = NULL;
        fte->owner = NULL;
        fte->pinned = false;

    }
    
    lock_release(&frame_lock);
}


void frame_set_pinned(struct frame_table_entry *fte, bool pinned) {
    ASSERT(fte != NULL);
    
    lock_acquire(&frame_lock);

    fte->pinned = pinned;
    
    lock_release(&frame_lock);
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


struct frame_table_entry *pick_frame_to_evict(void) {
    ASSERT(lock_held_by_current_thread(&frame_lock));

    if (list_empty(&frame_list)) {
        return NULL;
    }

    if (clock_ptr == NULL || clock_ptr == list_end(&frame_list)) {
        clock_ptr = list_begin(&frame_list);
    }

    int max_iterations = list_size(&frame_list) * 2;  
    
    for (int i = 0; i < max_iterations; i++) {
        struct frame_table_entry *fte = 
            list_entry(clock_ptr, struct frame_table_entry, list_elem);
        
        clock_ptr = list_next(clock_ptr);
        if (clock_ptr == list_end(&frame_list)) {
            clock_ptr = list_begin(&frame_list);
        }

        if (fte->pinned) {
            continue;  
        }

        bool accessed = false;
        if (fte->upage != NULL && fte->owner != NULL) {
            accessed = pagedir_is_accessed(fte->owner->pagedir, fte->upage);
        }
        
        if (accessed) {
            pagedir_set_accessed(fte->owner->pagedir, fte->upage, false);
            continue;  
        } else {
            return fte;
        }
    }

    return NULL;
}

