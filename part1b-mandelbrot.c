/************************************************************* 
* Filename: part1b-mandelbrot.c 
* Student name: Piyush Jha
* Student no.: 3035342691
* Date: Oct 19, 2019
* version: 1.1
* Development platform: Ubuntu 18.04 Cloud VM
* Compilation: gcc part1b-mandelbrot.c -o 1bmandel -l SDL2 -l m
* Usage: ./1bmandel [number of child processes] [number of rows in a task]
* Example Use: ./1bmandel 5 20
*************************************************************/

//Using SDL2 and standard IO
#include <signal.h> 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/resource.h>
#include "Mandel.h"
#include "draw.h"

// Using MSG struct as provided in specification of project
typedef struct message {
    int row_index;
    pid_t child_pid;
    float rowdata[IMAGE_WIDTH];
} MSG;
// Using TASK struct as provided in specification of project
typedef struct task {
    int start_row;
    int num_of_rows;
} TASK;

// create data PIPE for passing MSG data row by row 
int mpipe[2]; 
// create task PIPE for passing tasks between boss and workers
int tpipe[2];

//Arrays to store the PIDs and the number of times these processes work on tasks
int pid[16];
int c_pid[16];

// SIGINT signal handler, exits the child process everytime when called using kill from parent
void sigint_handler() {

    printf("Process %d is interrupted by ^C. Bye Bye\n",(int)getpid());
    exit(0);
}

// SIGUSR1 handler, after being called using kill the child receives a task from task PIPE and performs the madelbrot set computation
// until the num of rows as specified in the command line when starting the program and returns the result row by row to parent
void sigusr1_handler() {
    TASK received;
    if (read(tpipe[0], &received, sizeof(TASK)) < 0){
        printf("Error in reading from Task pipe with pid %d\n", (int)getpid());
    }
    // Start recording the elapse time
    struct timespec start_compute, end_compute;
    clock_gettime(CLOCK_MONOTONIC, &start_compute);
    
    printf("Child(%d) : Start the computation ...\n", (int)getpid());
    // Create a struct MSG for storing the computations done by this child
    MSG child;
            
    for (int y=received.start_row; y<received.start_row+received.num_of_rows; y++) {
    // Avoid passing value > IMAGE_HEIGHT during loop
    if (y >= IMAGE_HEIGHT){
        break;
    }
    child.row_index = y;
    child.child_pid = -1;
    // Add PID to last row of computation
    if (y == ((received.start_row+received.num_of_rows)-1)){
        child.child_pid = (int)getpid();
    }
    for (int x=0; x<IMAGE_WIDTH; x++) {
        //compute a value for each point c (x, y) in the complex plane and store in array of struct message
        child.rowdata[x] = Mandelbrot(x, y);
    }
    if (write(mpipe[1], &child, sizeof(MSG)) < 0){
            printf("Error in writing to MSG pipe with pid %d\n", (int)getpid());
        }
    }
    // write each row from array of structs to PIPE, hence write ROW BY ROW
    // Finish computation time recording
    clock_gettime(CLOCK_MONOTONIC, &end_compute);
	float difftime = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;
	printf("Child(%d) :     ... completed. Elapse time = %.3f ms\n", (int)getpid(),difftime);
    
}

