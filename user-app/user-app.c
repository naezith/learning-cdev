#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

enum e_actions { INVALID, READ, WRITE, IOCTL_READ, IOCTL_WRITE };

// IOCTL codes: magic number, command number, data type
#define MY_WR_DATA _IOW('a', 'a', char*)
#define MY_RD_DATA _IOR('a', 'b', char*)

#define BUFFER_SIZE 1024

int main(int argc, char* argv[]) {
    if(argc <= 1) {
r_invalid_operation:
        printf("Invalid operation, try one of: \"write\", \"read\", \"ioctl-write\", \"ioctl-read\".\n");
        goto r_error;
    }

    const char* arg_action = argv[1];
    enum e_actions action = strcmp(arg_action, "write")         == 0 ? WRITE :
                            strcmp(arg_action, "read")          == 0 ? READ : 
                            strcmp(arg_action, "ioctl-write")   == 0 ? IOCTL_WRITE : 
                            strcmp(arg_action, "ioctl-read")    == 0 ? IOCTL_READ : 
                                                                        INVALID;
    const char* action_data = argv[2];

    if(action == INVALID) {
        goto r_invalid_operation;
    }

    if(action == WRITE && action_data == NULL) {
        printf("Add the text to write like \"write hello\"\n");
        goto r_error;
    }

    if(action == IOCTL_WRITE && action_data == NULL) {
        printf("Add the text to write like \"ioctl-write 1337\"\n");
        goto r_error;
    }

    int fd = open("/dev/my_device", O_RDWR);
    if(fd < 0) {
        printf("Failed to open the device file\n");
        goto r_error;
    }

    switch(action) {
        case READ:
        {
            char read_buffer[BUFFER_SIZE];
            if(read(fd, read_buffer, BUFFER_SIZE) < 0) {
                printf("Failed to read from the device\n");
                goto r_error_cleanup;
            }
            
            printf("Successfully read %zu bytes from the character device:\n", strlen(read_buffer));
            printf("%s\n", read_buffer);
            break;
        }

        case WRITE:
            if(write(fd, action_data, strlen(action_data) + 1) <= 0) {
                printf("Failed to write to the device\n");
                goto r_error_cleanup;
            }

            printf("Successfully wrote to the character device\n");
            break;

        case IOCTL_WRITE:
        {
            int32_t num = atoi(action_data);
            if(ioctl(fd, MY_WR_DATA, (int32_t*) &num) == -1) {
                printf("Failed to IOCTL-write to the device\n");
                goto r_error_cleanup;
            }

            printf("Successfully IOCTL-wrote to the character device\n");
            break;
        }

        case IOCTL_READ:
        {
            int32_t num;
            if(ioctl(fd, MY_RD_DATA, (int32_t*) &num) == -1) {
                printf("Failed to IOCTL-read from the device\n");
                goto r_error_cleanup;
            }

            printf("Successfully IOCTL-read from the character device: %d\n", num);
            break;
        }
    }

    close(fd);
    return 0;

r_error_cleanup:
    close(fd);

r_error:
    return -1;
}