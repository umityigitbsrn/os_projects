#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <math.h>
#include <sys/shm.h>
#include <sys/mman.h>


// Define a name for your shared memory; you can give any name that start with a slash character; it will be like a filename.
#define SHAREDMEM_NAME "/shared_memory"

// Define semaphore names
// One semaphore for mutex
// One semaphore for counting active process
#define SEM_MUTEX "/sem_mutex"
#define SEM_COUNT "/sem_count"

// Define number of active processes allowed
#define MAX_PROCESSES_ALLOWED 10
#define MIN_BLOCK_SIZE 128

// Define semaphore(s)
sem_t *sem_mutex;
sem_t *sem_count;

// Define your stuctures and variables.
// to keep information about the available blocks
struct available_node {
	long head;
	long tail;
};

void *process_add; // starting add of shared memory for a specific process
void *starting_add; // starting add of the shared segment size

// shared memory init
int sbmem_init(int segmentsize)
{
	// get the order
	int order = (int) (log(segmentsize / MIN_BLOCK_SIZE) / log(2));

	// total size
	int total_size = segmentsize + ((order + 1) * sizeof(struct available_node)) + 2*sizeof(int); // segment + available_list + count + order

	// to map shared memory into address space of the process
	void *shm_start;

	// file descriptor
	int fd;

	// if already a shared segment with the same name
	// int ret;

	// if we want to get info about shared memory
	struct stat sbuf;

	// previously opened shared segment
	shm_unlink(SHAREDMEM_NAME);
	// if (ret == 0) {
	// 	// printf("Previously opened shared memory segment has been closed.\n");
	// }

	// open shared memory
	fd = shm_open(SHAREDMEM_NAME, O_RDWR | O_CREAT, 0660);

	if (fd == -1) {
		perror("Error while creating a shared memory segment.\n");
		return -1;
	}
	// else { // DELETE_PRINT
	// 	printf("Shared memory segment has been initialized.\n");
	// }

	// set the size of the shared memory
	ftruncate(fd, total_size);

	// info about shared memory
	fstat(fd, &sbuf);

	shm_start = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	// file descriptor can be closed
	close(fd);

	*(int *) (shm_start) = 0; // count
	*(int *) (shm_start + sizeof(int)) = order;

	// init available_list
	void *tmp_add = shm_start + 2 * sizeof(int);
	for (int i = 0; i <= order; ++i){
		struct available_node av;
		if (i != order){
			av.head = -1;
			av.tail = -1;
		}
		else {
			av.head = 0; // distance
			av.tail = 0;
		}
		*(struct available_node *) (tmp_add) = av;
		tmp_add += sizeof(struct available_node);
	}

	*(int *) (tmp_add) = 1; //tag
	*(int *) (tmp_add + sizeof(int)) = order; //order
	*(long *) (tmp_add) = -1; //distance next
	*(long *) (tmp_add) = -1; //distance prev

	// delete existing ones
	sem_unlink(SEM_MUTEX);
	sem_unlink(SEM_COUNT);

	// create new semaphores
	sem_mutex = sem_open(SEM_MUTEX, O_RDWR | O_CREAT, 0660, 1);
	sem_count = sem_open(SEM_COUNT, O_RDWR | O_CREAT, 0660, 1);

  return 0;
}

int sbmem_remove()
{
	// return from shm_unlink()
	int ret;

	// // remove semaphores
	int err = sem_unlink(SEM_MUTEX);
	if (err == -1){
		perror("Unlink semophore operation failed.");
		exit(1);
	}
	err = sem_unlink(SEM_COUNT);
	if (err == -1){
		perror("Unlink semophore operation failed.");
		exit(1);
	}

	ret = shm_unlink(SHAREDMEM_NAME);

	if (ret == 0) {
	// 	printf("Shared segment is removed.\n");
		return 0;
	}
	else {
		perror("Error while removing the shared segment.\n");
		return -1;
	}
}

