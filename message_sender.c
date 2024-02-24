#include "message_slot.h"

#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>      /* open */
#include <unistd.h>     /* exit */
#include <stdio.h>
#include <errno.h>

int perror_exit_1();

int perror_exit_1(){
    perror("");
    exit(1);
}

int main(int argc, char *argv[]) {
    char *filePath;
    unsigned int channelId;
    char *message;
    int fileDescriptor;

    if (argc == 4) {
        filePath = argv[1];
        channelId = atoi(argv[2]);
        message = argv[3];

        // Attempt to open the specified file
        fileDescriptor = open(filePath, O_RDWR);
        if (fileDescriptor == -1) {
            perror_exit_1(); // Custom error handling function
        }

        // Set the message slot channel
        if (ioctl(fileDescriptor, MSG_SLOT_CHANNEL, channelId) == -1){
            perror_exit_1(); // Error handling
        }

        // Write the message to the specified channel
        if (write(fileDescriptor, message, strlen(message)) < 0){
            perror_exit_1(); // Error handling
        }

        // Close the file descriptor
        if (close(fileDescriptor) < 0){
            perror_exit_1(); // Error handling
        }

        exit(0); // Successful execution
    }
    else {
        // Set error for invalid arguments
        errno = EINVAL;
        perror_exit_1(); // Handle error for invalid usage
    }
    return 0; // End of main function
}
