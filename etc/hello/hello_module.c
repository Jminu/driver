#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL v2");

static int hello_module_init(void) {
	printk("Hello module!\n");
	return 0;
}

static void bye_module_exit(void) {
	printk("Bye module!\n");
}

module_init(hello_module_init);
module_exit(bye_module_exit);
