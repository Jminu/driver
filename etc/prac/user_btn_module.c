#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/spinlock.h>

#define BTN 533
#define BUF_SIZE 64

static int btn_cnt = 0;
static int irq_num = 0;
static spinlock_t btn_cnt_lock;

static dev_t dev_num;
static struct cdev chr_dev;
static struct class *chr_class;
static struct device *dev;

static irqreturn_t btn_isr(int irq, void *data) {
	spin_lock(&btn_cnt_lock); // spinlock
	btn_cnt++;
	spin_unlock(&btn_cnt_lock);
	printk(KERN_INFO "버튼 누른 횟수: %d\n", btn_cnt);

	return IRQ_HANDLED;
}

static ssize_t read_btn_cnt(struct file *file, char __user *buf, size_t len, loff_t *pos) {
	char chr[BUF_SIZE];
	unsigned long flag;

	spin_lock_irqsave(&btn_cnt_lock, flag); // spinlock guard interrupt
	int str_len = snprintf(chr, BUF_SIZE, "%d\n", btn_cnt);
	spin_lock_irqrestore(&btn_cnt_lock, flag); // restore
	
	if (*pos > 0) {
		printk(KERN_ERR "pos error\n");
		return 0;
	}

	if (len < str_len) {
		printk(KERN_ERR "too small buffer\n");
		return -1;
	}

	if (copy_to_user(buf, chr, str_len) != 0) {
		printk(KERN_ERR "copy to user fail\n");
		return -1;
	}

	*pos += str_len;

	return str_len;
}


static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = read_btn_cnt,
};

static int init_device(void) {
	int ret;

	ret = alloc_chrdev_region(&dev_num, 0, 1, "chr dev number");
	if (ret < 0) {
		printk(KERN_ERR "alloc chrdev region error\n");
		return -1;
	}

	cdev_init(&chr_dev, &fops);
	
	ret = cdev_add(&chr_dev, dev_num, 1);
	if (ret != 0) {
		printk(KERN_ERR "cdev add fail\n");
		return -1;
	}

	chr_class = class_create("char driver class");
	dev = device_create(chr_class, NULL, dev_num, NULL, "char_device");

	printk(KERN_INFO "character device init\n");
	return 1;
}

static void exit_device(void) {
	device_destroy(chr_class, dev_num);
	class_destroy(chr_class);
	cdev_del(&chr_dev);
	unregister_chrdev_region(dev_num, 1);
}

static int __init btn_module_init(void) {
	int ret;
	spin_lock_init(&btn_cnt_lock); // spinlock init

	ret = gpio_request_one(BTN, GPIOF_IN, "button");
	if (ret != 0) {
		printk(KERN_ERR "gpio rquest one fail\n");
		return -1;
	}

	irq_num = gpio_to_irq(BTN);
	if (irq_num < 0) {
		printk(KERN_ERR "gpio to irq fail\n");
		return -1;
	}

	ret = request_irq(irq_num, btn_isr, IRQF_TRIGGER_RISING, "btn irq", NULL);
	if (ret < 0) {
		printk(KERN_ERR "request irq fail\n");
		return -1;
	}
	
	ret = init_device();
	if (ret != 1) {
		printk(KERN_ERR "init device fail\n");
		return -1;
	}

	printk(KERN_INFO "module init\n");
	return 0;
}

static void __exit btn_module_exit(void) {
	gpio_free(BTN);
	free_irq(irq_num, NULL);
	exit_device();
	printk(KERN_INFO "module exit\n");
}

module_init(btn_module_init);
module_exit(btn_module_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("JIN MINU");
