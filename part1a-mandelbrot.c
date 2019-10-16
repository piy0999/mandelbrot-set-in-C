/* 2019-20 Programming Project
	part1a-mandelbrot.c */

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

//#define IMAGE_WIDTH 10
//#define IMAGE_HEIGHT 10

typedef struct message {
    int row_index;
    float rowdata[IMAGE_WIDTH];
} MSG;

int main( int argc, char* args[] )
{   
    if (argc != 2) {
        printf("Usage: ./1amandelbrot [number of child processes]");
        return 0;
    }
    char *a = args[1];
    int num_child = atoi(a);
    //data structure to store the start and end times of the whole program
	struct timespec start_time, end_time;
	//get the start time
	clock_gettime(CLOCK_MONOTONIC, &start_time);
	
	//data structure to store the start and end times of the computation
	
	//generate mandelbrot image and store each pixel for later display
	//each pixel is represented as a value in the range of [0,1]
	//store the 2D image as a linear array of pixels (in row-major format)
	float * pixels;
	
	//allocate memory to store the pixels
	pixels = (float *) malloc(sizeof(float) * IMAGE_WIDTH * IMAGE_HEIGHT);
	if (pixels == NULL) {
		printf("Out of memory!!\n");
		exit(1);
	}
	//compute the mandelbrot image
	//keep track of the execution time - we are going to parallelize this part
	//printf("Start the computation ...\n");
	float difftime;
    int count = 0;
    int rows = (IMAGE_HEIGHT / num_child);
    int vert = 0;
    int pfd[2];
    int resp = pipe(pfd); 
    if (resp == 0){
        printf("start collecting image lines\n"); //As Pipe was successfully created
    }
    while (count < num_child){
        pid_t pid = fork();
            if (pid == 0) { 
            struct timespec start_compute, end_compute;
            clock_gettime(CLOCK_MONOTONIC, &start_compute);
            close(pfd[0]);
            printf("Child(%d) :Start the computation ...\n", (int)getpid());
            MSG *child = malloc(rows*sizeof(MSG));
            if (child == NULL) {
                    printf("Out of memory, can't create child MSG struct array!!\n");
                    exit(1);
            }
            int st_row_count = 0; //this count is for array of struct message
            // NEED TO FIND A BETTER WAY TO DIVIDE BLOCKS FOR COMPUTATION
            //printf("st_row_count1 = %d\n", st_row_count);
            for (int y=vert; y<rows+vert; y++) {
            //printf("Child(%d): y value %d and st_row_count is %d\n",(int)getpid(), y,st_row_count);
            if (y >= IMAGE_HEIGHT){
                break;
            }
            child[st_row_count].row_index = y;
            for (int x=0; x<IMAGE_WIDTH; x++) {
			//compute a value for each point c (x, y) in the complex plane
    		// pixels[y*IMAGE_WIDTH+x] = Mandelbrot(x, y)
                child[st_row_count].rowdata[x] = Mandelbrot(x, y);
                //printf("x=%d , y=%d , val=%.2f, st_rc=%d\n", x,y,child[st_row_count].rowdata[x],st_row_count);
    	    }
            st_row_count += 1;
            }
            //printf("st_row_count2 = %d \n", st_row_count);
            for (int i = 0; i < st_row_count; i++){
                write(pfd[1], &child[i], sizeof(MSG));
            }
            //printf("Reached here with PID %d \n", (int) getpid());
            st_row_count = 0;
            //close(pfd[1]);
            clock_gettime(CLOCK_MONOTONIC, &end_compute);
	        difftime = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;
	        printf("Child(%d) :     ... completed. Elapse time = %.3f ms\n", (int)getpid(),difftime);
            exit(0);
            }
        vert += rows; 
        count += 1;
    }
    count = 0;
    int prev = -1;
    while (count < num_child){
        close(pfd[1]);
        while (1) {
            MSG receive;
            int response = read(pfd[0], &receive, sizeof(MSG));
            if (response == 0){
                break;
            }
            if (receive.row_index >= 0 && receive.row_index <= IMAGE_HEIGHT){
                for(int j=0; j<IMAGE_WIDTH; j++){
                //printf("rowdata for row_y %d is received as %.2f\n", receive.row_index , receive.rowdata[j]);
                pixels[receive.row_index*IMAGE_WIDTH+j] = receive.rowdata[j];
            }
            }
            
        }
        pid_t terminated_pid = wait(NULL);
        count += 1;
    }
    printf("All Child processes have completed\n");
    struct rusage usg, pusg;
    if ( getrusage(RUSAGE_CHILDREN, &usg) == 0){
        
        printf("Total time spent by all child processes in user mode = %.3f ms\n", (double) (1000*(usg.ru_utime.tv_sec) + (usg.ru_utime.tv_usec)/1000) );
        printf("Total time spent by all child processes in system mode = %.3f ms\n", (double) (1000*(usg.ru_stime.tv_sec) + (usg.ru_stime.tv_usec)/1000) );

    }
    if (getrusage (RUSAGE_SELF, &pusg) == 0){
        printf("Total time spent by parent process in user mode = %.3f ms\n", (double) (1000*(pusg.ru_utime.tv_sec) + (pusg.ru_utime.tv_usec)/1000));
        printf("Total time spent by parent process in system mode = %.3f ms\n", (double) (1000*(pusg.ru_stime.tv_sec) + (pusg.ru_stime.tv_usec)/1000));

    }

	//Report timing
	clock_gettime(CLOCK_MONOTONIC, &end_time);
	difftime = (end_time.tv_nsec - start_time.tv_nsec)/1000000.0 + (end_time.tv_sec - start_time.tv_sec)*1000.0;
	printf("Total elapse time measured by the parent process = %.3f ms\n", difftime);
	
	printf("Draw the image\n");
	//Draw the image by using the SDL2 library
	DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 3000);
	
    free(pixels);
	return 0;
}