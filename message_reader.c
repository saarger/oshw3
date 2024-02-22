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
    char *message_file_path;
    unsigned long target_message_channel_id;
    int fd;
    char buffer[BUFFER_SIZE];

    if (argc == 3) {
        message_file_path = argv[1];
        target_message_channel_id = atoi(argv[2]);
        size_t message_len;
        /* open file */
        fd = open(message_file_path, O_RDWR);
        if (fd < 0) {
            perror_exit_1();
        }

        /* set channel id*/
        if (ioctl(fd, MSG_SLOT_CHANNEL, target_message_channel_id) < 0){
            perror_exit_1();
        }

        /* read message from channel to user buffer */
        message_len = read(fd, buffer, BUFFER_SIZE);
        if (message_len < 0){
            perror_exit_1();
        }

        if (close(fd) < 0){
            perror_exit_1();
        }


        /* write message from user buffer to stdout */
        if (write(STDOUT_FILENO, buffer, message_len) < 0){
            perror_exit_1();
        }

        exit(0);
    }

    else{
        errno = EINVAL;
        perror_exit_1();
    }
}
