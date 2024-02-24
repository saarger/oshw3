#include "message_slot.h"

#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>      /* open */
#include <unistd.h>     /* exit */
#include <stdio.h>
#include <errno.h>

#define STDOUT_FILENO 1

int perror_exit_1();

int perror_exit_1() {
    perror("");
    exit(1);
}

int main(int argc, char *argv[]) {
    char *filePath;
    unsigned long channelID;
    int fileDesc;
    char readBuffer[BUFFER_SIZE];

    if (argc == 3) {
        filePath = argv[1];
        channelID = strtoul(argv[2], NULL, 10); // Convert string to unsigned long
        size_t bytesRead;

        // Open the specified file
        fileDesc = open(filePath, O_RDWR);
        if (fileDesc < 0) {
            perror_exit_1();
        }

        // Set the channel ID for message retrieval
        if (ioctl(fileDesc, MSG_SLOT_CHANNEL, channelID) < 0){
            perror_exit_1();
        }

        // Read the message from the channel into the buffer
        bytesRead = read(fileDesc, readBuffer, BUFFER_SIZE);
        if (bytesRead < 0){
            perror_exit_1();
        }

        // Ensure the file is properly closed
        if (close(fileDesc) < 0){
            perror_exit_1();
        }

        // Output the retrieved message to stdout
        if (write(STDOUT_FILENO, readBuffer, bytesRead) < 0){
            perror_exit_1();
        }

        exit(0); // Exit successfully
    }
    else {
        // Handle incorrect usage
        errno = EINVAL;
        perror_exit_1();
    }
}
