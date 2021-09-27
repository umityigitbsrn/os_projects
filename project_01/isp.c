#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#define MAX_ARG_SIZE 64

void without_pipe(char ***arg_arr, int *arg_size, char *lineptr){
    char **arg_arr_f = *arg_arr;
    *arg_size = 0;

    char *tmp = strtok(lineptr, " \t\a\r\n");
    while(tmp != NULL){
      arg_arr_f[(*arg_size)] = tmp;
      (*arg_size)++;
      tmp = strtok(NULL, " \t\a\r\n");
    }
    arg_arr_f[(*arg_size)] = NULL;
}

int main(int argc, char const *argv[]) {
  /* code */
  if (argc != 3){
    printf("The number of arguements is not appropriate!\n");
    exit(1);
  }

  int byte_size = atoi(argv[1]);
  int mode = atoi(argv[2]);

  char *lineptr = NULL;
  size_t line_size = 0;

  const char *RED="\033[0;31m";
  const char *NC="\033[0m";
  const char *BLUE="\033[0;34m";
  const char *GREEN="\033[0;32m";

  while(1){
    struct timeval start_tv;
    struct timeval end_tv;

    printf("%sisp$:%s ", RED, NC);
    ssize_t success_read = getline(&lineptr, &line_size, stdin);
    if (success_read == -1){
      perror("Error in getline function");
      exit(1);
    }

    gettimeofday(&start_tv, NULL);
    if (strchr(lineptr, '|') == NULL){ // command does not have pipe
      pid_t child01;
      char **arg_arr = malloc(MAX_ARG_SIZE * sizeof(char *));
      int arg_size;

      without_pipe(&arg_arr, &arg_size, lineptr);

      child01 = fork();
      if (child01 < 0){
        perror("Child cannot be created - wout pipe");
        exit(1);
      }

      if (child01 == 0){
        execvp(arg_arr[0], arg_arr);
        perror("Error in execution - wout pipe");
        exit(1);
      }

      waitpid(child01, NULL, 0);
      free(arg_arr);
    }
    else {
      char **line_arr = malloc(MAX_ARG_SIZE * 2 * sizeof(char *));
      int line_arg_size;
      without_pipe(&line_arr, &line_arg_size, lineptr);

      char *f_arg_arr[MAX_ARG_SIZE];
      char *s_arg_arr[MAX_ARG_SIZE];

      int f_arg_size = 0;
      int s_arg_size = 0;
      int index = 0;
      while(strcmp(line_arr[index], "|") != 0){
        f_arg_arr[f_arg_size] = line_arr[index];
        f_arg_size++;
        index++;
      }

      index++;

      while(index < line_arg_size){
        s_arg_arr[s_arg_size] = line_arr[index];
        s_arg_size++;
        index++;
      }

      f_arg_arr[f_arg_size] = NULL;
      s_arg_arr[s_arg_size] = NULL;

      if (mode == 1){
        int pipe01[2];

        pid_t child01 = 0;
        pid_t child02 = 0;

        int pipe_stat = pipe(pipe01);
        if (pipe_stat == -1){
          perror("Pipe creation is failed");
          exit(1);
        }

        child01 = fork();

        if (child01 != 0)
          child02 = fork();

        if (child01 != 0 && child02 != 0){
          close(pipe01[0]);
          close(pipe01[1]);
        }
        else if (child01 == 0 && child02 == 0){ //child01
            close(pipe01[0]);
            if (dup2(pipe01[1], 1) == -1){
              perror("dup2 function for child01 failed");
              exit(1);
            }
            close(pipe01[1]);
            execvp(f_arg_arr[0], f_arg_arr);
            perror("exec for child01 is failed");
            exit(1);
        }
        else if (child01 != 0 && child02 == 0){ //child02
          close(pipe01[1]);
          if (dup2(pipe01[0], 0) == -1){
            perror("dup2 function for child02 failed");
            exit(1);
          }
          close(pipe01[0]);
          execvp(s_arg_arr[0], s_arg_arr);
          perror("exec for child02 is failed");
          exit(1);
        }

        waitpid(child01, NULL, 0);
        waitpid(child02, NULL, 0);
      }
      else if (mode == 2){
        int byte_count = 0;
        int read_count = 0;
        int write_count = 0;

        char *buff = malloc(byte_size * sizeof(char));

        int pipe01[2];
        int pipe02[2];

        pid_t child01 = 0;
        pid_t child02 = 0;

        int pipe_stat_01 = pipe(pipe01);
        if (pipe_stat_01 == -1){
          perror("First pipe creation is failed");
          exit(1);
        }

        int pipe_stat_02 = pipe(pipe02);
        if (pipe_stat_02 == -1){
          perror("Second pipe creation is failed");
          exit(1);
        }

        child01 = fork();

        if (child01 != 0)
          child02 = fork();

        if (child01 == 0 && child02 == 0){ //child01
            close(pipe02[0]);
            close(pipe02[1]);
            close(pipe01[0]);
            if (dup2(pipe01[1], 1) == -1){
              perror("dup2 function for child01 failed");
              exit(1);
            }
            close(pipe01[1]);
            execvp(f_arg_arr[0], f_arg_arr);
            perror("exec for child01 is failed");
            exit(1);
        }
        else if (child02 != 0){ //parent
          close(pipe01[1]);
          close(pipe02[0]);
          ssize_t success;
          while((success = read(pipe01[0], buff, byte_size))){
            write(pipe02[1], buff, success);
            byte_count += success;
            read_count++;
            write_count++;
          }
          read_count++;
          close(pipe01[0]);
          close(pipe02[1]);
        }
        else if (child02 == 0){ //child02
          close(pipe01[0]);
          close(pipe01[1]);
          close(pipe02[1]);
          if (dup2(pipe02[0], 0) == -1){
            perror("dup2 function for child02 failed");
            exit(1);
          }
          close(pipe02[0]);
          execvp(s_arg_arr[0], s_arg_arr);
          perror("exec for child02 is failed");
          exit(1);
        }

        waitpid(child01, NULL, 0);
        waitpid(child02, NULL, 0);
        printf("%scharacter-count:%s %d\n", BLUE, NC, byte_count);
        printf("%sread-syscall-count:%s %d\n", BLUE, NC, read_count);
        printf("%swrite-syscall-count:%s %d\n", BLUE, NC, write_count);
        free(buff);
      }

      free(line_arr);
    }
    gettimeofday(&end_tv, NULL);
    double elapsed = (end_tv.tv_sec - start_tv.tv_sec) + (end_tv.tv_usec - start_tv.tv_usec) / 1000000.0;
    printf("%sElapsed time:%s %lf\n", GREEN, NC, elapsed);
  }

  //should be freed
  free(lineptr);
  return 0;
}
