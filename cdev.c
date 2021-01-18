#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <asm/io.h>

#define BUFFER_SIZE 1024

// IOCTL codes: magic number, command number, data type
#define MY_WR_DATA _IOW('a', 'a', int32_t*)
#define MY_RD_DATA _IOR('a', 'b', int32_t*)

// CDEV data
static struct class* dev_class;
static struct my_device_data {
    char proc_buffer[BUFFER_SIZE];
	char buffer[BUFFER_SIZE];
    int proc_toggle;
	size_t size;
    dev_t dev;
    int32_t ioctl_val;
    struct cdev cdev;
} my_device = {"A naive proc example\n", "", 1, 0, 0, 0};

// cdev
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

// proc data
#define PROC_ENTRY_NAME "my_device_proc"

// proc
static int my_open_proc(struct inode* inode, struct file* file);
static int my_release_proc(struct inode* inode, struct file* file);
static ssize_t my_read_proc(struct file* file, char __user* user_buffer, size_t size, loff_t* offset);
static ssize_t my_write_proc(struct file* file, const char* user_buffer, size_t size, loff_t* offset);

static struct proc_ops fops_proc = {
    .proc_read           = my_read_proc,
    .proc_write          = my_write_proc,
    .proc_open           = my_open_proc,
    .proc_release        = my_release_proc
};

// Interrupt Handler IRQ
#define IRQ_NO 1 // Keyboard
irqreturn_t irq_handler(int irq, void* dev_id, struct pt_regs* regs);

// Tasklet
void tasklet_func(struct tasklet_struct* data);
// Static tasklet init
// DECLARE_TASKLET(tasklet, tasklet_func);

// Dynamic tasklet, init part is in the cdev init func
struct tasklet_struct* tasklet;

// Thread
static struct task_struct* my_thread1;
static struct task_struct* my_thread2;
int thread_func1(void* p);
int thread_func2(void* p);

// Spinlock
DEFINE_SPINLOCK(my_spinlock);
unsigned long spinlock_counter = 0;

// cdev
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
    if(device_create(dev_class, NULL, my_device.dev, NULL, "my_device") == NULL) {
        printk(KERN_INFO"my_device: Failed to create the device\n");
        goto r_device;
    }

    printk(KERN_INFO"my_device: Device driver is inserted\n");

    // Register the interrupt handler
    if (request_irq(IRQ_NO, (irq_handler_t)irq_handler, IRQF_SHARED, "my_device_kb_irq", (void*)(irq_handler))) {
        printk(KERN_INFO"my_device: Failed to register the interrupt handler\n");
        goto r_irq;
    }

    printk(KERN_INFO"my_device: Registered the interrupt handler No:%d\n", IRQ_NO);

    // Thread, run separately
    // if((my_thread1 = kthread_create(thread_func1, NULL, "my thread"))) {
    //     wake_up_process(my_thread1);
    // }
    // else {
    //     printk(KERN_INFO"my_device: Failed to create the thread\n");
    //     goto r_irq;
    // }

    // Thread, run directly
    if((my_thread1 = kthread_run(thread_func1, NULL, "my thread 1")) == 0) {
        printk(KERN_INFO"my_device: Failed to create the thread\n");
        goto r_irq;
    }
    if((my_thread2 = kthread_run(thread_func2, NULL, "my thread 2")) == 0) {
        printk(KERN_INFO"my_device: Failed to create the thread\n");
        goto r_irq;
    }


    // Dynamic tasklet init
    tasklet = kmalloc(sizeof(struct tasklet_struct), GFP_KERNEL);
    tasklet_init(tasklet, (void*)tasklet_func, 0);
    if(tasklet == NULL) {
        printk(KERN_INFO"my_device: Failed to alloc memory for the tasklet\n");
        goto r_irq;
    }

    // Create proc entry
    if(proc_create(PROC_ENTRY_NAME, 0666, NULL, &fops_proc) == NULL) {
        printk(KERN_INFO"my_device: Failed to create the proc entry\n");
        goto r_proc;
    }

    return 0;

r_proc:
    remove_proc_entry(PROC_ENTRY_NAME, NULL);

r_irq:
    free_irq(IRQ_NO, (void*)(irq_handler));

r_device: 
    device_destroy(dev_class, my_device.dev);

r_class: 
    class_destroy(dev_class);

    return -1;
}