int sbmem_open()
{
	int fd;

	// open shared memory
	fd = shm_open(SHAREDMEM_NAME, O_RDWR, 0660);

	if (fd == -1) {
		perror("Error while creating a shared memory segment.\n");
		return -1;
	}

	struct stat sbuf;

	fstat(fd, &sbuf);
	process_add = mmap(NULL, sbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);

	int order  = *(int *) (process_add + sizeof(int));
	starting_add = process_add + 2*sizeof(int) + (order + 1) * sizeof(struct available_node);

	sem_mutex = sem_open(SEM_MUTEX, O_RDWR);
	sem_count = sem_open(SEM_COUNT, O_RDWR);

	sem_wait(sem_count);
	int count = *(int *) (process_add);
	if (count >= MAX_PROCESSES_ALLOWED)
		return -1;
	(*(int *) process_add)++;
	sem_post(sem_count);

  return (0);
}


void *sbmem_alloc (int size)
{
	sem_mutex = sem_open(SEM_MUTEX, O_RDWR);
	sem_wait(sem_mutex);
	// allocate memory of size n = 2^k >= reqsize
	// on success, return pointer to allocated space, else NULL

	// finding available order
	int req_size = size + 2 * sizeof(int);
	int order;
	for (order = 0; req_size > (MIN_BLOCK_SIZE * (1 << order)); ++order); //requested order
	void *available_list = process_add + 2*sizeof(int);

	int j; // finding the min available order
	int max_order = *(int *) (process_add + sizeof(int));
	for (j = order; (*(struct available_node *) (available_list + j * sizeof(struct available_node))).head == -1 && j < max_order; ++j);


	long head_tmp = (*(struct available_node *) (available_list + j * sizeof(struct available_node))).head;
	long tail_tmp = (*(struct available_node *) (available_list + j * sizeof(struct available_node))).tail;

	// remove from head
	void *available_block = starting_add + head_tmp;
	if (tail_tmp == head_tmp){
		tail_tmp = -1;
		head_tmp = -1;
		(*(struct available_node *) (available_list + j * sizeof(struct available_node))).head = head_tmp;
		(*(struct available_node *) (available_list + j * sizeof(struct available_node))).tail = tail_tmp;
	}
	else{
		head_tmp = *(long *) (starting_add + head_tmp + 2 * sizeof(int)); // head = head->next
		(*(struct available_node *) (available_list + j * sizeof(struct available_node))).head = head_tmp;
		*(long *) (available_block + 2 * sizeof(int)) = -1; // rem->next = NULL
		*(long *) (starting_add + head_tmp + 2 * sizeof(int) + sizeof(long)) = -1; // head->back = NULL
	}

	// printf("The block with address: %p is removed from available list index: %d\n", available_block, j);

	while (j != order){ // need to be splitted
		j--; // decrease order
		head_tmp = (*(struct available_node *) (available_list + j * sizeof(struct available_node))).head;
		tail_tmp = (*(struct available_node *) (available_list + j * sizeof(struct available_node))).tail;
		void *splitted_add = available_block + (MIN_BLOCK_SIZE * (1 << j)); // next block

		*(int *) (splitted_add) = 1;
		*(int *) (splitted_add + sizeof(int)) = j;
		*(long *) (splitted_add + 2*sizeof(int)) = -1;
		*(long *) (splitted_add + 2*sizeof(int) + sizeof(long)) = -1;

		// add splitted and available block to the available list -- sorted
		// if head is null, new head becomes splitted address
	  	if (head_tmp == -1 && tail_tmp == -1) { //empty
	    	(*(struct available_node *) (available_list + j * sizeof(struct available_node))).head = splitted_add - starting_add;
				(*(struct available_node *) (available_list + j * sizeof(struct available_node))).tail = splitted_add - starting_add;
			}
    	else {
				long next_block;
				for (next_block = head_tmp; next_block != -1 && splitted_add - starting_add > next_block; next_block = *(long *)(next_block + 2 * sizeof(int)));

				if (next_block == -1){ // next to the tail
					*(long *) (tail_tmp + 2 * sizeof(int)) = splitted_add - starting_add; // tail->next = splitted
					*(long *) (splitted_add + 2*sizeof(int) + sizeof(long)) = tail_tmp; //splitted->back = tail
					(*(struct available_node *) (available_list + j * sizeof(struct available_node))).tail = splitted_add - starting_add; //tail = splitted
				}
				else if (next_block == head_tmp){
					*(long *) (splitted_add + 2*sizeof(int)) = head_tmp; // splitted->next = head
					*(long *)(head_tmp + 2 * sizeof(int) + sizeof(long)) = splitted_add - starting_add; //head->back = splitted
					(*(struct available_node *) (available_list + j * sizeof(struct available_node))).head = splitted_add - starting_add;
				}
				else {
					long prev = *(long *)(starting_add + next_block + 2 * sizeof(int) + sizeof(long)); //prev = next_block->back
					*(long *)(splitted_add + 2 * sizeof(int)) = next_block; // splitted->next = next_block
					*(long *)(starting_add + next_block + 2 * sizeof(int) + sizeof(long)) = splitted_add - starting_add; //next_block->back = splitted
					*(long *)(starting_add + prev + 2 * sizeof(int)) = splitted_add - starting_add; // prev->next = splitted
					*(long *)(splitted_add + 2 * sizeof(int) + sizeof(long)) = prev; // splitted->back = prev
				}
   	}

	 	// printf("The block with address: %p is added to available list index: %d\n", splitted_add, j);
	}

	*(int *) available_block = 0;
	*(int *) (available_block + sizeof(int)) = order;

	sem_post(sem_mutex);

	return available_block + 2 * sizeof(int);
}


