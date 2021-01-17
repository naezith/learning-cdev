#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>

#define BUFFER_SIZE 1024

// IOCTL codes: magic number, command number, data type
#define MY_WR_DATA _IOW('a', 'a', int32_t*)
#define MY_RD_DATA _IOR('a', 'b', int32_t*)

// CDEV data
static struct class* dev_class;
static struct my_device_data {
	char buffer[BUFFER_SIZE];
	size_t size;
    dev_t dev;
    int32_t ioctl_val;
    struct cdev cdev;
} my_device = {"", 0, 0, 0};

static int __init chr_driver_init(void);
static void __exit chr_driver_exit(void);
static ssize_t my_read(struct file* file, char __user* user_buffer, size_t size, loff_t* offset);
static ssize_t my_write(struct file* file, const char* user_buffer, size_t size, loff_t* offset);
static int my_open(struct inode* inode, struct file* file);
static int my_release(struct inode* inode, struct file* file);
static long my_ioctl(struct file* file, unsigned int cmd, unsigned long arg);

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .read           = my_read,
    .write          = my_write,
    .open           = my_open,
    .release        = my_release,
    .unlocked_ioctl = my_ioctl
};

static int __init chr_driver_init(void) {
    // Get character device identification, major, minor
    if(alloc_chrdev_region(&my_device.dev, 0, 1, "my_device_driver") != 0) {
        printk(KERN_INFO"my_device: Failed to allocate character device region");
        return -1;
    }

    printk(KERN_INFO"my_device: Major: %d, Minor: %d\n", MAJOR(my_device.dev), MINOR(my_device.dev));
    
    // Create character device structure
    cdev_init(&my_device.cdev, &fops);

    // Add character device to the system
    if(cdev_add(&my_device.cdev, my_device.dev, 1) < 0) {
        printk(KERN_INFO"my_device: Failed to add the character device\n");
        goto r_class;
    }

    // Create struct class
    if((dev_class = class_create(THIS_MODULE, "my_class")) == NULL) {
        printk(KERN_INFO"my_device: Failed to create the struct class\n");
        goto r_class;
    } 

    // Create device
    if((device_create(dev_class, NULL, my_device.dev, NULL, "my_device")) == NULL) {
        printk(KERN_INFO"my_device: Failed to create the device\n");
        goto r_device;
    }

    printk(KERN_INFO"my_device: Device driver is inserted\n");

    return 0;

r_device: 
    device_destroy(dev_class, my_device.dev);

r_class: 
    class_destroy(dev_class);

    return -1;
}

static void __exit chr_driver_exit(void) {
    device_destroy(dev_class, my_device.dev);
    class_destroy(dev_class);
    cdev_del(&my_device.cdev);
    unregister_chrdev_region(my_device.dev, 1);
    printk(KERN_INFO"my_device: Device driver is removed\n");
}

static int my_open(struct inode* inode, struct file* file) {
    struct my_device_data* my_data = container_of(inode->i_cdev, struct my_device_data, cdev);

    file->private_data = my_data;
 
    printk(KERN_INFO"my_device: Device file opened\n");
    return 0;
}

static int my_release(struct inode* inode, struct file* file) {
    printk(KERN_INFO"my_device: Device file closed\n");
    return 0;
}

static ssize_t my_read(struct file* file, char __user* user_buffer, size_t size, loff_t* offset) {
    struct my_device_data* my_data = (struct my_device_data*) file->private_data;
    
    if(size <= 0) {
        printk(KERN_INFO"my_device: Read: There is no data\n");
        return 0;
    }

    if(copy_to_user(user_buffer, my_data->buffer + *offset, size) != 0) {
        printk(KERN_INFO"my_device: Failed to copy data to user\n");
        return -1;
    }

    printk(KERN_INFO"my_device: Data is read\n");
    return size;
}

static ssize_t my_write(struct file* file, const char __user* user_buffer, size_t size, loff_t* offset) {
    struct my_device_data* my_data = (struct my_device_data*) file->private_data;

    if(size <= 0) {
        printk(KERN_INFO"my_device: Write: There is no data\n");
        return 0;
    }

    if(copy_from_user(my_data->buffer, user_buffer, size) != 0) {
        printk(KERN_INFO"my_device: Failed to copy data from the user\n");
        return -1;
    }

    printk(KERN_INFO"my_device: Data is written\n");
    return size;
}

static long my_ioctl(struct file* file, unsigned int cmd, unsigned long arg) {
    struct my_device_data* my_data = (struct my_device_data*) file->private_data;

    // arg is actually the address to the buffer
    switch(cmd) {
        case MY_WR_DATA:
            if(copy_from_user(&my_data->ioctl_val, (int32_t*)arg, sizeof(my_data->ioctl_val)) != 0) {
                printk(KERN_INFO"my_device: Failed to write IOCTL data to the user\n");
                return -1;
            }
            
            printk(KERN_INFO" WRITE ioctl_val = %d\n", my_data->ioctl_val);
            break;

        case MY_RD_DATA:
            if(copy_to_user((int32_t*)arg, &my_data->ioctl_val, sizeof(my_data->ioctl_val)) != 0) {
                printk(KERN_INFO"my_device: Failed to read IOCTL data from the user\n");
                return -1;
            }
            
            printk(KERN_INFO" READ ioctl_val = %d\n", my_data->ioctl_val);
            break;
    }

    return 0;
}


module_init(chr_driver_init);
module_exit(chr_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("naezith");
MODULE_DESCRIPTION("Character device driver test");
