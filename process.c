#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <stdbool.h>

typedef struct value_type {
	int test; // For ll testing.
	int clock_time;
} value_type;

typedef struct node {
	value_type v;
	struct node* next;
} node;

typedef struct ll {
	node* head;
	node* tail;
	int length;
	pthread_mutex_t lock;
} ll;

typedef struct queue_thread_arg {
	ll* q;
	int readfd;
} queue_thread_arg;

typedef struct processing_thread_arg {
	ll* q;
	int* logical_clock_time;
	int writefd1;
	int writefd2;
	int logfd;
	int speed;
} processing_thread_arg;

void push(value_type v, ll* l) {
	node* new_node = malloc(sizeof(node));
	new_node->v = v;
	new_node->next = NULL;
	pthread_mutex_lock(&l->lock);
	if (l->head == NULL) {
		l->head = new_node;
		l->tail = new_node;
	}
	else {
		l->tail->next = new_node;
		l->tail = new_node;
	}
	++l->length;
	pthread_mutex_unlock(&l->lock);
}

node* pop (ll* l) {
	pthread_mutex_lock(&l->lock);
	assert(l->head != NULL);
	if (l->head == l->tail) {
		l->tail = NULL;
	}
	node* ret = l->head;
	l->head = l->head->next;
	--l->length;
	pthread_mutex_unlock(&l->lock);
	return ret;
}

void ll_test(void) {
	ll* test_ll = malloc(sizeof(ll));
	test_ll->head = NULL;
	test_ll->tail = NULL;
	test_ll->length = 0;

	value_type v1;
	v1.test = 4;
	value_type v2;
	v2.test = 5;
	value_type v3;
	v3.test = 6;
	push(v1, test_ll);
	//printf("%d\n", test_ll->length);
	assert(test_ll->length == 1);
	push(v2, test_ll);
	push(v3, test_ll);
	assert(test_ll->length == 3);

	node* tmp = pop(test_ll);
	assert((tmp->v).test == 4);

	node* cur = test_ll->head;
	printf("should print:\n5\n6\n\n");
	while (cur != NULL) {
		printf("%d\n", (cur->v).test);
		cur = cur->next;
	}
	assert(test_ll->length == 2);

	free(test_ll);
	printf("LL test looks good!\n");
}

int max(int x, int y) {
	if (x > y) {
		return x;
	}
	else {
		return y;
	}
}

void* queue_thread(void* arg) {
	queue_thread_arg* qta = (queue_thread_arg*)arg;

	int clock_time;
	while (1) {
		if (read(qta->readfd, &clock_time, 4) > 0) {
			value_type v;
			v.clock_time = clock_time;
			push(v, qta->q);
		}
	}
	return NULL;
}

void* processing_thread(void* arg) {
	processing_thread_arg* pta = (processing_thread_arg*)arg;
	// Open up sockets to the other processes to send msgs
	float speed = (float) pta->speed;
	printf("speed: %f\n", speed);
	while (1) {
		usleep((1.0/speed) * 1000000);
		char* buf = malloc(100); // this will hold the log message
		time_t rawtime;
		struct tm* timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);

		// The queue is empty
		if (pta->q->length == 0) {
			int r = (rand() % 10) + 1;
			if (r == 1 || r == 4) {
				// send to writefd1
				int to_send = *(pta->logical_clock_time);
				write(pta->writefd1, &to_send, 4);
				*(pta->logical_clock_time) = (*(pta->logical_clock_time)) + 1;
				int n = sprintf(buf,
					"System time is: %slogical clock value is: %d\n\n",
					asctime(timeinfo), *(pta->logical_clock_time));
				write(pta->logfd, buf, n);
			}
			else if (r == 2 || r == 5) {
				// send to writefd2
				int to_send = *(pta->logical_clock_time);
				write(pta->writefd1, &to_send, 4);
				*(pta->logical_clock_time) = (*(pta->logical_clock_time)) + 1;
				int n = sprintf(buf,
					"System time is: %slogical clock value is: %d\n",
					asctime(timeinfo), *(pta->logical_clock_time));
				write(pta->logfd, buf, n);
			}
			else if (r == 3 || r == 6) {
				// Send to both writefd1 and writefd2.
				int to_send = *(pta->logical_clock_time);
				write(pta->writefd1, (char*)&to_send, 4);
				write(pta->writefd2, (char*)&to_send, 4);
				*(pta->logical_clock_time) = (*(pta->logical_clock_time)) + 1;
				int n = sprintf(buf,
					"System time is: %slogical clock value is: %d\n",
					asctime(timeinfo), *(pta->logical_clock_time));
				write(pta->logfd, buf, n);
			}
			else {
				// Update the local logical clock and log the
				// internal event, system time, logical clock value.
				*(pta->logical_clock_time) = (*(pta->logical_clock_time)) + 1;
				int n = sprintf(buf,
					"System time is: %slogical clock value is: %d\n",
					asctime(timeinfo), *(pta->logical_clock_time));
				write(pta->logfd, buf, n);
			}
			free(buf);
		}
		else {
			// There is a message in the queue, process it
			node* to_process = pop(pta->q);
			// Update local logical clock time.
			printf("%d %d\n", *(pta->logical_clock_time),
				to_process->v.clock_time);
			*(pta->logical_clock_time) = max(*(pta->logical_clock_time),
				to_process->v.clock_time);
			printf("%d\n", *(pta->logical_clock_time));
			// Write that it received message, global time, length of msg q,
			// current logical clock time.
			int n = sprintf(buf,
				"Received Message! System time is: %slogical clock value is:"
				" %d, length of message queue: %d\n\n",
				asctime(timeinfo), *(pta->logical_clock_time), pta->q->length);
			write(pta->logfd, buf, n);
		}
	}
	return NULL;
}

