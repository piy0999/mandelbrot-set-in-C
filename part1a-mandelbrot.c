/* 2019-20 Programming Project
	part1a-mandelbrot.c */

//Using SDL2 and standard IO
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "Mandel.h"
#include "draw.h"

//#define IMAGE_WIDTH 200
//#define IMAGE_HEIGHT 200

typedef struct message {
    int row_index;
    float rowdata[IMAGE_WIDTH];
} MSG;

int main( int argc, char* args[] )
{   
    if (argc != 2) {
        printf("Usage: ./1amandelbrot [No. of Child Processes(int)]");
        return 0;
    }
    char *a = args[1];
    int num_child = atoi(a);
    //data structure to store the start and end times of the whole program
	struct timespec start_time, end_time;
	//get the start time
	clock_gettime(CLOCK_MONOTONIC, &start_time);
	
	//data structure to store the start and end times of the computation
	struct timespec start_compute, end_compute;
	
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
	printf("Start the computation ...\n");
	clock_gettime(CLOCK_MONOTONIC, &start_compute);
    int x, y;
	float difftime;
    int count = 0;
    int rows = (IMAGE_HEIGHT / num_child) + 1;
    int vert = 0;
    int pfd[2];
    pipe(pfd); 
    while (count < num_child){
        pid_t pid = fork();
            if (pid == 0) { 
            close(pfd[0]);
            printf("Child (%d): Start the computation ...\n", (int)getpid());
            MSG child[rows];
            int st_row_count = 0; //this count is for array of struct message
            for (y=vert; y<rows+vert; y++) {
            //printf("Child(%d): y value %d\n",(int)getpid(), y);
            if (y >= IMAGE_HEIGHT){
                break;
            }
            child[st_row_count].row_index = y;
            for (x=0; x<IMAGE_WIDTH; x++) {
			//compute a value for each point c (x, y) in the complex plane
    		// pixels[y*IMAGE_WIDTH+x] = Mandelbrot(x, y);
                child[st_row_count].rowdata[x] = Mandelbrot(x, y);
    	    }
            st_row_count += 1;
            }
            write(pfd[1], child, rows * sizeof(MSG));
            close(pfd[1]);
            exit(0);
            }
        vert += rows; 
        count += 1;
    }
    count = 0;
    while (count < num_child){
        close(pfd[1]);
        MSG receive[rows];
        read(pfd[0], receive, rows * sizeof(MSG));
        pid_t terminated_pid = wait(NULL);
        for (int i=0; i < rows; i++){
            int temp_y = receive[i].row_index;
            if (temp_y >= IMAGE_HEIGHT){
                break;
            }
            for(int j=0; j<IMAGE_WIDTH; j++){
                pixels[temp_y*IMAGE_WIDTH+j] = receive[i].rowdata[j];
            }
        }
        printf("Child (%d) was terminated\n", (int) terminated_pid);
        count += 1;
    }

	clock_gettime(CLOCK_MONOTONIC, &end_compute);
	difftime = (end_compute.tv_nsec - start_compute.tv_nsec)/1000000.0 + (end_compute.tv_sec - start_compute.tv_sec)*1000.0;
	printf(" ... completed. Elapse time = %.3f ms\n", difftime);

	//Report timing
	clock_gettime(CLOCK_MONOTONIC, &end_time);
	difftime = (end_time.tv_nsec - start_time.tv_nsec)/1000000.0 + (end_time.tv_sec - start_time.tv_sec)*1000.0;
	printf("Total elapse time measured by the process = %.3f ms\n", difftime);
	
	printf("Draw the image\n");
	//Draw the image by using the SDL2 library
	DrawImage(pixels, IMAGE_WIDTH, IMAGE_HEIGHT, "Mandelbrot demo", 3000);
	
	return 0;
}