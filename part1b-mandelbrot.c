/* 2019-20 Programming Project
	File Name: part1b-mandelbrot.c 
    Name: Piyush Jha
    UID: 3035342691
    Date: 16 October 2019
    Version: 1
    Development Platform: Ubuntu 18.04 VM
    Compilation: gcc part1b-mandelbrot.c -o 1bmandel -l SDL2 -l m
    Usage: ./1bmandel [number of child processes] [number of rows in a task]
    Example Use: ./1bmandel 5 20
    */

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

// Using MSG struct as provided in specification for project
typedef struct message {
    int row_index;
    pid_t child_pid;
    float rowdata[IMAGE_WIDTH];
} MSG;

typedef struct task {
    int start_row;
    int num_of_rows;
} TASK;

// create data PIPE for passing MSG data row by row 
int mpipe[2]; 
// create task PIPE for passing tasks between boss and workers
int tpipe[2];

//SEE IF YOU CAN USE THIS AS A STRUCT TO COUNT THE NUM OF TASKS PROCESSED
pid_t pid[16];

void sigint_handler() {

    printf("Process %d is interrupted by ^C. Bye Bye\n",(int)getpid());
    exit(0);
}

void sigusr1_handler() {
    //close(tpipe[1]);
    printf("Received SIGUSR1\n");
    TASK received;
    if (read(tpipe[0], &received, sizeof(TASK)) < 0){
        printf("Error in reading from Task pipe with pid %d\n", (int)getpid());
    }
    printf("Child received %d\n", received.start_row);
    struct timespec start_compute, end_compute;
    clock_gettime(CLOCK_MONOTONIC, &start_compute);
    // As Child only writes to the PIPE, the reading from PIPE is closed
    //close(mpipe[0]);
    printf("Child(%d) : Start the computation ...\n", (int)getpid());
    // Create an array of struct MSG for storing all the computations done by this child
    MSG *child = malloc(received.num_of_rows*sizeof(MSG));
    if (child == NULL) {
        printf("Out of memory, can't create child MSG struct array!!\n");
        exit(1);
    }
    int st_row_count = 0; //this count is for array of struct message
            
    for (int y=received.start_row; y<received.start_row+received.num_of_rows; y++) {
    // Avoid passing IMAGE_HEIGHT during loop
    if (y >= IMAGE_HEIGHT){
        break;
    }
    child[st_row_count].row_index = y;
    child[st_row_count].child_pid = 0;
    if (st_row_count == (received.num_of_rows-1)){
        child[st_row_count].child_pid = (int)getpid();
    }
    for (int x=0; x<IMAGE_WIDTH; x++) {
        //compute a value for each point c (x, y) in the complex plane and store in array of struct message
        child[st_row_count].rowdata[x] = Mandelbrot(x, y);
    }
    st_row_count += 1;
    }
    // write each row from array of structs to PIPE, hence write ROW BY ROW
    printf("A child reached here with pid %d and st_row_ct %d\n", (int)getpid(), st_row_count);
    for (int i = 0; i < st_row_count; i++){
        if (write(mpipe[1], &child[i], sizeof(MSG)) < 0){
            printf("Error in writing to MSG pipe with pid %d\n", (int)getpid());
        }
        //printf("Child sent %d\n", (int)child[i].child_pid);
        //printf("Process %d with loop ct %d is writing with code %d\n",(int)getpid(),i,resp);
    }
    // Finish computation time recording
    clock_gettime(CLOCK_MONOTONIC, &end_compute);
	float difftime = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;
	printf("Child(%d) :     ... completed. Elapse time = %.3f ms\n", (int)getpid(),difftime);
    
    if (received.start_row+received.num_of_rows >= IMAGE_HEIGHT){
        printf("I CAME TO CLOSE THE WRITE PIPE IN CHILD\n");
    if (close(mpipe[1]) < 0){
        printf("Error in Closing write for Mpipe with pid %d\n", (int)getpid());
    }
    }
    
}

