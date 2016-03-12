#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

struct queue_thread_arg {
	// TODO: anything we want to pass to the queue thr
};

struct processing_thread_arg {
	// TODO: anything we want to pass to processing thr
};

void* queue_thread(void* arg) {

	return NULL;
}

void* processing_thread(void* arg) {

	return NULL;
}

int main (int argc, char** argv) {

	// Fork off three processes
	pid_t child1 = fork();

	if (child1 == 0) {
		int fd1 = open("child1log.txt", O_RDWR | O_CREAT, 0777);
		write(fd1, "CHILD 1 START\n", 15);
		pthread_t qt1;
		pthread_t pt1;
		assert(pthread_create(&qt1, NULL, queue_thread, NULL) == 0);
		assert(pthread_create(&pt1, NULL, processing_thread, NULL) == 0);
		pthread_join(pt1, NULL);
		pthread_join(qt1, NULL);
	}

	pid_t child2 = fork();

	if (child2 == 0) {
		int fd2 = open("child2log.txt", O_RDWR | O_CREAT, 0777);
		write(fd2, "CHILD 2 START\n", 15);
		pthread_t qt2;
		pthread_t pt2;
		assert(pthread_create(&qt2, NULL, queue_thread, NULL) == 0);
		assert(pthread_create(&pt2, NULL, processing_thread, NULL) == 0);
		pthread_join(pt2, NULL);
		pthread_join(qt2, NULL);
	}

	pid_t child3 = fork();

	if (child3 == 0) {
		int fd3 = open("child3log.txt", O_RDWR | O_CREAT, 0777);
		write(fd3, "CHILD 3 START\n", 15);
		pthread_t qt3;
		pthread_t pt3;
		assert(pthread_create(&qt3, NULL, queue_thread, NULL) == 0);
		assert(pthread_create(&pt3, NULL, processing_thread, NULL) == 0);
		pthread_join(pt3, NULL);
		pthread_join(qt3, NULL);
	}

	return 0;
}