int main( int argc, char* args[] )
{   
    // check if the number of childs and number of rows in a task have been passed in the program from command line
    if (argc != 3) {
        printf("Usage: ./1bmandel [number of child processes] [number of rows in a task]");
        return 0;
    }
    // store the number of child processes from the arguments array
    char *a = args[1];
    int num_child = atoi(a);
    // store the number of rows in a task from the arguments array
    char *b = args[2];
    int num_rows = atoi(b);
    //data structure to store the start and end times of the whole program
	struct timespec start_time, end_time;
	//get the start time
	clock_gettime(CLOCK_MONOTONIC, &start_time);
	
    // Create both the message and task pipes
    if (pipe(mpipe) < 0){
        printf("Error creating MSG pipe\n");
    }
    if (pipe(tpipe) < 0){
        printf("Error creating TASK pipe\n");
    }
    
	//generate mandelbrot image and store each pixel for later display
	//each pixel is represented as a value in the range of [0,1]
	//store the 2D image as a linear array of pixels (in row-major format)
	float * pixels;
	//allocate memory to store the pixels for drawing
	pixels = (float *) malloc(sizeof(float) * IMAGE_WIDTH * IMAGE_HEIGHT);
	if (pixels == NULL) {
		printf("Out of memory!!\n");
		exit(1);
	}
    else{
        
        printf("Start collecting image lines\n"); // As Pixels was successfully created
    
    }
	float difftime;
    int count = 0;

    // create num_child no. of childs as given as input
    while (count < num_child){
        // fork to create a child
        pid[count] = fork();
            if (pid[count] == 0) { 
            // Install the signal handlers for the child
            signal(SIGINT, sigint_handler);
            signal(SIGUSR1, sigusr1_handler);

            // As Child only reads from the TASK PIPE, the writing to PIPE is closed
            if (close(tpipe[1]) < 0){
                printf("Error in Closing write for Tpipe with pid %d\n", (int)getpid());
            }
            // As Child only writes to the MSG PIPE, the reading from PIPE is closed
            if (close(mpipe[0]) < 0){
                printf("Error in Closing read for Mpipe with pid %d\n", (int)getpid());
            }
            printf("Child(%d) : Start up. Wait for task!\n", (int)getpid());
            /* use sigaction to install a signal handler named sigint_handler1 */ 
            // Loop until signals are received
            while (1) {
                ;
            }
            
            }
        // Do this for all the children
        count += 1;
    }
    // Parent process begins
    count = 0;
    // Counting the row of tasks distributed, the start row value for each task
    int r_count = 0;
    // Parent only writes to TASK pipe so close the reading end
     if (close(tpipe[0]) < 0){
            printf("Error in Closing read for Tpipe with pid %d\n", (int)getpid());

        }
    // Parent only reads from the Message pipe so close the writing end 
    if (close(mpipe[1]) < 0){
            printf("Error in Closing write for Mpipe with pid %d\n", (int)getpid());
    }
    // Distribute each of the child one task by writing to Task pipe
    for (int i=0; i < num_child; i++){
        TASK send;
        send.num_of_rows = num_rows;
        send.start_row = r_count;
        write(tpipe[1], &send, sizeof(TASK));
        r_count += num_rows;
    }
    // Send SIGUSR1 signal to each of the child to start computation
    for (int i=0 ; i < num_child; i++){
        kill(pid[i],SIGUSR1);
        sleep(1);
        c_pid[i] = 1;
    }
    // Until all results are not received or the message pipe reaches EOF or leads to an error keep looping
    while (1) {

            MSG receive;
            // read from message PIPE row by row
            int response = read(mpipe[0], &receive, sizeof(MSG));
            // If error in read then break out of loop
            if (response < 0){
                printf("Error in reading from MSG pipe with pid %d\n", (int)getpid());
                break;
            }
            // If EOF in read then break out of loop
            if (response == 0){
                break;
            }
            
            // checking against any unlinkely garbage value that might cause segmentation fault
            
            if (receive.row_index >= 0 && receive.row_index < IMAGE_HEIGHT){
                for(int j=0; j<IMAGE_WIDTH; j++){
                // Copy data received from PIPE to pixels array for drawing
                pixels[receive.row_index*IMAGE_WIDTH+j] = receive.rowdata[j];
            }
            }
            // If all the results have been received (the last row index is returned) then terminate loop
            // This is based on the Atomicity of PIPE as PIPE is a FIFO structure and according to logic last row index woul be the last
            // to be written to PIPE and hence its return should mean all the data has been received
            if (receive.row_index == IMAGE_HEIGHT-1){
                break;
            }
            
            // checking if the received message contains the worker's PID
            int flag = 0;
            for (int i=0 ; i < num_child; i++){
                
                if (pid[i] == receive.child_pid){
                    flag = 1;
                    if (r_count < IMAGE_HEIGHT){
                    c_pid[i] += 1;
                    }
                }
            }
            
            // If the received row contains the PID of the child, it means the child has finished computation and it is idle so 
            // assign it a new task by calling SIGUSR1 and writing to Task PIPE before sending the signal
            if (flag == 1){
                if (r_count < IMAGE_HEIGHT){
                    TASK send;
                    send.num_of_rows = num_rows;
                    send.start_row = r_count;
                    if (write(tpipe[1], &send, sizeof(TASK)) < 0){
                        printf("Error in writing to Task pipe with pid %d\n", (int)getpid());
                    }
                    r_count += num_rows;
                    kill(receive.child_pid,SIGUSR1);
                    sleep(1);
                }
                  
            }
            
            
        }
    // While loop terminates and processing has finished so send SIGINT to each child to terminate the child processes
    for (int i=0 ; i < num_child; i++){
            kill(pid[i],SIGINT);
            sleep(1);
    }

        
    // Wait until all children get terminated
    while (count < num_child){
        pid_t terminated = wait(NULL);
        printf("Child process %d terminated and completed %d tasks\n",terminated,c_pid[count]);
        // Do this until all child are terminated
        count += 1;
    }
    
    printf("All Child processes have completed\n");
    
    // Using getrusage to display the statistics of the runtimes first for the child processes and then the parent process
    struct rusage usg, pusg;
    if ( getrusage(RUSAGE_CHILDREN, &usg) == 0){
        printf("Total time spent by all child processes in user mode = %.3f ms\n", (double) (1000*(usg.ru_utime.tv_sec) + (usg.ru_utime.tv_usec)/1000) );
        printf("Total time spent by all child processes in system mode = %.3f ms\n", (double) (1000*(usg.ru_stime.tv_sec) + (usg.ru_stime.tv_usec)/1000) );

    }
    if (getrusage (RUSAGE_SELF, &pusg) == 0){
        printf("Total time spent by parent process in user mode = %.3f ms\n", (double) (1000*(pusg.ru_utime.tv_sec) + (pusg.ru_utime.tv_usec)/1000));
        printf("Total time spent by parent process in system mode = %.3f ms\n", (double) (1000*(pusg.ru_stime.tv_sec) + (pusg.ru_stime.tv_usec)/1000));

    }
    
	//Report total elapsed time
	clock_gettime(CLOCK_MONOTONIC, &end_time);
	difftime = (end_time.tv_nsec - start_time.tv_nsec)/1000000.0 + (end_time.tv_sec - start_time.tv_sec)*1000.0;
	printf("Total elapse time measured by parent process = %.3f ms\n", difftime);
	
    
	printf("Draw the image\n");
	//Draw the image by using the SDL2 library
	DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 3000);
	
    // free memory of pixels after drawing
    free(pixels);
	return 0;
}