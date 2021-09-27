// include
#include <stdlib.h>
#include <mqueue.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>

// global vruntime arr
double *v_runtime_arr;
unsigned long *avg_waiting_time_arr;
int *f_b_count_arr;

/**************************************
  linked list structure - START
***************************************/

struct b_node{
  struct b_node *next;
  long t_index;
  long b_index;
  unsigned long b_length; //ms
  unsigned long gen_time; //ms
};

struct linked_list{
  struct b_node *head;
  struct b_node *tail;
  long l_count;
};

void
linked_list_init(struct linked_list *list){
  list->l_count = 0;
  list->head = NULL;
  list->tail = NULL;
}

int
linked_list_insert(struct linked_list *list, struct b_node *node){
  if (list != NULL){
    if (node != NULL){
      if (list->l_count == 0){
        list->head = node;
        list->tail = node;
      } else {
        list->tail->next = node;
        list->tail = node;
      }

      list->l_count++;
      return 1; //success
    }
  }

  return 0; //fail
}

struct b_node *
linked_list_retrieve(struct linked_list *list, const char *alg){
  if (list->l_count == 0)
    return NULL;

  struct b_node *node;

  if (strcmp("FCFS", alg) == 0){
    node = list->head;
    list->head = list->head->next;
  } else {
    struct b_node *curr;
    if (strcmp("SJF", alg) == 0){
      // find the min node
      node = list->head;
      for (curr = list->head->next; curr != NULL; curr = curr->next){
        if (curr->b_length < node->b_length)
          node = curr;
      }
    } else if (strcmp("PRIO", alg) == 0){
      // find the node with the highest priority
      node = list->head;
      for (curr = list->head->next; curr != NULL; curr = curr->next){
        if (curr->t_index < node->t_index)
          node = curr;
      }
    } else if (strcmp("VRUNTIME", alg) == 0){
      // find the node with the lowest v_runtime
      node = list->head;
      for (curr = list->head->next; curr != NULL; curr = curr->next){
        if (v_runtime_arr[(curr->t_index) - 1] < v_runtime_arr[(node->t_index) - 1])
          node = curr;
      }
    } else {
      return NULL;
    }

    //remove the selected node from the list
    if (node == list->head)
      list->head = list->head->next;
    else{
      for (curr = list->head; curr->next != node; curr = curr->next);
      if (node == list->tail){
        list->tail = curr;
	list->tail->next = NULL;
      }
      curr->next = node->next;
    }
  }

  list->l_count--;
  return node;
}

/**************************************
  linked list structure - END
***************************************/

/**************************************
synchronized linked list with mutex and conditional variables - START
***************************************/

struct sync_linked_list{
  struct linked_list *list;
  pthread_mutex_t sll_mutex;
  pthread_cond_t sll_cond_hasitem;
};

void
sll_add(struct sync_linked_list* sll,  struct b_node *node){
  pthread_mutex_lock(&sll->sll_mutex);

  // critical section - START
  int stat = linked_list_insert(sll->list, node);
  if (stat == 0){
    printf("Linked list insertion error\n");
    exit(1);
  }
  printf("Node with t_index: %li, v_runtime: %f, b_index: %li, and b_length: %lu is added\n", node->t_index, v_runtime_arr[(node->t_index) - 1], node->b_index, node->b_length);
  // cond var signal
  if (sll->list->l_count == 1)
    pthread_cond_signal(&sll->sll_cond_hasitem);
  // critical section - END
  pthread_mutex_unlock(&sll->sll_mutex);
}

struct b_node *
sll_retrieve(struct sync_linked_list *sll, const char *alg){
  struct b_node *node;

  pthread_mutex_lock(&sll->sll_mutex);

  //critical section - START
  //cond var wait
  while (sll->list->l_count == 0)
    pthread_cond_wait(&sll->sll_cond_hasitem, &sll->sll_mutex);

  node = linked_list_retrieve(sll->list, alg);
  if (node == NULL){
    printf("Linked list retrieve operation failed\n");
    exit(1);
  }
  double v_runtime = v_runtime_arr[(node->t_index) - 1];
  printf("Node with t_index: %li, v_runtime: %f, b_index: %li, and b_length: %lu is retrieved\n", node->t_index, v_runtime, node->b_index, node->b_length);
  pthread_mutex_unlock(&sll->sll_mutex);
  return node;
}

/**************************************
synchronized linked list with mutex and conditional variables - END
***************************************/

// globals
struct sync_linked_list *rq;
int N;
int B_count;
int minB;
int avgB;
int minA;
int avgA;
char alg[16];
char prefix[64];

