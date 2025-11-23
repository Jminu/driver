#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define DRIVER_NAME "led_driver"
#define GPIO_LED 525 // GPIO13

static dev_t led_num;
static struct cdev led_cdev;
static struct class *led_class;


static ssize_t write_led(struct file *file, const char __user *buf, size_t count, loff_t *pos) {
	char command;
	int ret = copy_from_user(&command, buf, 1);
	if (ret < 0) {
		printk(KERN_ERR "copy from user fail\n");
		return -1;
	}

	if (command == '1') {
		gpio_set_value(GPIO_LED, 1);
		printk(KERN_INFO "gpio set 1\n");
	}
	else if (command == '0') {
		gpio_set_value(GPIO_LED, 0);
		printk(KERN_INFO "gpio set 0\n");
	}
	else {
		printk(KERN_INFO "command error\n");
		return -1;
	}

	return 1;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.write = write_led,
};

static int __init led_module_init(void) {
	int ret = alloc_chrdev_region(&led_num, 0, 1, DRIVER_NAME);
	if (ret < 0) {
		printk(KERN_ERR "device number alloc fail\n");
		return -1;
	}

	ret = gpio_request(GPIO_LED, DRIVER_NAME);
	if (ret < 0) {
		printk(KERN_ERR "gpio request fail\n");
		return -1;
	}
	gpio_direction_output(GPIO_LED, 0);

	cdev_init(&led_cdev, &fops);
	ret = cdev_add(&led_cdev, led_num, 1);
	if (ret < 0) {
		printk(KERN_ERR "cdev add fail\n");
		return -1;
	}

	led_class = class_create(DRIVER_NAME);
	device_create(led_class, NULL, led_num, NULL, DRIVER_NAME);

	return 1;
}

static void __exit led_module_exit(void) {
	device_destroy(led_class, led_num);
	class_destroy(led_class);

	cdev_del(&led_cdev);
	gpio_free(GPIO_LED);
	unregister_chrdev_region(led_num, 1);

	printk(KERN_INFO "module unloaded\n");
}

module_init(led_module_init);
module_exit(led_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");
