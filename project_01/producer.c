#include <stdio.h>
#include <stdlib.h>

#define UPPER 61
#define LOWER 0

int main(int argc, char const *argv[]) {
  /* code */
  char *alphanum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  for (int i = 0; i < atoi(argv[1]); i++){
    int random = (rand() % (UPPER - LOWER + 1)) + LOWER;
    printf("%c", alphanum[random]);
  }
  return 0;
}
