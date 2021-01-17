#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define BUFFER_SIZE 1024

static struct class* dev_class;
static struct my_device_data {
    dev_t dev;
    struct cdev cdev;
	char buffer[BUFFER_SIZE];
	size_t size;
} my_device;

static int __init chr_driver_init(void);
static void __exit chr_driver_exit(void);
static ssize_t my_read(struct file* file, char __user* user_buffer, size_t size, loff_t* offset);
static ssize_t my_write(struct file* file, const char* user_buffer, size_t size, loff_t* offset);
static int my_open(struct inode* inode, struct file* file);
static int my_release(struct inode* inode, struct file* file);

static struct file_operations fops = {
    .owner      = THIS_MODULE,
    .read       = my_read,
    .write      = my_write,
    .open       = my_open,
    .release    = my_release
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
    struct my_device_data *my_data = (struct my_device_data *) file->private_data;
    
    if(size <= 0) {
        printk(KERN_INFO"my_device: Read: There is no data\n");
        return 0;
    }

    if(copy_to_user(user_buffer, my_data->buffer + *offset, size) != 0) {
        printk(KERN_INFO"my_device: Failed to copy data to user\n");
        return -EFAULT;
    }

    printk(KERN_INFO"my_device: Data is read\n");
    return size;
}

static ssize_t my_write(struct file* file, const char __user* user_buffer, size_t size, loff_t* offset) {
    struct my_device_data *my_data = (struct my_device_data *) file->private_data;

    if(size <= 0) {
        printk(KERN_INFO"my_device: Write: There is no data\n");
        return 0;
    }

    if(copy_from_user(my_data->buffer, user_buffer, size)) {
        printk(KERN_INFO"my_device: Failed to copy data from the user\n");
        return -EFAULT;
    }

    printk(KERN_INFO"my_device: Data is written\n");
    return size;
}


module_init(chr_driver_init);
module_exit(chr_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("naezith");
MODULE_DESCRIPTION("Character device driver test");
