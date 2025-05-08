#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "list.h"
#include "process.h"

#define VALIDATE_PTR(ptr)   \
	if (!is_valid_ptr(ptr)) \
		exit(-1);

static void syscall_handler(struct intr_frame *);
bool is_valid_ptr(const void *);
struct file_descriptor *get_open_file(int fd);
int wait(tid_t tid);
void halt(void);
bool create(const char *file_name, unsigned initial_size);
bool remove(const char *file_name);
int filesize(int fd);
void seek(int fd, unsigned position);
unsigned tell(int fd);

struct lock fs_lock;
struct list open_files;

void release_filesys_lock();
void acquire_filesys_lock();

extern bool running;

struct file_descriptor
{
	int fd_num;
	tid_t owner;
	struct file *file_struct;
	struct list_elem elem;
};

void syscall_init(void)
{
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
	lock_init(&fs_lock);
	list_init(&open_files);
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
	int *p = f->esp;
	VALIDATE_PTR(p);
	int system_call = *p;

	switch (system_call)
	{
	case SYS_HALT:
		halt();
		break;

	case SYS_EXIT:
		VALIDATE_PTR(p + 1);
		exit(*(p + 1));
		break;

	case SYS_EXEC:
		VALIDATE_PTR(p + 1);
		VALIDATE_PTR(*(p + 1));
		f->eax = exec(*(p + 1));
		break;

	case SYS_WAIT:
		VALIDATE_PTR(p + 1);
		tid_t tid = *(p + 1);
		f->eax = wait(tid);
		break;

	case SYS_CREATE:
		VALIDATE_PTR(p + 5);
		VALIDATE_PTR(*(p + 4));
		f->eax = create(*(p + 4), *(p + 5));
		break;

	case SYS_REMOVE:
		VALIDATE_PTR(p + 1);
		VALIDATE_PTR(*(p + 1));
		f->eax = remove(*(p + 1));
		break;

	case SYS_OPEN:
		VALIDATE_PTR(p + 1);
		VALIDATE_PTR(*(p + 1));

		acquire_filesys_lock();
		struct file *fptr = filesys_open(*(p + 1));
		release_filesys_lock();
		if (fptr == NULL)
			f->eax = -1;
		else
		{
			struct file_descriptor *pfile = malloc(sizeof(*pfile));
			pfile->file_struct = fptr;
			pfile->fd_num = thread_current()->fd_count;
			thread_current()->fd_count++;
			list_push_back(&thread_current()->files, &pfile->elem);
			f->eax = pfile->fd_num;
		}
		break;

	case SYS_FILESIZE:
		VALIDATE_PTR(p + 1);
		f->eax = filesize(*(p + 1));
		break;

	case SYS_READ:
		VALIDATE_PTR(p + 7);
		VALIDATE_PTR(*(p + 6));
		if (*(p + 5) == 0)
		{
			int i;
			uint8_t *buffer = *(p + 6);
			for (i = 0; i < *(p + 7); i++)
				buffer[i] = input_getc();
			f->eax = *(p + 7);
		}
		else
		{
			struct file_descriptor *fptr = get_open_file(*(p + 5));
			if (fptr == NULL)
				f->eax = -1;
			else
			{
				acquire_filesys_lock();
				f->eax = file_read(fptr->file_struct, *(p + 6), *(p + 7));
				release_filesys_lock();
			}
		}
		break;

	case SYS_WRITE:
		VALIDATE_PTR(p + 7);
		VALIDATE_PTR(*(p + 6));
		if (*(p + 5) == 1)
		{
			putbuf(*(p + 6), *(p + 7));
			f->eax = *(p + 7);
		}
		else
		{
			struct file_descriptor *fptr = get_open_file(*(p + 5));
			if (fptr == NULL)
				f->eax = -1;
			else
			{
				acquire_filesys_lock();
				f->eax = file_write(fptr->file_struct, *(p + 6), *(p + 7));
				release_filesys_lock();
			}
		}
		break;

	case SYS_SEEK:
		VALIDATE_PTR(p + 5);
		seek(*(p+4), *(p+5));
		break;

	case SYS_TELL:
		VALIDATE_PTR(p + 1);
		f->eax = tell(*(p+1));
		break;

	case SYS_CLOSE:
		VALIDATE_PTR(p + 1);
		acquire_filesys_lock();
		close_file(&thread_current()->files, *(p + 1));
		release_filesys_lock();
		break;

	default:
		printf("Default %d\n", *p);
	}
}

