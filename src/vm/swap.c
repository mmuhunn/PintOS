#include "vm/swap.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include <bitmap.h>

static struct block *swap_block;
static struct bitmap *swap_bitmap;
static struct lock swap_lock;

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

void swap_init(void) {
    swap_block = block_get_role(BLOCK_SWAP);
    if (!swap_block)
        PANIC("No swap device!");
    size_t swap_size = block_size(swap_block) / SECTORS_PER_PAGE;
    swap_bitmap = bitmap_create(swap_size);
    lock_init(&swap_lock);
}

size_t swap_out(struct page *page, void *kaddr) {
    lock_acquire(&swap_lock);
    size_t swap_slot = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
    if (swap_slot == BITMAP_ERROR)
        PANIC("No free swap slot!");
        
    size_t i; 
    for (i = 0; i < SECTORS_PER_PAGE; ++i)
        block_write(swap_block, swap_slot * SECTORS_PER_PAGE + i, kaddr + i * BLOCK_SECTOR_SIZE);

    lock_release(&swap_lock);
    page->swap_slot = swap_slot;
    return swap_slot;
}

void swap_in(struct page *page, void *kaddr) {
    lock_acquire(&swap_lock);
    size_t swap_slot = page->swap_slot;
    size_t i;
    for (i = 0; i < SECTORS_PER_PAGE; ++i)
        block_read(swap_block, swap_slot * SECTORS_PER_PAGE + i, kaddr + i * BLOCK_SECTOR_SIZE);

    bitmap_set(swap_bitmap, swap_slot, false);
    lock_release(&swap_lock);
}

void swap_free(size_t swap_slot) {
    lock_acquire(&swap_lock);
    bitmap_set(swap_bitmap, swap_slot, false);
    lock_release(&swap_lock);
}