void sbmem_free (void *p)
{
	sem_mutex = sem_open(SEM_MUTEX, O_RDWR);
	sem_wait(sem_mutex);
	void *block_add = p - 2*sizeof(int);
	int order_of_block = *(int *) (block_add + sizeof(int));
	int size_block = MIN_BLOCK_SIZE * (1 << order_of_block);

	void *available_list = process_add + 2*sizeof(int);

	// remove if block exist
	if (*(int *) (block_add) == 1){
		// remove existing block from available list
		if (block_add - starting_add == (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head){
				if ((*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).tail == (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head){
					(*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).tail = -1;
					(*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head = -1;
			}
			else{
				(*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head = *(long *) (starting_add + (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head + 2 * sizeof(int)); // head = head->next
				*(long *) (block_add + 2 * sizeof(int)) = -1; // buddy->next = NULL
				*(long *) (starting_add + (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head + 2 * sizeof(int) + sizeof(long)) = -1; // head->back = NULL
			}
		}
		else if (block_add - starting_add == (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).tail){
			if ((*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).tail == (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head){
				(*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).tail = -1;
				(*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head = -1;
			}
			else{
				(*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).tail = *(long *) (starting_add + (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).tail + 2 * sizeof(int) + sizeof(long)); // tail = tail->prev
				*(long *) (block_add + 2 * sizeof(int) + sizeof(long)) = -1; // buddy->prev = NULL
				*(long *) (starting_add + (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).tail + 2 * sizeof(int)) = -1; // tail->next = NULL
			}
		}
		else {
			long buddy_next = *(long *) (block_add + 2 * sizeof(int));
			long buddy_prev = *(long *) (block_add + 2 * sizeof(int) + sizeof(long));

			*(long *) (starting_add + buddy_prev + 2 * sizeof(int)) = buddy_next; //buddy_prev->next = buddy_next
			*(long *) (starting_add + buddy_next + 2 * sizeof(int) + sizeof(long)) = buddy_prev; //buddy_next->prev = buddy_prev
			*(long *) (block_add + 2 * sizeof(int)) = -1; // buddy->next = NULL
			*(long *) (block_add + 2 * sizeof(int) + sizeof(long)) = -1; // buddy->prev = NULL
		}
	}

	*(int *) (block_add) = 0;

	// finding buddy add
	int buddy_num = (block_add - starting_add) / size_block;
	void *buddy_add;
	if (buddy_num % 2 == 0)
		buddy_add = block_add + size_block;
	else
		buddy_add = block_add - size_block;

	// can buddy and block be merged
	int buddy_order = *(int *) (buddy_add + sizeof(int));
	int buddy_tag = *(int *) (buddy_add);
	if (buddy_order == order_of_block && buddy_tag == 1){ // can be merged
			// merge add
			void *merge_add;
			if (buddy_num % 2 == 0)
				merge_add = block_add;
			else
				merge_add = buddy_add;

			// remove buddy block from available list
			if (buddy_add == starting_add + (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head){
				if ((*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).tail == (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head){
					(*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).tail = -1;
					(*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head = -1;
				}
				else{
					(*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head = *(long *) (starting_add + (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head + 2 * sizeof(int)); // head = head->next
					*(long *) (buddy_add + 2 * sizeof(int)) = -1; // buddy->next = NULL
					*(long *) (starting_add + (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head + 2 * sizeof(int) + sizeof(long)) = -1; // head->back = NULL
				}
			}
			else if (buddy_add == starting_add + (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).tail){
				if ((*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).tail == (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head){
					(*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).tail = -1;
					(*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head = -1;
				}
				else{
					(*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).tail = *(long *) (starting_add + (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).tail + 2 * sizeof(int) + sizeof(long)); // tail = tail->prev
					*(long *) (buddy_add + 2 * sizeof(int) + sizeof(long)) = -1; // buddy->prev = -1
					*(long *) (starting_add + (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).tail + 2 * sizeof(int)) = -1; // tail->next = -1
				}
			}
			else {
				long buddy_next = *(long *) (buddy_add + 2 * sizeof(int));
				long buddy_prev = *(long *) (buddy_add + 2 * sizeof(int) + sizeof(long));

				*(long *) (starting_add + buddy_prev + 2 * sizeof(int)) = buddy_next; //buddy_prev->next = buddy_next
				*(long *) (starting_add + buddy_next + 2 * sizeof(int) + sizeof(long)) = buddy_prev; //buddy_next->prev = buddy_prev
				*(long *) (buddy_add + 2 * sizeof(int)) = -1; // buddy->next = -1
				*(long *) (buddy_add + 2 * sizeof(int) + sizeof(long)) = -1; // buddy->prev = -1
			}

			*(int *) (merge_add) = 1;
			*(int *) (merge_add + sizeof(int)) = ++buddy_order;
			*(long *) (merge_add + 2*sizeof(int)) = -1;
			*(long *) (merge_add + 2*sizeof(int) + sizeof(long)) = -1;

			// add merged and available block to the available list -- sorted
			// if head is null, new head becomes splitted address
			if ((*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head == -1 && (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).tail == -1) { //empty
					(*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head = merge_add - starting_add;
					(*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).tail = merge_add - starting_add;
			}
			else {
				long next_block;
				for (next_block = (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head; next_block != -1 && merge_add - starting_add > next_block; next_block = *(long *)(starting_add + next_block + 2 * sizeof(int)));
				if (next_block == -1){ // next to the tail
					*(long *)(starting_add + (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).tail + 2 * sizeof(int)) = merge_add - starting_add; // tail->next = merged
					*(long *) (merge_add + 2*sizeof(int) + sizeof(long)) = (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).tail; //merged->back = tail
					(*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).tail = merge_add - starting_add; //tail = merged
				}
				else if (next_block == (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head){
					*(long *) (merge_add + 2*sizeof(int)) = (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head; // merged->next = head
					*(long *)(starting_add + (*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head + 2 * sizeof(int) + sizeof(long)) = merge_add - starting_add; //head->back = merged
					(*(struct available_node *) (available_list + buddy_order * sizeof(struct available_node))).head = merge_add - starting_add; // head = merged
				}
				else {
					long prev = *(long *)(starting_add + next_block + 2 * sizeof(int) + sizeof(long)); //prev = next_block->back
					*(long *)(merge_add + 2 * sizeof(int)) = next_block; // merged->next = next_block
					*(long *)(starting_add + next_block + 2 * sizeof(int) + sizeof(long)) = merge_add - starting_add; //next_block->back = merged
					*(long *)(starting_add + prev + 2 * sizeof(int)) = merge_add - starting_add; // prev->next = merged
					*(long *)(merge_add + 2 * sizeof(int) + sizeof(long)) = prev; // merged->back = prev
				}
		 }
		 // printf("The block with address: %p is merged and added to available list index: %d\n", merge_add, buddy_order);
		 sem_post(sem_mutex);
		 sbmem_free(merge_add + 2*sizeof(int));
	}
	else { // cannot merged
		*(int *) (block_add) = 1;
		*(int *) (block_add + sizeof(int)) = order_of_block;
		*(long *) (block_add + 2*sizeof(int)) = -1;
		*(long *) (block_add + 2*sizeof(int) + sizeof(long)) = -1;

		// add merged and available block to the available list -- sorted
		// if head is null, new head becomes splitted address
		if ((*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head == -1 && (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).tail == -1) { //empty
				(*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head = block_add - starting_add;
				(*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).tail = block_add - starting_add;
		}
		else {
			long next_block;
			for (next_block = (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head; next_block != -1 && block_add > (starting_add + next_block); next_block = *(long *)(starting_add + next_block + 2 * sizeof(int)));
			if (next_block == -1){ // next to the tail
				*(long *)(starting_add + (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).tail + 2 * sizeof(int)) = block_add - starting_add; // tail->next = merged
				*(long *) (block_add + 2*sizeof(int) + sizeof(long)) = (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).tail; //merged->back = tail
				(*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).tail = block_add - starting_add; //tail = merged
			}
			else if (next_block == (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head){
				*(long *) (block_add + 2*sizeof(int)) = (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head; // merged->next = head
				*(long *)(starting_add + (*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head + 2 * sizeof(int) + sizeof(long)) = block_add - starting_add; //head->back = merged
				(*(struct available_node *) (available_list + order_of_block * sizeof(struct available_node))).head = block_add - starting_add; // head = merged
			}
			else {
				long prev = *(long *)(starting_add + next_block + 2 * sizeof(int) + sizeof(long)); //prev = next_block->back
				*(long *)(block_add + 2 * sizeof(int)) = next_block; // merged->next = next_block
				*(long *)(starting_add + next_block + 2 * sizeof(int) + sizeof(long)) = block_add - starting_add; //next_block->back = merged
				*(long *)(starting_add + prev + 2 * sizeof(int)) = block_add - starting_add; // prev->next = merged
				*(long *)(block_add + 2 * sizeof(int) + sizeof(long)) = prev; // merged->back = prev
			}
		}
		// printf("The block with address: %p is added to available list index: %d without merging\n", block_add, order_of_block);
	}
	sem_post(sem_mutex);
}

int sbmem_close()
{
	sem_count = sem_open(SEM_COUNT, O_RDWR);
	sem_wait(sem_count);
	(*(int *) process_add)--;
	sem_post(sem_count);

	int fd = shm_open(SHAREDMEM_NAME, O_RDWR, 0660);

	if (fd == -1) {
		perror("Error while creating a shared memory segment.\n");
		return -1;
	}

	struct stat sbuf;

	fstat(fd, &sbuf);
	close(fd);
	munmap(process_add, sbuf.st_size);


	// when process is done using the library, this function will be called
	// shared segment will be unmapped from virtual addr. space of process
	// if process wants to use again, call sbmem_open() again
	// previously allocated space by the process does not need to be deallocated
	// it is OPTIONAL

    return (0);
}