// exponential distribution
double
expo(double lambda, int min_val){
  double random;
  double result;

  do{
    random = rand() / (RAND_MAX + 1.);
    result = -log(1 - random) / lambda;
  } while(result < min_val);
  return result;
}

// get line count
int
line_count(){
  int result = 0;
  int i;

  for (i = 1; i <= N; ++i){
    FILE *fp;
    char str[256];
    char filename[64];
    strcpy(filename, prefix);
    sprintf(str, "-%d.txt", i);
    strcat(filename, str);

    char c;
    int line_count = 0;

    fp = fopen(filename, "r");
    if (fp == NULL){
      perror("File could not be opened\n");
      exit(1);
    }

    while(!feof(fp)){
      c = fgetc(fp);
      if(c == '\n'){
        result++;
        line_count++;
      }
    }
    f_b_count_arr[i - 1] = line_count;
    fclose(fp);
  }

  return result;
}
/**************************************
threads - START
***************************************/

static void *
worker_thread(void *arg){
  long t_index;
  long b_index;
  unsigned long b_length;
  unsigned long gen_time;
  int i;

  struct b_node *node;

  t_index = (long) arg;
  srand(t_index);
  for (i = 0; i < B_count; ++i){
    usleep((unsigned long) (expo((1./avgA), minA) * 1000));
    b_index = (long) (i + 1);
    b_length = (unsigned long) expo((1./avgB), minB);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    gen_time = tv.tv_sec * 1000 + (tv.tv_usec / 1000);

    node = (struct b_node *) malloc(sizeof(struct b_node));
    if (node == NULL){
      perror("Node creation is failed\n");
      exit(1);
    }
    node->next = NULL;
    node->t_index = t_index;
    node->b_index = b_index;
    node->b_length = b_length;
    node->gen_time = gen_time;

    sll_add(rq, node);
  }

  pthread_exit(NULL);
}

static void *
scheduler_thread(void *arg){
  struct b_node *node;
  int i;
  unsigned long exec_time;
  for (i = 0; i < B_count * N; ++i){
    node = sll_retrieve(rq, alg);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    exec_time = tv.tv_sec * 1000 + (tv.tv_usec / 1000);
    avg_waiting_time_arr[(node->t_index) - 1] += (exec_time - node->gen_time);
    usleep(node->b_length * 1000);
    v_runtime_arr[(node->t_index) - 1] += (node->b_length * (0.7 + 0.3 * node->t_index));
    free(node);
  }

  pthread_exit(NULL);
}

static void *
f_worker_thread(void *arg){
  long t_index;
  long b_index;
  unsigned long b_length;
  unsigned long gen_time;
  int i = 0;

  struct b_node *node;

  t_index = (long) arg;

  FILE *fp;
  char str[256];
  char filename[64];
  strcpy(filename, prefix);
  sprintf(str, "-%li.txt", t_index);
  strcat(filename, str);

  fp = fopen(filename, "r");
  if (fp == NULL){
    perror("File could not be opened: %s\n");
    exit(1);
  }

  while (fgets(str, 256, fp) != NULL){
    char *first_str;
    char *second_str;

    first_str = strtok(str, "\t\a\r\n, ");
    second_str = strtok(NULL, "\t\a\r\n, ");

    usleep((unsigned long) (atoi(first_str) * 1000));

    b_index = (long) (i + 1);
    b_length = (unsigned long) atoi(second_str);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    gen_time = tv.tv_sec * 1000 + (tv.tv_usec / 1000);

    node = (struct b_node *) malloc(sizeof(struct b_node));
    if (node == NULL){
      perror("Node creation is failed\n");
      exit(1);
    }
    node->next = NULL;
    node->t_index = t_index;
    node->b_index = b_index;
    node->b_length = b_length;
    node->gen_time = gen_time;

    sll_add(rq, node);
    i++;
  }
  fclose(fp);
  pthread_exit(NULL);
}

static void *
f_scheduler_thread(void *arg){
  struct b_node *node;
  int i;
  unsigned long exec_time;

  for (i = 0; i < B_count; ++i){
    node = sll_retrieve(rq, alg);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    exec_time = tv.tv_sec * 1000 + (tv.tv_usec / 1000);
    avg_waiting_time_arr[(node->t_index) - 1] += (exec_time - node->gen_time);
    usleep(node->b_length * 1000);
    v_runtime_arr[(node->t_index) - 1] += (node->b_length * (0.7 + 0.3 * node->t_index));
    free(node);
  }

  pthread_exit(NULL);
}

/**************************************
threads - END
***************************************/

