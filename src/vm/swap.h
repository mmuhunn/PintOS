#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>
#include "vm/page.h"

void swap_init(void);
size_t swap_out(struct page *page, void *kaddr);
void swap_in(struct page *page, void *kaddr);
void swap_free(size_t swap_slot);

#endif
