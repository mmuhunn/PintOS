/* Try writing a file in the most normal way. */

#include <syscall.h>
#include "tests/userprog/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  
  printf(">>> REACHED test_main()\n");
  void *esp_val;
  asm("movl %%esp, %0" : "=g"(esp_val));
  printf("[userprog] current esp: %p\n", esp_val);

  int handle, byte_cnt;

  CHECK (create ("test.txt", sizeof sample - 1), "create \"test.txt\"");
  CHECK ((handle = open ("test.txt")) > 1, "open \"test.txt\"");

  byte_cnt = write (handle, sample, sizeof sample - 1);
  if (byte_cnt != sizeof sample - 1)
    fail ("write() returned %d instead of %zu", byte_cnt, sizeof sample - 1);
}

