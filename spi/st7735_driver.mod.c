#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x2c992cbf, "__spi_register_driver" },
	{ 0x92893115, "driver_unregister" },
	{ 0x991fb4bf, "gpiod_set_value" },
	{ 0x1f3f08, "fb_deferred_io_cleanup" },
	{ 0xdc807621, "unregister_framebuffer" },
	{ 0x999e8297, "vfree" },
	{ 0x40064c60, "framebuffer_release" },
	{ 0x92997ed8, "_printk" },
	{ 0xdcb764ad, "memset" },
	{ 0x4e660045, "spi_sync" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0xfaf8208b, "framebuffer_alloc" },
	{ 0x3250fd9c, "devm_gpiod_get" },
	{ 0xbee3ddd5, "vzalloc_noprof" },
	{ 0x476b165a, "sized_strscpy" },
	{ 0x743dad74, "fb_deferred_io_init" },
	{ 0x36a78de3, "devm_kmalloc" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0x2596a0cd, "register_framebuffer" },
	{ 0x81de26f7, "fb_sys_read" },
	{ 0x1af1ac4e, "fb_sys_write" },
	{ 0x9f07225c, "cfb_fillrect" },
	{ 0x57812fd5, "cfb_copyarea" },
	{ 0x3b493b89, "cfb_imageblit" },
	{ 0x474e54d2, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cmy-custom,st7735");
MODULE_ALIAS("of:N*T*Cmy-custom,st7735C*");
MODULE_ALIAS("spi:my-custom,st7735");

MODULE_INFO(srcversion, "1C0C480540738103E3066D9");
