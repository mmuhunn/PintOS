#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <stdbool.h>
#include <stddef.h>
#include "threads/thread.h"
#include "filesys/file.h"

bool install_page(void *upage, void *kpage, bool writable);

enum page_type {
  VM_ANON,     // swap 영역
  VM_FILE,     // 파일 백업형
};

struct page {
  void *vaddr;              // 사용자 가상 주소 (기준 주소)
  struct frame *frame;      // 연결된 프레임
  bool writable;            // 쓰기 가능 여부
  enum page_type type;      // 페이지 타입

  // file-backed 용
  struct file *file;
  off_t offset;
  size_t read_bytes;
  size_t zero_bytes;

  // swap 용
  size_t swap_slot;

  struct hash_elem hash_elem;
};

void supplemental_page_table_init(struct hash *spt);
bool page_insert(struct hash *spt, struct page *page);
struct page *page_lookup(struct hash *spt, void *vaddr);
void supplemental_page_table_destroy(struct hash *spt);

#endif
