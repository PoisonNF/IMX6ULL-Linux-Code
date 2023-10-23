#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/platform_device.h>

/* 寄存器物理地址 */
#define CCM_CCGR1_BASE              (0X020C406C)
#define SW_MUX_GPIO1_IO03_BASE      (0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE      (0X020E02F4)
#define GPIO1_DR_BASE               (0X0209C000)
#define GPIO1_GDIR_BASE             (0X0209C004)

/* 寄存器长度 */
#define REGISTER_LENGTH      4   

/* 释放led模块 */
static void led_release(struct device *dev)
{
    printk("led device release!\n");
}

/* 设备资源结构体，存放LED0所需要的寄存器 */
static struct resource led_resources[] = {
    [0] = {
        .start = CCM_CCGR1_BASE,
        .end   = CCM_CCGR1_BASE + REGISTER_LENGTH - 1,
        .flags = IORESOURCE_MEM,        //代表内存资源
    },
    [1] = {
        .start = SW_MUX_GPIO1_IO03_BASE,
        .end   = SW_MUX_GPIO1_IO03_BASE + REGISTER_LENGTH - 1,
        .flags = IORESOURCE_MEM,        //代表内存资源
    },
    [2] = {
        .start = SW_PAD_GPIO1_IO03_BASE,
        .end   = SW_PAD_GPIO1_IO03_BASE + REGISTER_LENGTH - 1,
        .flags = IORESOURCE_MEM,        //代表内存资源
    },
    [3] = {
        .start = GPIO1_DR_BASE,
        .end   = GPIO1_DR_BASE + REGISTER_LENGTH - 1,
        .flags = IORESOURCE_MEM,        //代表内存资源
    },
    [4] = {
        .start = GPIO1_GDIR_BASE,
        .end   = GPIO1_GDIR_BASE + REGISTER_LENGTH - 1,
        .flags = IORESOURCE_MEM,        //代表内存资源
    },
};

/* platform设备结构体 */
static struct platform_device leddevice = {
    .name = "platform-led",                         //设备名
    .id = -1,
    .dev = {
        .release = &led_release,
    },
    .num_resources = ARRAY_SIZE(led_resources),     //资源个数
    .resource = led_resources,                      //资源
};

/* 入口函数 */
static int __init leddevice_init(void)
{
    return platform_device_register(&leddevice);    //向内核注册platform设备
}

/* 出口函数 */
static void __exit leddevice_exit(void)
{
    return platform_device_unregister(&leddevice);  //卸载platform设备
}

/* 注册和卸载驱动 */
module_init(leddevice_init);
module_exit(leddevice_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("bcl");