int main( int argc, char* args[] )
{   // check if the number of childs have been passed in the program from command line

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
	
    if (pipe(mpipe) == 0){
        printf("message pipe created\n");
    }
    if (pipe(tpipe) == 0){
        printf("Task pipe created\n");
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
	// Declare variables for workload division among child processes
	float difftime;
    int count = 0;
    //int rows = (IMAGE_HEIGHT / num_child);
    //int vert = 0;
    // create num_child no. of childs as given as input
    while (count < num_child){
        // fork to create a child
        pid[count] = fork();
            if (pid[count] == 0) { 
            
            signal(SIGINT, sigint_handler);
            signal(SIGUSR1, sigusr1_handler);

            if (close(tpipe[1]) < 0){
                printf("Error in Closing write for Tpipe with pid %d\n", (int)getpid());
            }
            if (close(mpipe[0]) < 0){
                printf("Error in Closing read for Mpipe with pid %d\n", (int)getpid());
            }
            printf("Child(%d) : Start up. Wait for task!\n", (int)getpid());
            /* use sigaction to install a signal handler named sigint_handler1 */ 
            
            while (1) {
                sleep(1);
            }
            /*
            // Start recording computation time after child starts working
            struct timespec start_compute, end_compute;
            clock_gettime(CLOCK_MONOTONIC, &start_compute);
            // As Child only writes to the PIPE, the reading from PIPE is closed
            close(mpipe[0]);
            printf("Child(%d) : Start the computation ...\n", (int)getpid());
            // Create an array of struct MSG for storing all the computations done by this child
            MSG *child = malloc(rows*sizeof(MSG));
            if (child == NULL) {
                    printf("Out of memory, can't create child MSG struct array!!\n");
                    exit(1);
            }
            int st_row_count = 0; //this count is for array of struct message
            
            for (int y=vert; y<rows+vert; y++) {
            // Avoid passing IMAGE_HEIGHT during loop
            if (y >= IMAGE_HEIGHT){
                break;
            }
            child[st_row_count].row_index = y;
            for (int x=0; x<IMAGE_WIDTH; x++) {
                //compute a value for each point c (x, y) in the complex plane and store in array of struct message
                child[st_row_count].rowdata[x] = Mandelbrot(x, y);
    	    }
            st_row_count += 1;
            }
            // write each row from array of structs to PIPE, hence write ROW BY ROW
            for (int i = 0; i < st_row_count; i++){
                write(mpipe[1], &child[i], sizeof(MSG));
            }
            st_row_count = 0;
            // Finish computation time recording
            clock_gettime(CLOCK_MONOTONIC, &end_compute);
	        difftime = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;
	        printf("Child(%d) :     ... completed. Elapse time = %.3f ms\n", (int)getpid(),difftime);
            // Terminate this child process
            
            exit(0);
            */
            }
            
        // Increment the start of variable y for the next child by IMAGE_HEIGHT/num_child (rows)
        //vert += rows; 
        count += 1;
    }
    count = 0;
    int r_count = 0;
    // Read from all Child using Wait
     if (close(tpipe[0]) < 0){
            printf("Error in Closing read for Tpipe with pid %d\n", (int)getpid());

        }
    if (close(mpipe[1]) < 0){
            printf("Error in Closing write for Mpipe with pid %d\n", (int)getpid());
    }
    for (int i=0; i < num_child; i++){
        TASK send;
        send.num_of_rows = num_rows;
        send.start_row = r_count;
        write(tpipe[1], &send, sizeof(TASK));
        r_count += num_rows;
    }

    for (int i=0 ; i < num_child; i++){
        sleep(1);
        kill(pid[i],SIGUSR1);
        sleep(1);
    }
    /*
    for (int i=0 ; i < num_child; i++){
        sleep(1);
        kill(pid[i],SIGINT);
        sleep(1);
    }
    */
    //while (count < num_child){
        // This time we only use PIPE for reading and close the write end
        //
        // Loop until PIPE reaches EOF (response from read = 0)
        while (1) {

            MSG receive;
            // read from PIPE row by row
            int response = read(mpipe[0], &receive, sizeof(MSG));
            printf("parent received %d\n", receive.row_index);
            if (response <= 0){
                printf("Error in reading from MSG pipe with pid %d\n", (int)getpid());
                break;
            }
            
            //printf("received response code %d\n", response);
            //printf("receive row %d\n",receive.row_index);
            //printf("receive pid %d\n", receive.child_pid);
            //printf("parent receive rowdata %f\n", receive.rowdata[0]);
            //if (response == 0){
                
            //}
            // checking against any unlinkely garbage value that migt cause segmentation fault
            
            if (receive.row_index >= 0 && receive.row_index < IMAGE_HEIGHT){
                for(int j=0; j<IMAGE_WIDTH; j++){
                // Copy data received from PIPE to pixels array for drawing
                //printf("Parent received row_index %d with rowdata %f \n", receive.row_index, receive.rowdata[j]);
                pixels[receive.row_index*IMAGE_WIDTH+j] = receive.rowdata[j];
            }
            }
            // checking if the received message contains the worker's PID
            if (receive.row_index == IMAGE_HEIGHT-1){
                break;
            }
            int flag = 0;
            for (int i=0 ; i < num_child; i++){
                
                if (pid[i] == receive.child_pid){
                    flag = 1;
                }
            }
            

            if (flag == 1){
                printf("I reached here with r_count %d and IMAGE_HEIGHT %d\n", r_count, IMAGE_HEIGHT);
                if (r_count < IMAGE_HEIGHT){
                    TASK send;
                    send.num_of_rows = num_rows;
                    send.start_row = r_count;
                    if (write(tpipe[1], &send, sizeof(TASK)) < 0){
                        printf("Error in writing to Task pipe with pid %d\n", (int)getpid());
                    }
                    printf("parent sent %d\n", send.start_row);
                    r_count += num_rows;
                    sleep(1);
                    kill(receive.child_pid,SIGUSR1);
                    sleep(1);
                }
                  
            }
            
            
        }

        for (int i=0 ; i < num_child; i++){
            printf("Sending SIGINT NOW\n");
            sleep(1);
            kill(pid[i],SIGINT);
            sleep(1);
        }

        
        // Wait until the child gets terminated
        while (count < num_child){
        pid_t terminated = wait(NULL);
        printf("Child process %d terminated and completed tasks\n",terminated);
        // Do this until all child are terminated
        count += 1;
        }
    //}
    
    
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