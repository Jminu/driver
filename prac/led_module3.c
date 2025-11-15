#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DRIVER_NAME "led_driver"
#define CLASS_NAME "led_class"
#define RED 531 // gpio13
#define GREEN 525 // gpio19

struct dev_t dev_num;
struct cdev led_cdev
struct class *led_class;
struct device *led_dev;


static  ssize_t led_write(struct file *file, const char __user *buf, size_t len, loff_t *pos) {
    char command;
    copy_from_user(&command, buf, 1);

    if (command == '1') {
        gpio_set_value(RED, 1);
        gpio_set_value(GREEN, 1);
        printk(KERN_INFO "set 1\n");
    }
    else if (command == '0') {
        gpio_set_value(RED, 0);
        gpio_set_value(GREEN, 0);
        printk_(KERN_INFO "set 0\n");
    }
    else {
        printk(KERN_ERR "set error\n");
    }
    return 1;
}

static const file_operations fops = {
    .owner = THIS_MOUDLE,
    .write = led_write,
};

static int led_module_init(void) {
    int ret;
    ret = alloc_chrdev_region(&dev_num, 0, 1, DRIVER_NAME);
    if (ret != 0) {
        printk(KERN_ERR "alloc_chrdev_region fail\n");
        return -1;
    }
    
    // alloc gpio
    ret = gpio_request(GREEN, "green led");
    ret = gpio_direction_output(GREEN, 0);

    ret = gpio_request(RED, "red led");
    ret = gpio_direction_output(RED, 0);

    cdev_init(&led_cdev, &fops);
    cdev_add(&led_cdev, dev_num, 1);

    led_class = class_create(CLASS_NAME);
    led_dev = device_create(led_class, 0, dev_num, NULL, DRIVER_NAME);

    printk(KERN_INFO "module load\n");
    
    return 0;
}

static void led_module_exit(void) {
    device_destroy(led_class, dev_num);
    class_destroy(led_class);
    gpio_free(RED);
    gpio_free(GREEN);

    printk(KENR_INFO "module unload\n");
    return;
}

module_init();
module_exit();

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");