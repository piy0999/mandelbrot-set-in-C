/************************************************************* 
* Filename: part2-mandelbrot.c 
* Student name: Piyush Jha
* Student no.: 3035342691
* Date: Nov 13, 2019
* version: 1.1
* Development platform: Ubuntu 18.04 Cloud VM
* Compilation: gcc part2-mandelbrot.c -o part2-mandelbrot -l SDL2 -l m -lpthread 
* Usage: ./part2-mandelbrot [number of workers] [number of rows in a task] [number of buffers]
* Example Use: ./part2-mandelbrot 5 20 10
*************************************************************/

//Using SDL2 and standard IO
#include <signal.h> 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/resource.h>
#include "Mandel.h"
#include "draw.h"


// Using TASK struct as provided in specification of project
typedef struct task {
    int start_row;
    int num_of_rows;
    int terminate;
} TASK;
 
TASK task_buffer[10];
pthread_mutex_t mv;
pthread_cond_t notFULL, notEMPTY;
int buff_count = 0;
int num_buffers;


//Arrays to store the PIDs and the number of times these processes work on tasks
int task_count[16];

//generate mandelbrot image and store each pixel for later display
//each pixel is represented as a value in the range of [0,1]
//store the 2D image as a linear array of pixels (in row-major format)
float * pixels;


void *consumer (void *thdata) {
    int id = *((int *) thdata);
    while (1){
    pthread_mutex_lock(&mv); 
    while (buff_count == 0){
        pthread_cond_wait(&notEMPTY, &mv);
    }
    TASK received;
    received = task_buffer[0];
    for(int i=0; i<buff_count-1; i++)
    {
        task_buffer[i] = task_buffer[i + 1];
    }
    buff_count -= 1;
    pthread_cond_signal(&notFULL);
    pthread_mutex_unlock(&mv);
    if (received.terminate == 1){
        break;
    }
    // Start recording the elapse time
    struct timespec start_compute, end_compute;
    clock_gettime(CLOCK_MONOTONIC, &start_compute);
    
    printf("Worker(%d) : Start the computation ...\n", id);
    // Create a struct MSG for storing the computations done by this child
            
    for (int y=received.start_row; y<received.start_row+received.num_of_rows; y++) {
    // Avoid passing value > IMAGE_HEIGHT during loop
    if (y >= IMAGE_HEIGHT){
        break;
    }
    for (int x=0; x<IMAGE_WIDTH; x++) {
        //compute a value for each point c (x, y) in the complex plane and store in array of struct message
        pixels[y*IMAGE_WIDTH+x] = Mandelbrot(x, y);
    }
    }
    // write each row from array of structs to PIPE, hence write ROW BY ROW
    // Finish computation time recording
    clock_gettime(CLOCK_MONOTONIC, &end_compute);
	float difftime = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;
	printf("Worker(%d) :     ... completed. Elapse time = %.3f ms\n", id,difftime);
    task_count[id] += 1;
    }
    return NULL;

}

int main( int argc, char* args[] )
{   
    // check if all arguments have been passed from command line
    if (argc != 4) {
        printf("Usage: ./part2-mandelbrot [number of workers] [number of rows in a task] [number of buffers]");
        return 0;
    }
    // store the number of threads from the arguments array
    char *a = args[1];
    int num_threads = atoi(a);
    // store the number of rows in a task from the arguments array
    char *b = args[2];
    int num_rows = atoi(b);
    // store the number of buffers from the arguments array
    char *c = args[3];
    num_buffers = atoi(c);
    //data structure to store the start and end times of the whole program
	struct timespec start_time, end_time;
	//get the start time
	clock_gettime(CLOCK_MONOTONIC, &start_time);

    //allocate memory to store the pixels for drawing
    pixels = (float *) malloc(sizeof(float) * IMAGE_WIDTH * IMAGE_HEIGHT);
    if (pixels == NULL) {
	printf("Out of memory!!\n");
	exit(1);
    }

   
    float difftime;

    
    pthread_t * worker_thread = (pthread_t *) malloc(num_threads * sizeof(pthread_t));
    
    int worker_IDs[num_threads];
	
    for (int i = 0; i < num_threads; i++) {
	worker_IDs[i] = i;
    task_count[i] = 0;
	pthread_create(&worker_thread[i], NULL, consumer, (void *) (&worker_IDs[i]));
    }
    // Counting the row of tasks distributed, the start row value for each task
    int r_count = 0;

    while (r_count < IMAGE_HEIGHT){
        TASK send;
        send.num_of_rows = num_rows;
        send.start_row = r_count;
        send.terminate = 0;
        r_count += num_rows;
        pthread_mutex_lock(&mv); 
        
        while (buff_count == num_buffers){
            pthread_cond_wait(&notFULL, &mv);
        }
        task_buffer[buff_count] = send;
        
        buff_count += 1;
        pthread_cond_signal(&notEMPTY);
        pthread_mutex_unlock(&mv);
        }
    
    for (int i = 0; i < num_threads; i++) {
	    TASK send;
        send.num_of_rows = 0;
        send.start_row = 0;
        send.terminate = 1;
        r_count += num_rows;
        pthread_mutex_lock(&mv);
        while (buff_count == num_buffers){
            pthread_cond_wait(&notFULL, &mv);
        }
        task_buffer[buff_count] = send;
        buff_count += 1;
        pthread_cond_signal(&notEMPTY);
        pthread_mutex_unlock(&mv);
    }


    for (int i = 0; i < num_threads; i++) {
	    pthread_join(worker_thread[i], NULL);
        printf("Worker thread %d has terminated and completed %d tasks\n", i, task_count[i]);
    }

    
    printf("All worker threads have terminated\n");
    
    // Using getrusage to display the statistics of the runtimes first for the child processes and then the parent process
    struct rusage pusg;
    if (getrusage (RUSAGE_SELF, &pusg) == 0){
        printf("Total time spent by process and its threads in user mode = %.3f ms\n", (double) (1000*(pusg.ru_utime.tv_sec) + (pusg.ru_utime.tv_usec)/1000));
        printf("Total time spent by process and its threads in system mode = %.3f ms\n", (double) (1000*(pusg.ru_stime.tv_sec) + (pusg.ru_stime.tv_usec)/1000));

    }
    
	//Report total elapsed time
	clock_gettime(CLOCK_MONOTONIC, &end_time);
	difftime = (end_time.tv_nsec - start_time.tv_nsec)/1000000.0 + (end_time.tv_sec - start_time.tv_sec)*1000.0;
	printf("Total elapse time measured by the process = %.3f ms\n", difftime);
	
	printf("Draw the image\n");
	//Draw the image by using the SDL2 library
	DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 3000);
	
    // free memory of pixels after drawing
    free(pixels);
	return 0;
}