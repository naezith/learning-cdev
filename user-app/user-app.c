#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

enum e_actions { INVALID, READ, WRITE };

#define BUFFER_SIZE 1024

int main(int argc, char* argv[]) {
    if(argc <= 1) {
        printf("Try args: \"write hello\" then \"read hello\"\n");
        return -1;
    }

    const char* arg_action = argv[1];
    enum e_actions action = strcmp(arg_action, "write") == 0 ? WRITE :
                            strcmp(arg_action, "read")  == 0 ? READ : INVALID;
    const char* action_data = argv[2];

    if(action == INVALID) {
        printf("Invalid operation, try \"write\" or \"read\".\n");
        return -1;
    }

    if(action == READ) {
        printf("Will read from the device.\n");
    }

    if(action == WRITE) {
        if(action_data == NULL) {
            printf("Add the text to write like \"write hello\"\n");
            return -1;
        }
        printf("Will write \"%s\" to the device.\n", action_data);
    }

    int fd = open("/dev/my_device", O_RDWR);
    if(fd < 0) {
        printf("Failed to open the device file\n");
        return -1;
    }

    switch(action) {
        case READ:
        {
            char read_buffer[BUFFER_SIZE];
            if(read(fd, read_buffer, BUFFER_SIZE) <= 0) {
                printf("Failed to read from the device!");
                return -1;
            }
            printf("Successfully read data from the character device:\n");
            printf("%s\n", read_buffer);
            break;
        }
        case WRITE:
            if(write(fd, action_data, strlen(action_data) + 1) <= 0) {
                printf("Failed to write to the device!");
                return -1;
            }
            printf("Successfully wrote the data to the character device\n");
            break;

        default: 
            break;
    }

    close(fd);
    
    return 0;
}