static void __exit chr_driver_exit(void) {
    kthread_stop(my_thread1);
    kthread_stop(my_thread2);
    device_destroy(dev_class, my_device.dev);
    class_destroy(dev_class);
    cdev_del(&my_device.cdev);
    unregister_chrdev_region(my_device.dev, 1);
    tasklet_kill(tasklet);
    free_irq(IRQ_NO, (void*)(irq_handler));
    remove_proc_entry(PROC_ENTRY_NAME, NULL);
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

    if(copy_to_user(user_buffer, my_data->buffer, size) != 0) {
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
            
            printk(KERN_INFO"my_device: WRITE ioctl_val = %d\n", my_data->ioctl_val);
            break;

        case MY_RD_DATA:
            if(copy_to_user((int32_t*)arg, &my_data->ioctl_val, sizeof(my_data->ioctl_val)) != 0) {
                printk(KERN_INFO"my_device: Failed to read IOCTL data from the user\n");
                return -1;
            }
            
            printk(KERN_INFO"my_device: READ ioctl_val = %d\n", my_data->ioctl_val);
            break;
    }

    return 0;
}


static int my_open_proc(struct inode* inode, struct file* file) {
    printk(KERN_INFO"my_device: Opened proc fs\n");
    return 0;
}

static int my_release_proc(struct inode* inode, struct file* file) {
    printk(KERN_INFO"my_device: Closed proc fs\n");
    return 0;
}

static ssize_t my_read_proc(struct file* file, char __user* user_buffer, size_t size, loff_t* offset) {
    struct my_device_data* my_data = &my_device;

    if(my_data->proc_toggle == 0) {
        printk(KERN_INFO"my_device: Sending 0 after first one to stop the loop\n");
        my_data->proc_toggle = 1;
        return 0;
    }
    
    my_data->proc_toggle = 0;
    
    if(size <= 0) {
        printk(KERN_INFO"my_device: Read: Size is 0\n");
        return 0;
    }

    if(copy_to_user(user_buffer, my_data->proc_buffer, BUFFER_SIZE) != 0) {
        printk(KERN_INFO"my_device: Failed to copy data to user\n");
        return -1;
    }

    printk(KERN_INFO"my_device: Data is read\n");
    return BUFFER_SIZE;
}

static ssize_t my_write_proc(struct file* file, const char* user_buffer, size_t size, loff_t* offset) {
    struct my_device_data* my_data = &my_device;

    if(size <= 0) {
        printk(KERN_INFO"my_device: Write: There is no data\n");
        return 0;
    }

    if(copy_from_user(my_data->proc_buffer, user_buffer, size) != 0) {
        printk(KERN_INFO"my_device: Failed to copy data from the user\n");
        return -1;
    }

    printk(KERN_INFO"my_device: Data is written\n");
    return size;
}

irqreturn_t irq_handler(int irq, void* dev_id, struct pt_regs* regs) {
    static size_t i = 0;
    spin_lock_irq(&my_spinlock);
    
    printk(KERN_INFO"my_device: Keyboard Interrupt #%ld, spinlock counter: %lu\n", i++, ++spinlock_counter);

    spinlock_unlock_irq(&my_spinlock);
    
    // Static tasklet
    // tasklet_schedule(&tasklet);
    // Dynamic tasklet
    tasklet_schedule(tasklet);

    return IRQ_HANDLED;
}

void tasklet_func(struct tasklet_struct* data) {
    printk(KERN_INFO"my_device: Executing the tasklet function, spinlock counter: %lu\n", ++spinlock_counter);

    spinlock_unlock_irq(&my_spinlock);
}

int thread_func1(void* p) {
    int i = 0;

    while(!kthread_should_stop()) {
        if(!spin_is_locked(&my_spinlock)) {
            printk(KERN_INFO"my_device: func1: Not locked, looping inside the kernel thread: %d\n", i++);
        }

        spin_lock(&my_spinlock);
        if(spin_is_locked(&my_spinlock)) 
            printk(KERN_INFO"my_device: func1: Locked: %d\n", i);

        printk(KERN_INFO"my_device: func1: Spinlock counter: %lu\n", ++spinlock_counter);

        spin_unlock(&my_spinlock);

        msleep(1000);
    }
    
    return 0;
}

int thread_func2(void* p) {
    int i = 0;

    while(!kthread_should_stop()) {
        if(!spin_is_locked(&my_spinlock)) {
            printk(KERN_INFO"my_device: func2: Not locked, looping inside the kernel thread: %d\n", i++);
        }

        spin_lock(&my_spinlock);
        if(spin_is_locked(&my_spinlock)) 
            printk(KERN_INFO"my_device: func2: Locked: %d\n", i);

        printk(KERN_INFO"my_device: func2: Spinlock counter: %lu\n", ++spinlock_counter);

        spin_unlock(&my_spinlock);

        msleep(1000);
    }

    return 0;
}

module_init(chr_driver_init);
module_exit(chr_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("naezith");
MODULE_DESCRIPTION("Character device driver test");
