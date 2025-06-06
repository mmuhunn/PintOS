#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <stdbool.h>
#include "vm/page.h"
#include "threads/palloc.h"

struct frame {
    void *kaddr;           // 커널 가상주소 (실제 프레임의 시작 주소)
    struct page *page;     // 연결된 page
    struct hash_elem hash_elem;
};

struct frame_table_entry {
    void *kpage;                   
    void *upage;                   
    struct thread *owner;          
    struct hash_elem hash_elem;    
    struct list_elem list_elem;    
    bool pinned;                   
};

void frame_table_init(void);
struct frame *frame_allocate(enum palloc_flags flags, struct page *page);
void frame_free(void *kaddr);
struct frame *frame_evict(void);

#endif
