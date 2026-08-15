#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "kernel/fs.h"
#include "user/user.h"

#define MAP_FAILED ((char *) -1)

char *testname = "???";

void err(char *why) {
  printf("mmapsharedtest: %s failed: %s, pid=%d\n", testname, why, getpid());
  exit(1);
}

// create test file with 1 page of 'a's
void make_shared_file(const char *f) {
  int i;
  char buf[BSIZE];

  unlink(f);
  int fd = open(f, O_WRONLY | O_CREATE);
  if(fd < 0) err("open");

  memset(buf, 'a', BSIZE);
  for(i = 0; i < PGSIZE / BSIZE; i++) {
    if(write(fd, buf, BSIZE) != BSIZE) err("write");
  }
  if(close(fd) < 0) err("close");
}

void test_basic_shared(void) {
  int fd;
  const char *f = "shared.test";

  printf("test basic map_shared\n");
  testname = "basic_map_shared";

  make_shared_file(f);
  fd = open(f, O_RDWR);
  if(fd < 0) err("open");

  char *p = mmap(0, PGSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(p == MAP_FAILED) err("mmap");

  if(p[0] != 'a') err("verify content");

  p[500] = 'x'; // modify pattern

  if(munmap(p, PGSIZE) < 0) err("munmap failed");

  close(fd);

  fd = open(f, O_RDONLY);
  if(fd < 0) err("reopen file");

  char buf[BSIZE];
  if(read(fd, buf, BSIZE) != BSIZE) err("read back failed");

  if(buf[500] != 'x') err("content not persisted");

  close(fd);
  unlink(f);

  printf("basic map_shared: ok\n");
}

void test_shared_pages(void) {
  int fd, pid, status;
  const char *f = "shared_task";

  printf("test shared physical pages\n");
  testname = "shared_pages";

  make_shared_file(f);

  fd = open(f, O_RDWR);
  if(fd < 0) err("open file");

  char *p1 = mmap(0, PGSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(p1 == MAP_FAILED) err("parent mmap failed");

  if(p1[0] != 'a') err("initial content wrong");

  printf("parent mapped %p ok\n", p1);

  pid = fork();
  if(pid < 0) err("fork failed");

  if(pid == 0) {
    char *p2 = mmap(0, PGSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(p2 == MAP_FAILED) {
      printf("child mmap failed - mmap_shared_task not implemented\n");
      exit(1);
    }

    printf("child mapped %p\n", p2);
    if(p2[0] != 'a') exit(1);
    p2[100] = 'b';
    p2[200] = 'c';

    sleep(10);
    if(p2[300] == 'd') {
      printf("child: shared page success\n");
      munmap(p2, PGSIZE);
      exit(0);
    } else {
      munmap(p2, PGSIZE);
      exit(2);
    }
  } 
  else {
    sleep(1);
    if(p1[100] == 'b' && p1[200] == 'c') {
      printf("parent: shared page success\n");
      p1[300] = 'd';
      wait(&status);
      if(status == 0) printf("shared_task: passed\n");
      else if(status == 2 * 256) printf("shared_task: not implemented\n");
      else printf("shared_task: failed\n");
    } 
    else {
      p1[300] = 'd';
      wait(&status);
      printf("shared_task: not implemented\n");
    }
    munmap(p1, PGSIZE);
    close(fd);
    unlink(f);
  }
}

void test_mmap_exists(void) {
  printf("test mmap syscall exists\n");
  testname = "mmap_exists";
  if(mmap((void*)0xdeadbeef, 0, 0, 0, -1, 0) == MAP_FAILED) printf("mmap syscall error handled\n");
  else err("mmap error failed");
  printf("mmap syscall: ok\n");
}

int main(int argc, char *argv[]) {
  printf("mmapsharedtest start\n");
  test_mmap_exists();
  test_basic_shared();
  test_shared_pages();
  printf("mmapsharedtest done\n");
  exit(0);
}
