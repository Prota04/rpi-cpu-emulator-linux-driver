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
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0x92997ed8, "_printk" },
	{ 0x8d1a8936, "gpio_to_desc" },
	{ 0xe0588aeb, "gpiod_direction_output" },
	{ 0x5775903e, "gpiod_direction_input" },
	{ 0x7197e226, "gpiod_to_irq" },
	{ 0x92d5838e, "request_threaded_irq" },
	{ 0xc1514a3b, "free_irq" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0xf6e1d11f, "cdev_init" },
	{ 0x42041a93, "cdev_add" },
	{ 0xf795a77a, "class_create" },
	{ 0xe5393233, "device_create" },
	{ 0xc467ddce, "class_unregister" },
	{ 0x7057799f, "class_destroy" },
	{ 0x4e54674e, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x37a0cba, "kfree" },
	{ 0xb112c8e1, "gpiod_set_value" },
	{ 0x5df7df87, "device_destroy" },
	{ 0x57f3757b, "gpiod_get_value_cansleep" },
	{ 0x637493f3, "__wake_up" },
	{ 0xa628ef6e, "gpiod_get_value" },
	{ 0xe3e78877, "kmalloc_caches" },
	{ 0x604324bd, "__kmalloc_cache_noprof" },
	{ 0xf9a482f9, "msleep" },
	{ 0x5f754e5a, "memset" },
	{ 0xae353d77, "arm_copy_from_user" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x800473f, "__cond_resched" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x647af474, "prepare_to_wait_event" },
	{ 0x1000e51, "schedule" },
	{ 0x49970de8, "finish_wait" },
	{ 0xc3055d20, "usleep_range_state" },
	{ 0x51a910c0, "arm_copy_to_user" },
	{ 0xd824a911, "param_ops_uint" },
	{ 0xf1ce2f51, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "0B7C54EE9A5301179921CED");
