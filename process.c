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
	// TODO: this is what's going to be the value in the ll
	// which gets put on the queue.
	int test;
	int clock_time;
} value_type;

typedef struct node {
	value_type v;
	struct node* next;
} node;

typedef struct ll {
	node* head;
	int length;
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
} processing_thread_arg;

void push(value_type v, ll* l) {
	node* new_node = malloc(sizeof(node));
	new_node->v = v;
	new_node->next = l->head;
	l->head = new_node;
	++l->length;
}

node* pop (ll* l) {
	assert(l->head != NULL);
	node* ret = l->head;
	l->head = l->head->next;
	--l->length;
	return ret;
}

void ll_test(void) {
	ll* test_ll = malloc(sizeof(ll));
	test_ll->head = NULL;
	test_ll->length = 0;

	value_type v1;
	v1.test = 4;
	value_type v2;
	v2.test = 5;
	value_type v3;
	v3.test = 6;
	push(v1, test_ll);
	assert(test_ll->length == 1);
	push(v2, test_ll);
	push(v3, test_ll);

	node* tmp = pop(test_ll);
	assert((tmp->v).test == 6);

	node* cur = test_ll->head;
	printf("should print:\n5\n4\n\n");
	while (cur != NULL) {
		printf("%d\n", (cur->v).test);
		cur = cur->next;
	}
	assert(test_ll->length == 2);

	free(test_ll); // this is a mem leak but who cares.
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

void init_queue_thread_arg(queue_thread_arg* qta, int readfd, ll* q) {
	qta->readfd = readfd;
	qta->q = q;
}

/*

On each clock cycle, if there is a message in the message queue for the machine
(remember, the queue is not running at the same cycle speed) the virtual machine
should take one message off the queue, update the local logical clock, and write
that it received a message, the global time (gotten from the system), the length
of the message queue, and the logical clock time.

If there is no message in the queue, the virtual machine should generate a
random number in the range of 1-10, and

if the value is 1, send to one of the other machines a message that is the
	local logical clock time, update it’s own logical clock, and update the
	log with the send, the system time, and the logical clock time
if the value is 2, send to the other virtual machine a message that is the
	local logical clock time, update it’s own logical clock, and update the log
	with the send, the system time, and the logical clock time.
if the value is 3, send to both of the other virtual machines a message that is
	the logical clock time, update it’s own logical clock, and update the log
	with the send, the system time, and the logical clock time.
if the value is other than 1-3, treat the cycle as an internal event; update
	the local logical clock, and log the internal event, the system time, and
	the logical clock value.
*/

void* processing_thread(void* arg) {
	processing_thread_arg* pta = (processing_thread_arg*)arg;
	// Open up sockets to the other processes to send msgs
	float speed = (float)((rand() % 6) + 1);
	while (1) {
		usleep((1.0/speed) * 1000000);
		char* buf = malloc(100); // this will hold the log message
		time_t rawtime;
		struct tm* timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		if (pta->q->length == 0) {
			// The queue is empty

			int r = (rand() % 10) + 1;
			if (r == 1) {
				// send to writefd1
				int to_send = *(pta->logical_clock_time);
				write(pta->writefd1, &to_send, 4);
				*(pta->logical_clock_time) = (*(pta->logical_clock_time)) + 1;
				int n = sprintf(buf,
					"System time is: %s, logical clock value is: %d\n",
					asctime(timeinfo), *(pta->logical_clock_time));
				write(pta->logfd, buf, n);
			}
			else if (r == 2) {
				// send to writefd2
				int to_send = *(pta->logical_clock_time);
				write(pta->writefd1, &to_send, 4);
				*(pta->logical_clock_time) = (*(pta->logical_clock_time)) + 1;
				int n = sprintf(buf,
					"System time is: %s, logical clock value is: %d\n",
					asctime(timeinfo), *(pta->logical_clock_time));
				write(pta->logfd, buf, n);
			}
			else if (r == 3) {
				// send to both writefd1 and writefd2
				int to_send = *(pta->logical_clock_time);
				write(pta->writefd1, (char*)&to_send, 4);
				write(pta->writefd2, (char*)&to_send, 4);
				*(pta->logical_clock_time) = (*(pta->logical_clock_time)) + 1;
				int n = sprintf(buf,
					"System time is: %s, logical clock value is: %d\n",
					asctime(timeinfo), *(pta->logical_clock_time));
				write(pta->logfd, buf, n);
			}
			else {
				// update the local logical clock
				// log the internal event, system time, logical clock value
				*(pta->logical_clock_time) = (*(pta->logical_clock_time)) + 1;
				int n = sprintf(buf,
					"System time is: %s, logical clock value is: %d\n",
					asctime(timeinfo), *(pta->logical_clock_time));
				write(pta->logfd, buf, n);
			}
			free(buf);
		}
		else {
			// There is a message in the queue, process it
			node* to_process = pop(pta->q);
			// update local logical clock time
			*(pta->logical_clock_time) = max(*(pta->logical_clock_time),
				to_process->v.clock_time);
			// write that it received message, global time, length of msg q,
			// current logical clock time
			int n = sprintf(buf,
				"Received Message! System time is: %s, logical clock value is:"
				" %d, length of message queue: %d\n",
				asctime(timeinfo), *(pta->logical_clock_time), pta->q->length);
			write(pta->logfd, buf, n);
		}
	}
	return NULL;
}

void init_processing_thread_arg(processing_thread_arg* p, int* lct, int wfd1,
		int wfd2, int lfd, ll* q) {
	p->logical_clock_time = lct;
	p->writefd1 = wfd1;
	p->writefd2 = wfd2;
	p->logfd = lfd;
	p->q = q;
}

int main (int argc, char** argv) {
	srand((unsigned)time(NULL));

	// Set up bidirectional socket connections.
	// We can then pass these into the various threads
	// for communication purposes.
	int sv1[2];
	int sv2[2];
	int sv3[2];

    char s[] = "hello process 2";

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv1) != -1);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv2) != -1);
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv3) != -1);

	// Test the very simple linked list implementation
	ll_test();

	// Fork off three processes
	pid_t child1 = fork();

	if (child1 == 0) {
		// This child will write into sv2[0] and sv3[0]
		// and read from sv1[1]
		// for proper hygiene, do some closes here
		// close(sv2[1]);
		// close(sv3[1]);
		// close(sv1[0]);
		write(sv2[0], s, strlen(s));
		int child1_logical_clock_time = 0;
		ll* c1_ll = malloc(sizeof(ll));
		c1_ll->head = NULL;
		c1_ll->length = 0;
		int fd1 = open("child1log.txt", O_RDWR | O_CREAT, 0777);
		write(fd1, "CHILD 1 START\n", 15);

		// Argument to be passed into the processing thread
		processing_thread_arg* p = malloc(sizeof(processing_thread_arg));
		init_processing_thread_arg(p, &child1_logical_clock_time,
				sv2[0], sv3[0], fd1, c1_ll);

		// Argument to be passed into the queue thread
		queue_thread_arg* qta = malloc(sizeof(queue_thread_arg));
		init_queue_thread_arg(qta, sv1[1], c1_ll);

		// Create and start threads
		pthread_t qt1;
		pthread_t pt1;
		assert(pthread_create(&qt1, NULL, queue_thread, (void*)qta) == 0);
		assert(pthread_create(&pt1, NULL, processing_thread, (void*)p) == 0);
		pthread_join(pt1, NULL);
		pthread_join(qt1, NULL);

		free(p);
	}

	pid_t child2 = fork();

	if (child2 == 0) {
		// This child will write into sv1[0] and sv3[0]
		// and read from sv2[1]

		// close(sv1[1]);
		// close(sv3[1]);
		// close(sv2[0]);
		ll* c2_ll = malloc(sizeof(ll));
		c2_ll->head = NULL;
		c2_ll->length = 0;
		//char* buf = malloc(100);
		//read(sv2[1], buf, strlen(s));
		int child2_logical_clock_time = 0;
		int fd2 = open("child2log.txt", O_RDWR | O_CREAT, 0777);
		write(fd2, "CHILD 2 START\n", 15);
		// write(fd2, "recv:", 6);
		// write(fd2, buf, strlen(s));

		// Argument to be passed into the processing thread
		processing_thread_arg* p = malloc(sizeof(processing_thread_arg));
		init_processing_thread_arg(p, &child2_logical_clock_time,
				sv1[0], sv3[0], fd2, c2_ll);

		// Argument to be passed into the queue thread
		queue_thread_arg* qta = malloc(sizeof(queue_thread_arg));
		init_queue_thread_arg(qta, sv2[1], c2_ll);

		pthread_t qt2;
		pthread_t pt2;
		assert(pthread_create(&qt2, NULL, queue_thread, (void*)qta) == 0);
		assert(pthread_create(&pt2, NULL, processing_thread, (void*)p) == 0);
		pthread_join(pt2, NULL);
		pthread_join(qt2, NULL);

		free(p);
	}

	pid_t child3 = fork();

	if (child3 == 0) {
		// This child will write into sv1[0] and sv2[0]
		// and read from sv3[1]
		// close(sv1[1]);
		// close(sv2[1]);
		// close(sv3[0]);
		ll* c3_ll = malloc(sizeof(ll));
		c3_ll->head = NULL;
		c3_ll->length = 0;
		int child3_logical_clock_time = 0;
		int fd3 = open("child3log.txt", O_RDWR | O_CREAT, 0777);
		write(fd3, "CHILD 3 START\n", 15);

		// Argument to be passed into the processing thread
		processing_thread_arg* p = malloc(sizeof(processing_thread_arg));
		init_processing_thread_arg(p, &child3_logical_clock_time,
				sv1[0], sv2[0], fd3, c3_ll);

		// Argument to be passed into the queue thread
		queue_thread_arg* qta = malloc(sizeof(queue_thread_arg));
		init_queue_thread_arg(qta, sv3[1], c3_ll);

		pthread_t qt3;
		pthread_t pt3;
		assert(pthread_create(&qt3, NULL, queue_thread, (void*)qta) == 0);
		assert(pthread_create(&pt3, NULL, processing_thread, (void*)p) == 0);
		pthread_join(pt3, NULL);
		pthread_join(qt3, NULL);

		free(p);
	}

	waitpid(child1, NULL, 0);
	waitpid(child2, NULL, 0);
	waitpid(child3, NULL, 0);

	return 0;
}
