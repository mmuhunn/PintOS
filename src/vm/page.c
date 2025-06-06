#include "vm/page.h"
#include "vm/frame.h" 
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include <hash.h>


bool install_page(void *upage, void *kpage, bool writable) {
    struct thread *t = thread_current();
    return (pagedir_get_page(t->pagedir, upage) == NULL
            && pagedir_set_page(t->pagedir, upage, kpage, writable));
}

void page_destructor(struct hash_elem *e, void *aux);
// 해시 함수
static unsigned
page_hash(const struct hash_elem *e, void *aux UNUSED) {
  const struct page *p = hash_entry(e, struct page, hash_elem);
  return hash_bytes(&p->vaddr, sizeof(p->vaddr));
}

// 해시 비교 함수
static bool
page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  const struct page *pa = hash_entry(a, struct page, hash_elem);
  const struct page *pb = hash_entry(b, struct page, hash_elem);
  return pa->vaddr < pb->vaddr;
}

// 페이지 테이블 초기화
void
supplemental_page_table_init(struct hash *spt) {
  hash_init(spt, page_hash, page_less, NULL);
}

// 페이지 추가
bool
page_insert(struct hash *spt, struct page *page) {
  return hash_insert(spt, &page->hash_elem) == NULL;
}

// 페이지 조회
struct page *
page_lookup(struct hash *spt, void *vaddr) {
  struct page p;
  p.vaddr = pg_round_down(vaddr);  // 페이지 기준 주소로 정렬
  struct hash_elem *e = hash_find(spt, &p.hash_elem);
  if (e == NULL) return NULL;
  return hash_entry(e, struct page, hash_elem);
}

// 전체 파괴
void
supplemental_page_table_destroy(struct hash *spt) {
  hash_destroy(spt, page_destructor);
}

// 메모리 해제 함수
void page_destructor(struct hash_elem *e, void *aux UNUSED) {
  struct page *p = hash_entry(e, struct page, hash_elem);
  if (p->frame != NULL) {
    frame_free(p->frame->kaddr);  // 프레임 해제
  }
  free(p);  // 페이지 구조체 해제
}