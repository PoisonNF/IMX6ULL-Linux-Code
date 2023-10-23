#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#define LEDDEV_CNT      1
#define LEDDEV_NAME     "dtsplatformled"    /* 设备名字 */
#define LEDOFF 0                            /* 关闭 */
#define LEDON  1                            /* 开启 */

/* LED设备结构体 */
struct leddev_dev{
    struct cdev cdev;           /* 字符设备 */
    dev_t devid;                /* 设备号 */
    struct class *class;        /* 类 */
    struct device *device;      /* 设备 */
    int major;                  /* 主设备号 */
    int minor;                  /* 次设备号 */
    struct device_node *nd;     /* 设备节点 */
    int led_gpio;               /* led使用的GPIO编号 */
};

struct leddev_dev leddev; /* led设备 */

static void led_switch(u8 sta)
{
    if(sta == LEDON){
        gpio_set_value(leddev.led_gpio,0);
    }else{
        gpio_set_value(leddev.led_gpio,1);
    }
}

static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &leddev;       /* 设置私有数据 */
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
    u32 ret;
    u8 dataBuf[1];

    ret = copy_from_user(dataBuf,buf,count);
    if(ret != 0){
        printk("kernel write failed\n");
        return -EFAULT;
    }

    //根据dataBuf判断开灯还是关灯
    led_switch(dataBuf[0]);
    
    return 0;
}

/* 设备操作函数 */
static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .write = led_write,
};

static int led_probe(struct platform_device *dev)
{
    int ret = 0;

    printk("led driver and device has matched!\n");

    /* 申请设备号 */
    if(leddev.major)
    {
        leddev.devid = MKDEV(leddev.major,0);
        ret = register_chrdev_region(leddev.devid,1,LEDDEV_NAME);
    }else{
        ret = alloc_chrdev_region(&leddev.devid,0,1,LEDDEV_NAME);
        leddev.major = MAJOR(leddev.devid);
        leddev.minor = MINOR(leddev.devid);
    }

    if(ret < 0){
        printk("leddev chrdev_region err!\n");
        goto fail_devid;
    }

    printk("leddev major = %d,minor = %d\n",leddev.major,leddev.minor);
    
    /* 注册字符设备 */
    leddev.cdev.owner = THIS_MODULE;
    cdev_init(&leddev.cdev,&led_fops);
    ret = cdev_add(&leddev.cdev, leddev.devid, 1);
    if(ret < 0)
        goto fail_cdev;

    /* 自动添加设备节点 */
    /* 添加类 */
    leddev.class = class_create(THIS_MODULE,LEDDEV_NAME);
    if(IS_ERR(leddev.class)){
        ret = PTR_ERR(leddev.class);
        goto fail_class;
    }

    /* 添加设备 */
    leddev.device = device_create(leddev.class, NULL,
			     leddev.devid, NULL,
			     LEDDEV_NAME);
    if(IS_ERR(leddev.device)){
        ret = PTR_ERR(leddev.device);
        goto fail_device;
    }

    /* 设置LED所使用的GPIO */
    /* 1.获取设备节点：gpioled */
#if 0
    leddev.nd = of_find_node_by_path("/gpioled");  //设备节点名
    if(leddev.nd == NULL){
        printk("gpioled node can't not found\n");
        ret = -EINVAL;
        goto fail_node;
    }else{
        printk("gpioled node has been found\n");
    }
#endif

    leddev.nd = dev->dev.of_node;       //通过platform结构体获取设备节点

    /* 2.获取设备树中的gpio属性，得到LED的编号 */
    leddev.led_gpio = of_get_named_gpio(leddev.nd,"led-gpio",0);//根据设备树实际命名更改
    if(leddev.led_gpio < 0){
        printk("can't get led-gpio\n");
        ret = -EINVAL;
        goto fail_node;
    }
    printk("led-gpio num = %d\n",leddev.led_gpio);

    /* 3.请求gpio */
    ret = gpio_request(leddev.led_gpio,"led-gpio");
    if(ret){
        printk("can't request led-gpio\n");
        goto fail_node;
    }

    /* 4.设置 GPIO1_IO03 为输出，并且输出高电平，默认关闭 LED 灯 */
    ret = gpio_direction_output(leddev.led_gpio,1);
    if(ret < 0){
        printk("can't set gpio\n");
        goto fail_setoutput;
    }

    return 0;

fail_setoutput:
    gpio_free(leddev.led_gpio);
fail_node:
    device_destroy(leddev.class,leddev.devid);
fail_device:
    device_destroy(leddev.class,leddev.devid);
fail_class:
    class_destroy(leddev.class);
fail_cdev:
    unregister_chrdev_region(leddev.devid,1);
fail_devid:
    return ret;
}

static int led_remove(struct platform_device *dev)
{
    /* 关闭LED，并释放 */
    led_switch(LEDOFF);
    gpio_free(leddev.led_gpio);

    /* 删除字符设备 */
    cdev_del(&leddev.cdev);

    /* 注销设备号 */
    unregister_chrdev_region(leddev.devid,1);

    /* 摧毁设备 */
    device_destroy(leddev.class,leddev.devid);

    /* 摧毁类 */
    class_destroy(leddev.class);
    return 0;
}

/* 匹配列表 */
static const struct of_device_id led_of_match[] = {
    {.compatible = "atkalpha-gpioled"},
    {/* Sentinel */},
};

/* platform 驱动结构体 */
static struct platform_driver led_driver = {
    .driver = {
        .name = "dtsplatform-led",          /* 驱动名字，用于和设备匹配 */
        .of_match_table = led_of_match,     /* 设备树匹配表 */
    },
    .probe = led_probe,
    .remove = led_remove,
};


/* 入口函数 */
static int __init leddriver_init(void)
{
    return platform_driver_register(&led_driver);    //向内核注册platform驱动
}

/* 出口函数 */
static void __exit leddriver_exit(void)
{
    return platform_driver_unregister(&led_driver);  //卸载platform驱动
}

/* 注册和卸载驱动 */
module_init(leddriver_init);
module_exit(leddriver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("bcl");