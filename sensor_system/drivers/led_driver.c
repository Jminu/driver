#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/class.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

#define DRIVER_NAME "LED_DRIVER"
#define CLASS_NAME "LED_CLASS"
#define DEVICE_NAME "LED_DEVICE"
#define LED1 531
#define LED2 525

static dev_t led_dev_num;
static struct cdev led_chr_dev;
static struct class *led_class;
static struct device *led_dev;

static ssize_t led_write(struct file *file, const char __user *buf, size_t len, loff_t *pos) {
	char command;

	copy_from_user(&command, buf, 1);
	if (command == '1') {
		gpio_set_value(LED1, 1);
		gpio_set_value(LED2, 1);
		printk(KERN_INFO "gpio set 1\n");
	}
	else if (command == '0') {
		gpio_set_value(LED1, 0);
		gpio_set_value(LED2, 0);
		printk(KERN_INFO "gpio set 0\n");
	}
	else {
		printk(KERN_ERR "gpio set err\n");
		return -1;
	}
	return 1;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.write = led_write,
};



static int __init led_init(void) {
	int ret;

	ret = gpio_request_one(531, GPIOF_OUT_INIT_LOW);
	if (ret != 0) {
		printk(KERN_ERR "gpio request err\n");
		return -1;
	}

	ret = gpio_request_one(525, GPIOF_OUT_INIT_LOW);
	if (ret != 0) {
		printk(KERN_ERR "gpio request err\n");
		return -1;
	}

	ret = alloc_chrdev_region(&led_dev_num, 0, 1, DRIVER_NAME);
	if (ret != 0) {
		printk(KERN_ERR "get device number err\n");
		return -1;
	}

	cdev_init(&led_chr_dev, &fops);
	ret = cdev_add(&led_chr_dev, led_dev_num, 1);
	if (ret < 0) {
		printk(KERN_ERR "char device add err\n");
		return -1;
	}

	led_class = class_create(CLASS_NAME);
	led_dev = device_create(led_class, NULL, led_dev_num, NULL, DEVICE_NAME);

	printk(KERN_INFO "init sucess\n");
	return 0;
}

static void __exit led_exit(void) {
	gpio_free(LED1); // gpio free
	gpio_free(LED2);

	device_destroy(led_class, led_dev_num);
	class_destroy(led_class);

	unregister_chrdev_region(led_dev_num, 1);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");
MODULE_DESCRIPTION("LED MODULE");
