#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

static const int mem_size = 1024;

static dev_t dev = 0;
static struct class* dev_class;
static struct cdev my_cdev;
unsigned char* kernel_buffer;

static int __init chr_driver_init(void);
static void __exit chr_driver_exit(void);
static ssize_t my_read(struct file* file, char __user* buffer, size_t len, loff_t* off);
static ssize_t my_write(struct file* file, const char* buffer, size_t len, loff_t* off);
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
    // Allocate character device region 
    if(alloc_chrdev_region(&dev, 0, 1, "my_dev") < 0) {
        printk(KERN_INFO"Failed to allocate character device region");
        return -1;
    }

    printk(KERN_INFO"Major: %d, Minor: %d\n", MAJOR(dev), MINOR(dev));
    
    // Create character device structure
    cdev_init(&my_cdev, &fops);

    // Add character device to the system
    if(cdev_add(&my_cdev, dev, 1) < 0) {
        printk(KERN_INFO"Failed to add the character device\n");
        goto r_class;
    }

    // Create struct class
    if((dev_class = class_create(THIS_MODULE, "my_class")) == NULL) {
        printk(KERN_INFO"Failed to create the struct class\n");
        goto r_class;
    } 

    // Create device
    if((device_create(dev_class, NULL,dev, NULL, "my_device")) == NULL) {
        printk(KERN_INFO"Failed to create the device\n");
        goto r_device;
    }

    printk(KERN_INFO"Device driver is inserted\n");

    return 0;
    
r_device:
    class_destroy(dev_class);

r_class:
    unregister_chrdev_region(dev, 1);
    return -1;
}

static void __exit chr_driver_exit(void) {
    device_destroy(dev_class, dev);
    class_destroy(dev_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev, 1);
    printk(KERN_INFO"Device driver is removed\n");
}

static int my_open(struct inode* inode, struct file* file) {
    if((kernel_buffer = kmalloc(mem_size, GFP_KERNEL)) == 0) {
        printk(KERN_INFO"Failed to allocate the memory\n");
        return -1;
    }
 
    printk(KERN_INFO"Device file opened\n");
    return 0;
}

static int my_release(struct inode* inode, struct file* file) {
    kfree(kernel_buffer);
    printk(KERN_INFO"Device file closed\n");
    return 0;
}

static ssize_t my_read(struct file* file, char __user* buffer, size_t len, loff_t* off) {
    copy_to_user(buffer, kernel_buffer, mem_size);
    printk(KERN_INFO"Data is read\n");
    return mem_size;
}

static ssize_t my_write(struct file* file, const char __user* buffer, size_t len, loff_t* off) {
    copy_from_user(kernel_buffer, buffer, len);
    printk(KERN_INFO"Data is written\n");
    return len;
}


module_init(chr_driver_init);
module_exit(chr_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("naezith");
MODULE_DESCRIPTION("Character device driver");
