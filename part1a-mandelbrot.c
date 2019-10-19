/************************************************************* 
* Filename: part1a-mandelbrot.c 
* Student name: Piyush Jha
* Student no.: 3035342691
* Date: Oct 19, 2019
* version: 1.1
* Development platform: Ubuntu 18.04 Cloud VM
* Compilation: gcc part1a-mandelbrot.c -o 1amandel -l SDL2 -l m
* Usage: ./1amandel [number of child processes]
* Example Use: ./1amandel 5
*************************************************************/

//Using SDL2 and standard IO
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
    float rowdata[IMAGE_WIDTH];
} MSG;

int main( int argc, char* args[] )
{   // check if the number of childs have been passed in the program from command line
    if (argc != 2) {
        printf("Usage: ./1amandel [number of child processes]");
        return 0;
    }
    // store the num_child from the arguments array
    char *a = args[1];
    int num_child = atoi(a);
    //data structure to store the start and end times of the whole program
	struct timespec start_time, end_time;
	//get the start time
	clock_gettime(CLOCK_MONOTONIC, &start_time);
	
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
	// Declare variables for workload division among child processes
	float difftime;
    int count = 0;
    int rows = (IMAGE_HEIGHT / num_child);
    int vert = 0;
    // create PIPE for passing data row by row
    int pfd[2];
    int resp = pipe(pfd); 
    if (resp == 0){
        printf("Start collecting image lines\n"); // As Pipe was successfully created
    }
    // create num_child no. of childs as given as input
    while (count < num_child){
        // fork to create a child
        pid_t pid = fork();
            if (pid == 0) { 
            // Start recording computation time after child starts working
            struct timespec start_compute, end_compute;
            clock_gettime(CLOCK_MONOTONIC, &start_compute);
            // As Child only writes to the PIPE, the reading from PIPE is closed
            close(pfd[0]);
            printf("Child(%d) :Start the computation ...\n", (int)getpid());
            // Create an array of struct MSG for storing all the computations done by this child
            MSG child;
            //int st_row_count = 0; //this count is for array of struct message
            
            for (int y=vert; y<rows+vert; y++) {
            // Avoid passing IMAGE_HEIGHT during loop
            if (y >= IMAGE_HEIGHT){
                break;
            }
            child.row_index = y;
            for (int x=0; x<IMAGE_WIDTH; x++) {
                //compute a value for each point c (x, y) in the complex plane and store in array of struct message
                child.rowdata[x] = Mandelbrot(x, y);
    	    }
            write(pfd[1], &child, sizeof(MSG));
            }
            
            // Finish computation time recording
            clock_gettime(CLOCK_MONOTONIC, &end_compute);
	        difftime = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;
	        printf("Child(%d) :     ... completed. Elapse time = %.3f ms\n", (int)getpid(),difftime);
            // Terminate this child process
            exit(0);
            }
        // Increment the start of variable y for the next child by IMAGE_HEIGHT/num_child (rows)
        vert += rows; 
        count += 1;
    }
    count = 0;
    // Read from all Child using Wait
    while (count < num_child){
        // This time we only use PIPE for reading and close the write end
        close(pfd[1]);
        // Loop until PIPE reaches EOF (response from read = 0)
        while (1) {
            MSG receive;
            // read from PIPE row by row
            int response = read(pfd[0], &receive, sizeof(MSG));
            if (response == 0){
                break;
            }
            // checking against any unlinkely garbage value that migt cause segmentation fault
            if (receive.row_index >= 0 && receive.row_index <= IMAGE_HEIGHT){
                for(int j=0; j<IMAGE_WIDTH; j++){
                // Copy data received from PIPE to pixels array for drawing
                pixels[receive.row_index*IMAGE_WIDTH+j] = receive.rowdata[j];
            }
            }
            
        }
        // Wait until the child gets terminated
        wait(NULL);
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