int wait(tid_t tid)
{
	return process_wait(tid);
}

void halt(void)
{
	shutdown_power_off();
}

int exec(char *file_name)
{
	acquire_filesys_lock();
	char *fn_cp = malloc(strlen(file_name) + 1);
	strlcpy(fn_cp, file_name, strlen(file_name) + 1);

	char *save_ptr;
	fn_cp = strtok_r(fn_cp, " ", &save_ptr);

	struct file *f = filesys_open(fn_cp);

	if (f == NULL)
	{
		release_filesys_lock();
		return -1;
	}
	else
	{
		file_close(f);
		release_filesys_lock();
		return process_execute(file_name);
	}
}

void exit(int status)
{
	// printf("Exit : %s %d %d\n",thread_current()->name, thread_current()->tid, status);
	struct list_elem *e;

	for (e = list_begin(&thread_current()->parent->child_proc); e != list_end(&thread_current()->parent->child_proc);
		 e = list_next(e))
	{
		struct child *f = list_entry(e, struct child, elem);
		if (f->tid == thread_current()->tid)
		{
			f->used = true;
			f->exit_error = status;
		}
	}

	thread_current()->exit_error = status;

	if (thread_current()->parent->waitingon == thread_current()->tid)
		sema_up(&thread_current()->parent->child_lock);

	thread_exit();
}

bool create(const char *file_name, unsigned initial_size)
{
	if (!is_valid_ptr(file_name))
	{
		exit(-1);
	}

	acquire_filesys_lock();
	bool success = filesys_create(file_name, initial_size);
	release_filesys_lock();

	return success;
}

bool remove(const char *file_name)
{
	if (!is_valid_ptr(file_name))
	{
		exit(-1);
	}

	acquire_filesys_lock();
	bool success = filesys_remove(file_name);
	release_filesys_lock();

	return success;
}

int filesize(int fd)
{
	struct file_descriptor *fdesc = get_open_file(fd);
	if (fdesc == NULL)
		return -1;

	acquire_filesys_lock();
	int size = file_length(fdesc->file_struct);
	release_filesys_lock();

	return size;
}

void seek(int fd, unsigned position)
{
	struct file_descriptor *fdesc = get_open_file(fd);
	if (fdesc == NULL)
		return;

	acquire_filesys_lock();
	file_seek(fdesc->file_struct, position);
	release_filesys_lock();
}

unsigned tell(int fd)
{
	struct file_descriptor *fdesc = get_open_file(fd);
	if (fdesc == NULL)
		return -1;

	acquire_filesys_lock();
	unsigned pos = file_tell(fdesc->file_struct);
	release_filesys_lock();

	return pos;
}

bool is_valid_ptr(const void *usr_ptr)
{
	if (!is_user_vaddr(usr_ptr))
	{
		return false;
	}
	void *ptr = pagedir_get_page(thread_current()->pagedir, usr_ptr);
	if (!ptr)
	{
		return false;
	}
	return true;
}

struct file_descriptor *
get_open_file(int fd)
{
	struct list_elem *e;
	struct list *files = &thread_current()->files;

	for (e = list_begin(files); e != list_end(files); e = list_next(e))
	{
		struct file_descriptor *fd_struct = list_entry(e, struct file_descriptor, elem);

		if (fd_struct->fd_num == fd)
		{
			return fd_struct;
		}
	}

	return NULL;
}

void close_file(struct list *files, int fd_num)
{

	struct list_elem *e;

	struct file_descriptor *f;

	for (e = list_begin(files); e != list_end(files);
		 e = list_next(e))
	{
		f = list_entry(e, struct file_descriptor, elem);
		if (f->fd_num == fd_num)
		{
			file_close(f->file_struct);
			list_remove(e);
		}
	}

	free(f);
}

void close_all_files(struct list *files)
{

	struct list_elem *e;

	while (!list_empty(files))
	{
		e = list_pop_front(files);

		struct file_descriptor *f = list_entry(e, struct file_descriptor, elem);

		file_close(f->file_struct);
		list_remove(e);
		free(f);
	}
}

void acquire_filesys_lock()
{
	lock_acquire(&fs_lock);
}

void release_filesys_lock()
{
	lock_release(&fs_lock);
}