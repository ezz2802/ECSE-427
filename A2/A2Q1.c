#include <time.h>
#include <float.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

static int data = 0;
static int read_count = 0;
static sem_t rw_mutex, mutex;
const int num_reader = 500;
const int num_writer = 10;
static float time_reader = 0.0;
static float time_writer = 0.0;
static float min_read_time = 100.0;
static float max_read_time = 0.0;
static float min_write_time = 100.0;
static float max_write_time = 0.0;

static void *writer(void *arg){
	double time_waited = 0;
	int write_loops = *((int *) arg);
	double sleep_time = (rand()%101)*1000;
	clock_t s, e;
	int c = 0;
	do{
		s = clock();
		sem_wait(&rw_mutex);
		e = clock();
		time_waited = ((double)(e - s))/CLOCKS_PER_SEC;
		data += 10;
		time_writer += time_waited;
		if (time_waited > max_write_time) max_write_time = time_waited;
		if (time_waited < min_write_time) min_write_time = time_waited;
		sem_post(&rw_mutex);
		c++;
		usleep((int)(sleep_time));
	}while (c < write_loops);
	pthread_exit(NULL);
}
static void *reader(void *arg){
	double time_waited1 = 0;
	int read_loops = *((int *) arg);
	double sleep_time2 = (rand()%101)*1000;
	clock_t s2, e2;
	int c2 = 0;
	do{
		s2 = clock();
		sem_wait(&mutex);
		read_count++;
		if (read_count == 1) sem_wait(&rw_mutex);
		sem_post(&mutex);
		e2 = clock();
		time_waited1 = ((double)(e2 - s2))/CLOCKS_PER_SEC;
		int read_data = data;
		sem_wait(&mutex);
		time_reader += time_waited1;
		if (time_waited1 > max_read_time) max_read_time = time_waited1;
		if (time_waited1 < min_read_time) min_read_time = time_waited1;
		read_count--;
		if (read_count == 0) sem_post(&rw_mutex);
		sem_post(&mutex);
		c2++;
		usleep((int)(sleep_time2));
	}while (c2 < read_loops);
	pthread_exit(NULL);
}
int main(int argc, char *argv[]){
	int write_loop = atoi(argv[1]);
    int read_loop = atoi(argv[2]);
	pthread_t readers[num_reader], writers[num_writer];
	sem_init(&rw_mutex, 0, 1);
	sem_init(&mutex, 0, 1);
	int j = 0;
	for (int i = 0; i < num_reader; i++){
		pthread_create(&readers[i], NULL, reader, &read_loop);
		if (i%(num_reader/num_writer) == 0){
			pthread_create(&writers[j], NULL, writer, &write_loop);
			j++;
		}
	}
	int j2 = 0;
	for (int i2 = 0; i2 < num_reader; i2++){
		pthread_join(readers[i2], NULL);
		if (i2%(num_reader/num_writer) == 0){
			pthread_join(writers[j2], NULL);
			j2++;
		}
	}

	printf("The maximum wait time for writer is %f \n", max_write_time);
	printf("The maximum wait time for reader is %f \n\n", max_read_time);
	
	printf("The minimum wait time for writer is %f \n", min_write_time);
	printf("The minimum wait time for reader is %f \n\n", min_read_time);

	printf("The average wait time for writer is %f \n", time_writer/(num_writer*write_loop));
	printf("The average wait time for reader is %f \n\n", time_reader/(num_reader*read_loop));
	exit(0);
}