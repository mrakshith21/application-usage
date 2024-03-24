#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#define MAX_LINES 100 // Maximum lines to read and print
#define MAX_LINE_LENGTH 200 // Maximum characters per line

#define INTERVAL 5 // Default interval in seconds
#define DEFAULT_DISPLAY_OPTION 1 // Default display option: 1 (Yes)

int main(int argc, char *argv[]) {

    char previous_content[MAX_LINES][MAX_LINE_LENGTH + 1]; // Store previous content
    memset(previous_content, 0, sizeof(previous_content)); // Initialize array with null characters

    
    int lines_printed=0;

    int interval = INTERVAL;
    int display_all = DEFAULT_DISPLAY_OPTION;

    // Check if command-line arguments are provided
    if (argc >= 2) {
        interval = atoi(argv[1]); // Set interval from command line
    }

    if (argc >= 3) {
        display_all = atoi(argv[2]); // Set display option from command line
    }
    
    	int flag=0;
    
        while (1) {
        
        
        int lines=0;

	// Erase previous content by printing newlines
	if(flag==1){
		
		for (int i = 0; i < lines_printed; i++) {
		    printf("\033[1A\033[K"); // Move cursor up and clear the line
		}
        
        }
        
        
        

        // Read and display contents of /proc/app_running_time
        FILE *file = fopen("/proc/app_running_time", "r");
        if (file) {
            
            int c;
            char current_line[MAX_LINE_LENGTH + 1];
            while (fgets(current_line, MAX_LINE_LENGTH + 1, file) != NULL && lines < MAX_LINES) {
                // Remove newline character from current line
                current_line[strcspn(current_line, "\n")] = '\0';

                // Store current line as previous content
                strcpy(previous_content[lines], current_line);

                lines++;
            }
            
            fclose(file);
            
           // if(flag==1){
            
            	    // Move cursor to start of previous content
		//    for (int i = 0; i < lines_printed; i++) {
		  //      printf("\033[1A"); // Move cursor up
		    //}
            
           //}

            lines_printed = 0;

            // Print the new content
            for (int i = 0; i < lines; i++) {
            	
            	if(strstr(previous_content[i], "APPLICATION")){
            	
            		printf("%s\n", previous_content[i]);
            		lines_printed++;
            	}
                else if (display_all == 1 || strstr(previous_content[i], "Yes")) {
                    printf("%s\n", previous_content[i]);
                    lines_printed++;
                }
            }
        } else {
            perror("Error reading /proc/app_running_time");
        }
        
        flag=1;

        fflush(stdout); // Flush output buffer
        sleep(INTERVAL); // Wait for specified interval
    }
    return 0;
}