/*
 * Functions to initialize threads and the data structures we
 * pass into threads.
*/

void init_queue_thread_arg(queue_thread_arg* qta, int readfd, ll* q) {
	qta->readfd = readfd;
	qta->q = q;
}

void init_processing_thread_arg(processing_thread_arg* p, int* lct, int wfd1,
		int wfd2, int lfd, ll* q, int randn) {
	p->logical_clock_time = lct;
	p->writefd1 = wfd1;
	p->writefd2 = wfd2;
	p->logfd = lfd;
	p->q = q;
	p->speed = randn;
}

void init_ll(ll* l) {
	l->head = NULL;
	l->length = 0;
	l->tail = NULL;
	pthread_mutex_init(&l->lock, NULL);
}

void init_thread(int child, int writefd1, int writefd2, int readfd,
		char* logfile, int randn) {
	ll* c_ll = malloc(sizeof(ll));
	init_ll(c_ll);

	int child_logical_clock_time = 0;
	int fd = open(logfile, O_RDWR | O_CREAT, 0777);
	write(fd, "CHILD START\n", 12);

	// Argument to be passed into the processing thread
	processing_thread_arg* p = malloc(sizeof(processing_thread_arg));
	init_processing_thread_arg(p, &child_logical_clock_time,
			writefd1, writefd2, fd, c_ll, randn);

	// Argument to be passed into the queue thread
	queue_thread_arg* qta = malloc(sizeof(queue_thread_arg));
	init_queue_thread_arg(qta, readfd, c_ll);

	// Spin up queue and processing threads and wait for them.
	pthread_t qt;
	pthread_t pt;
	assert(pthread_create(&qt, NULL, queue_thread, (void*)qta) == 0);
	assert(pthread_create(&pt, NULL, processing_thread, (void*)p) == 0);
	pthread_join(pt, NULL);
	pthread_join(qt, NULL);

	free(p);
}

int main (int argc, char** argv) {
	srand((unsigned)time(NULL));

	// Pre-generate random nubmers to avoid bugs.
	int rand1 = (rand() % 6) + 1;
	int rand2 = (rand() % 6) + 1;
	int rand3 = (rand() % 6) + 1;

	// Set up bidirectional socket connections.
	// We can then pass these into the various threads
	// for communication purposes.
	int sv1[2];
	int sv2[2];
	int sv3[2];
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv1) != -1);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv2) != -1);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv3) != -1);

	// Fork off three processes
	pid_t child1 = fork();

	if (child1 == 0) {
		// This child will write into sv2[0] and sv3[0]
		// and read from sv1[1]
		init_thread(2, sv2[0], sv3[0], sv1[1], "child1log.txt", rand1);
	}

	pid_t child2 = fork();

	if (child2 == 0) {
		// This child will write into sv1[0] and sv3[0]
		// and read from sv2[1]
		init_thread(2, sv1[0], sv3[0], sv2[1], "child2log.txt", rand2);
	}

	pid_t child3 = fork();

	if (child3 == 0) {
		// This child will write into sv1[0] and sv2[0]
		// and read from sv3[1]
		init_thread(3, sv1[0], sv2[0], sv3[1], "child3log.txt", rand3);
	}

	waitpid(child1, NULL, 0);
	waitpid(child2, NULL, 0);
	waitpid(child3, NULL, 0);

	return 0;
}
