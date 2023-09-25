#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

const int nonblock = 1;

void exittest(void) {
  printf("exit test\n");

  for (int i = 0; i < 3; i++) {
    int pid = fork();
    if (pid == 0) {
      sleep(10);
      exit(0);
    }
  }

  // exit(0);
  // printf("exit test OK\n");
}

int main(int argc, char *argv[]) {
  exittest();
  exit(0);
}