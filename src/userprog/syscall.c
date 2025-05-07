#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "threads/malloc.h" 

struct lock filesys_lock;
static void syscall_handler(struct intr_frame *);
void check_user_vaddr(const void *vaddr);
bool is_valid_ptr(const void *vaddr);
int create(const char *file, unsigned initial_size);
int open(const char *file);
int write(int fd, const void *buffer, unsigned size);
void user_exit(int status);
int allocate_fd(struct file *file);
struct file *get_open_file(int fd);
void check_stack_args(void *esp, int arg_count);

struct file_descriptor
{
  int fd;
  struct file *file;
  struct list_elem elem;
};

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

bool is_valid_ptr(const void *vaddr)
{
  return vaddr != NULL && is_user_vaddr(vaddr) &&
         pagedir_get_page(thread_current()->pagedir, vaddr) != NULL;
}

void check_user_vaddr(const void *vaddr) {
  if (!is_valid_ptr(vaddr)) {
    printf("[vaddr]Invalid user address: %p\n", vaddr);
    user_exit(-1);
  }
}

void check_buffer_range(const void *buffer, size_t size) {
  const uint8_t *ptr;
  for (ptr = (const uint8_t *)buffer; ptr < (const uint8_t *)buffer + size; ptr++) {
    if (!is_user_vaddr(ptr)) {
      printf("[range] Not user vaddr: %p\n", ptr);
      user_exit(-1);
    }

    void *page = pagedir_get_page(thread_current()->pagedir, ptr);
    if (page == NULL) {
      printf("[range] pagedir NULL for: %p\n", ptr);
      user_exit(-1);
    }
  }
}

void check_stack_args(void *esp, int arg_count) {
  int i;
  for (i = 0; i <= arg_count; i++) {
    check_user_vaddr((uint32_t *)esp + i);
  }
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  int syscall_num = *(int *)f->esp;
  check_user_vaddr(f->esp);  // syscall_num

  printf("[hex_dump near f->esp]\n");
  hex_dump((uintptr_t)f->esp - 16, f->esp - 16, 64, true);

  printf("[debug] syscall_num=%d\n", syscall_num);
  printf("[debug] esp = %p\n", f->esp);
  printf("[debug] args: fd=%d, buffer=%p, size=%u\n",
        *((int *)f->esp + 1),
        (void *)*((int *)f->esp + 2),
        *((unsigned *)f->esp + 3));

  printf("[syscall] esp(32dump): %p\n", f->esp);
  hex_dump("user esp", f->esp, 32, true);
  
  switch (syscall_num)
  {
  case SYS_HALT:
    break;
  case SYS_EXIT:
  {
    int status = *((int *)f->esp + 1);
    user_exit(status);
    break;
  }
  case SYS_EXEC:
    break;
  case SYS_WAIT:
    break;
  case SYS_CREATE:
  {
    const char *filename = (const char *)*((int *)f->esp + 1);
    unsigned size = *((unsigned *)f->esp + 2);
    check_user_vaddr(filename);
    f->eax = create(filename, size);
    break;
  }
  case SYS_REMOVE:
    break;
  case SYS_OPEN:
  {
    const char *filename = (const char *)*((int *)f->esp + 1);
    check_user_vaddr(filename);

    f->eax = open(filename);
    break;
  }
  case SYS_FILESIZE:
    break;
  case SYS_READ:
    break;
  case SYS_WRITE:
  {
    check_stack_args(f->esp, 3);  // fd, buffer, size

    int fd = *((int *)f->esp + 1);
    void *buffer = (void *)*((int *)f->esp + 2);
    unsigned size = *((unsigned *)f->esp + 3);

    check_buffer_range(buffer, size);
    f->eax = write(fd, buffer, size);
    break;
  }

  case SYS_SEEK:
    break;
  case SYS_TELL:
    break;
  case SYS_CLOSE:
    break;
  }
}

int write(int fd, const void *buffer, unsigned size)
{
  printf("[write] fd=%d, buffer=%p, size=%u\n", fd, buffer, size);
  if (fd == 1)
  {
    putbuf(buffer, size); // stdout
    return size;
  }

  struct file *file = get_open_file(fd);
  if (file == NULL)
    return -1;

  lock_acquire(&filesys_lock);
  int result = file_write(file, buffer, size);
  lock_release(&filesys_lock);
  return result;
}

int allocate_fd(struct file *file)
{
  struct thread *t = thread_current();
  struct file_descriptor *fd_elem = malloc(sizeof(struct file_descriptor));
  if (fd_elem == NULL)
    return -1;

  fd_elem->fd = t->next_fd++;
  fd_elem->file = file;
  list_push_back(&t->fd_list, &fd_elem->elem);
  return fd_elem->fd;
}

struct file *get_open_file(int fd)
{
  struct thread *t = thread_current();
  struct list_elem *e;

  for (e = list_begin(&t->fd_list); e != list_end(&t->fd_list); e = list_next(e))
  {
    struct file_descriptor *fd_elem = list_entry(e, struct file_descriptor, elem);
    if (fd_elem->fd == fd)
      return fd_elem->file;
  }
  return NULL;
}

int create(const char *file, unsigned initial_size)
{
  lock_acquire(&filesys_lock);
  bool success = filesys_create(file, initial_size);
  lock_release(&filesys_lock);
  return success;
}

int open(const char *file)
{
  lock_acquire(&filesys_lock);
  struct file *f = filesys_open(file);
  lock_release(&filesys_lock);

  if (f == NULL)
    return -1;

  return allocate_fd(f);
}

void user_exit(int status) {
  struct thread *cur = thread_current();
  cur->exit_code = status;

  printf("%s: exit(%d)\n", cur->name, status);

  thread_exit();  // 실제 종료
}
/*
static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  printf ("system call!\n");
  thread_exit ();
}
*/