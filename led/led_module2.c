#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>

#define GPIO_LED 529
#define DRIVER_NAME "led_driver"
#define CLASS_NAME "LED"

static dev_t dev_num;
static struct cdev led_cdev;
static struct class *led_class;
static struct device *led_dev;

static void blink(int gpio) {
    for(int i = 0; i < 5; i++) {
        gpio_set_value(gpio, 0);
        msleep(1000);
        gpio_set_value(gpio, 1);
    }
}

static ssize_t write_led(struct file *file, const char __user *buf, size_t len, loff_t *pos) {
    char command;
    copy_from_user(&command, buf, 1);

    if (command == '0') {
        gpio_set_value(GPIO_LED, 0);
    }
    else if (command == '1') {
        blink(GPIO_LED);
    }
    else {
        return -1;
    }
    return 1;
}

const struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = write_led,
};

static int __init led_module_init(void) {
    int ret;
    ret = gpio_request(GPIO_LED, "LED GPIO");
    ret = gpio_direction_output(GPIO_LED, 0);

    ret = alloc_chrdev_region(&dev_num, 0, 0, DRIVER_NAME);
    if (ret != 0) {
        printk(KERN_ERR "device number alloc fail\n");
        return -1;
    }

    cdev_init(&led_cdev, &fops);
    ret = cdev_add(&led_cdev, dev_num, 1);
    if (ret != 0) {
        printk(KERN_ERR "cdev add fail\n");
        return -1;
    }

    led_class = class_create(CLASS_NAME);
    led_dev = device_create(led_class, NULL, dev_num, NULL, DRIVER_NAME);

    printk(KERN_INFO "init success\n");
    return 0;
}

static void __exit led_module_exit(void) {
    gpio_free(GPIO_LED);
    device_destroy(led_class, dev_num);
    class_destroy(led_class);
    cdev_del(&led_cdev);
    unregister_chrdev_region(led_num, 1);
    printk(KERN_INFO "module unload\n");
}

module_init(led_module_init);
module_exit(led_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");