int
main(int argc, char const **argv) {
  if (argc != 8 && argc != 5){
    printf("\"./schedule <N> <B_count> <minB> <avgB> <minA> <avgA> <ALG>\"\n");
    printf("OR\n");
    printf("\"./schedule <N> <ALG> -f <inprefix>\"\n");
    exit(1);
  }

  printf("START\n");
  // init shared sync linked list
  rq = (struct sync_linked_list *) malloc(sizeof(struct sync_linked_list));
  rq->list = (struct linked_list *) malloc(sizeof(struct linked_list));
  linked_list_init(rq->list);
  pthread_mutex_init(&rq->sll_mutex, NULL);
  pthread_cond_init(&rq->sll_cond_hasitem, NULL);

  N = atoi(argv[1]);
  if (!(N >= 1 && N <= 10)){
    printf("The N value have to be in range [1, 10]\n");
    exit(1);
  }

  v_runtime_arr = (double *) malloc(N * sizeof(double));
  avg_waiting_time_arr = (unsigned long *) malloc(N * sizeof(unsigned long));
  f_b_count_arr = (int *) malloc(N * sizeof(int));
  int index;
  for (index = 0; index < N; ++index){
    v_runtime_arr[index] = 0.0;
    avg_waiting_time_arr[index] = 0;
    f_b_count_arr[index] = 1;
  }

  if (argc == 8){
    B_count = atoi(argv[2]);

    minB = atoi(argv[3]);
    avgB = atoi(argv[4]);

    minA = atoi(argv[5]);
    avgA = atoi(argv[6]);

    strcpy(alg, argv[7]);

    pthread_t w_tids[N];
    pthread_t s_tid;

    int i;
    int stat;

    // worker threads
    for (i = 0; i < N; ++i){
      f_b_count_arr[i] = B_count;
      stat = pthread_create(&w_tids[i], NULL, worker_thread, (void *) (long) (i + 1));
      if (stat != 0){
        perror("Worker thread creation is failed\n");
        exit(1);
      }
    }

    // scheduler thread
    stat = pthread_create(&s_tid, NULL, scheduler_thread, (void *) NULL);
    if (stat != 0){
      perror("Scheduler thread creation is failed\n");
      exit(1);
    }

    // join worker threads
    for (i = 0; i < N; ++i){
      stat = pthread_join(w_tids[i], NULL);
      if (stat != 0){
        perror("Worker thread join is failed\n");
        exit(1);
      }
    }

    // join scheduler thread
    stat = pthread_join(s_tid, NULL);
    if (stat != 0){
      perror("Scheduler thread join is failed\n");
      exit(1);
    }
  } else if (argc == 5){
    strcpy(alg, argv[2]);
    strcpy(prefix, argv[4]);

    B_count = line_count();

    pthread_t w_tids[N];
    pthread_t s_tid;

    int i;
    int stat;

    // worker threads
    for (i = 0; i < N; ++i){
      stat = pthread_create(&w_tids[i], NULL, f_worker_thread, (void *) (long) (i + 1));
      if (stat != 0){
        perror("Worker thread creation is failed\n");
        exit(1);
      }
    }

    // scheduler thread
    stat = pthread_create(&s_tid, NULL, f_scheduler_thread, (void *) NULL);
    if (stat != 0){
      perror("Scheduler thread creation is failed\n");
      exit(1);
    }

    // join worker threads
    for (i = 0; i < N; ++i){
      stat = pthread_join(w_tids[i], NULL);
      if (stat != 0){
        perror("Worker thread join is failed\n");
        exit(1);
      }
    }

    // join scheduler thread
    stat = pthread_join(s_tid, NULL);
    if (stat != 0){
      perror("Scheduler thread join is failed\n");
      exit(1);
    }
  }

  unsigned long tot_avg_waiting_time = 0;
  for (index = 0; index < N; ++index){
    tot_avg_waiting_time += (avg_waiting_time_arr[index] / f_b_count_arr[index]);
    printf("Avg. waiting time for thread %d: %lu\n", (index + 1), (avg_waiting_time_arr[index] / f_b_count_arr[index]));
  }

  printf("Avg. waiting time for %s algorithm with %d thread: %lu\n", alg, N, (tot_avg_waiting_time / N));

  // free & destroy
  pthread_mutex_destroy(&rq->sll_mutex);
  pthread_cond_destroy(&rq->sll_cond_hasitem);

  free(rq->list);
  free(rq);

  free(v_runtime_arr);
  free(avg_waiting_time_arr);
  free(f_b_count_arr);
  printf("END\n");
  return 0;
}
