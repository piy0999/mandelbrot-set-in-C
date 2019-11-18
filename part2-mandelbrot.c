/************************************************************* 
* Filename: part2-mandelbrot.c 
* Student name: Piyush Jha
* Student no.: 3035342691
* Date: Nov 18, 2019
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

// Array of struct TASK which acts as the buffer for Boss Worker Interaction 
TASK task_buffer[10];
// Declare mutex for thread synchronisation
pthread_mutex_t mv;
// Declare condition Variables to check if the buffer is full or empty
pthread_cond_t notFULL, notEMPTY;
// initialise buffer count and total number of buffers
int buff_count = 0;
int num_buffers;


//Array to store the number of tasks processed by each worker
int task_count[16];

//generate mandelbrot image and store each pixel for later display
//each pixel is represented as a value in the range of [0,1]
//store the 2D image as a linear array of pixels (in row-major format)
float * pixels;

// Declare consumer function for each worker thread
void *consumer (void *thdata) {
    // get thread ID
    int id = *((int *) thdata);
    // Loop until receive termination tasks
    while (1){
    // Enable Mutex Lock
    pthread_mutex_lock(&mv); 
    // Check if the buffer is not empty
    while (buff_count == 0){
        // wait for TASK to be added to the empty buffer pool
        pthread_cond_wait(&notEMPTY, &mv);
    }
    // Buffer is definitely not empty now so get a TASK from buffer
    TASK received;
    received = task_buffer[0];
    // Remove the task from the TASK buffer
    for(int i=0; i<buff_count-1; i++)
    {
        task_buffer[i] = task_buffer[i + 1];
    }
    // Reduce the counter for the buffer after removing the TASK
    buff_count -= 1;
    // Signal waiting threads as buffer is not full
    pthread_cond_signal(&notFULL);
    // release the mutex lock
    pthread_mutex_unlock(&mv);
    // Exit the thread process if received a terminating TASK
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
    // Increment the number of tasks performed against this thread ID
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
    // store the number of worker threads from the arguments array
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
    // Store worker thread IDs in array
    int worker_IDs[num_threads];
	// Create worker threads and call the consumer function using each thread
    for (int i = 0; i < num_threads; i++) {
	worker_IDs[i] = i;
    task_count[i] = 0;
	pthread_create(&worker_thread[i], NULL, consumer, (void *) (&worker_IDs[i]));
    }
    // Counting the row of tasks distributed, the start row value for each task
    int r_count = 0;

    // Loop until all tasks have been put into the TASK buffer
    while (r_count < IMAGE_HEIGHT){
        // Create a TASK to be sent to TASK Buffer pool
        TASK send;
        send.num_of_rows = num_rows;
        send.start_row = r_count;
        send.terminate = 0;
        r_count += num_rows;
        // Enable the mutex lock
        pthread_mutex_lock(&mv); 
        // Check if the current count of elements in buffer is equal to total number of buffers
        while (buff_count == num_buffers){
            // Wait for TASK to be consumed from the Full Buffer pool
            pthread_cond_wait(&notFULL, &mv);
        }
        // Add TASK to the Buffer Pool as the Buffer pool is definitely not full
        task_buffer[buff_count] = send;
        // Increment the buffer_counter as one more task is added to the Buffer Pool
        buff_count += 1;
        // Signal waiting threads that the Buffer is not empty
        pthread_cond_signal(&notEMPTY);
        // Release the Mutex Lock
        pthread_mutex_unlock(&mv);
        }
    
    // Loop until the total number of working threads to add terminating task as all Tasks have been already sent into the TASK buffer
    // Uses the same mechanism as described in the loop above only changes the send.terminate variable to 1 to indicate terminating TASK for the worker thread
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

    // Wait for all the worker threads to be terminated
    for (int i = 0; i < num_threads; i++) {
	    pthread_join(worker_thread[i], NULL);
        printf("Worker thread %d has terminated and completed %d tasks\n", i, task_count[i]);
    }

    
    printf("All worker threads have terminated\n");
    
    // Using getrusage to display the statistics of the runtimes for the master process and the worker threads
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