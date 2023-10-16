#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#define GPIOLED_CNT     1           /* 设备号个数 */
#define GPIOLED_NAME    "gpioled"   /* 名字 */
#define LEDOFF          0           /* 关灯 */
#define LEDON           1           /* 开灯 */

/* led设备结构体 */
struct gpioled_dev{
    dev_t devid;                /* 设备号 */
    int major;                  /* 主设备号 */
    int minor;                  /* 次设备号 */
    struct cdev cdev;           /* cdev */
    struct class *class;        /* 类 */
    struct device *device;      /* 设备 */
    struct device_node *nd;     /* 设备节点 */
    int led_gpio;               /* led使用的GPIO编号 */
    int dev_stats;              /* 设备状态，0:设备未使用 >0:设备已经被使用 */
    spinlock_t lock;            /* 自旋锁 */
};

struct gpioled_dev gpioled; /* led设备 */

static int gpioled_open(struct inode *inode, struct file *filp)
{
    unsigned long flags;
    filp->private_data = &gpioled;   //设置私有属性

    spin_lock_irqsave(&gpioled.lock,flags); /* 上锁 */
    /* 如果设备被使用了 */
    if(gpioled.dev_stats){
        spin_unlock_irqrestore(&gpioled.lock,flags);    /* 解锁 */
        return -EBUSY;
    }

    gpioled.dev_stats++; /* 如果设备没有打开，那么标记已经打开了 */
    spin_unlock_irqrestore(&gpioled.lock,flags);    /* 解锁 */

    return 0;
}

static int gpioled_close(struct inode *inode, struct file *filp)
{
    unsigned long flags;
    struct gpioled_dev *dev = filp->private_data;

    /* 关闭驱动文件的时候将dev_stats减1 */
    spin_lock_irqsave(&gpioled.lock,flags); /* 上锁 */
    if(dev->dev_stats){
        dev->dev_stats--;
    }
    spin_unlock_irqrestore(&gpioled.lock,flags);    /* 解锁 */

    return 0;
}

static ssize_t gpioled_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
    u32 ret;
    u8 dataBuf[1];
    struct gpioled_dev *dev = (struct gpioled_dev *)filp->private_data;

    //struct gpioled_dev *dev = (struct gpioled_dev*)filp->private_data;

    ret = copy_from_user(dataBuf,buf,count);
    if(ret != 0){
        printk("kernel write failed\n");
        return -EFAULT;
    }

    //根据dataBuf判断开灯还是关灯
    if(dataBuf[0] == LEDON){
        gpio_set_value(dev->led_gpio,0);
    }else{
        gpio_set_value(dev->led_gpio,1);
    }

    return 0;
}

/* 设备操作函数 */
static const struct file_operations gpioled_fops = {
    .owner   = THIS_MODULE,
    .open    = gpioled_open,
    .release = gpioled_close,
    .write   = gpioled_write,
};

/* 驱动入口函数 */
static int __init led_init(void)
{
    int ret;

    /* 初始化自旋锁 */
    spin_lock_init(&gpioled.lock);

    /* 申请设备号 */
    gpioled.major = 0;  //由内核分配
    if(gpioled.major){  //给定设备号
        gpioled.devid = MKDEV(gpioled.major,0);
        ret = register_chrdev_region(gpioled.devid,GPIOLED_CNT,GPIOLED_NAME);
    }else{
        ret = alloc_chrdev_region(&gpioled.devid,0,GPIOLED_CNT,GPIOLED_NAME);
        gpioled.major = MAJOR(gpioled.devid);
        gpioled.minor = MINOR(gpioled.devid);
    }
    printk("gpioled major = %d,minor = %d\n",gpioled.major,gpioled.minor);
    if(ret < 0){
        goto fail_devid;
    }

    /* 2.添加字符设备 */
    gpioled.cdev.owner = THIS_MODULE;
    cdev_init(&gpioled.cdev,&gpioled_fops);
    ret = cdev_add(&gpioled.cdev,gpioled.devid,GPIOLED_CNT);
    if(ret < 0){
        goto fail_cdev;
    }

    /* 自动添加设备节点 */
    /* 1.添加类 */
    gpioled.class = class_create(THIS_MODULE,GPIOLED_NAME);
    if(IS_ERR(gpioled.class)){
        ret = PTR_ERR(gpioled.class);
        goto fail_class;
    }

    /* 2.添加设备 */
    gpioled.device = device_create(gpioled.class,NULL,gpioled.devid,NULL,GPIOLED_NAME);
    if(IS_ERR(gpioled.device)){
        ret = PTR_ERR(gpioled.device);
        goto fail_device;
    }

    /* 设置LED所使用的GPIO */
    /* 1.获取设备节点：gpioled */
    gpioled.nd = of_find_node_by_path("/gpioled");  //设备节点名
    if(gpioled.nd == NULL){
        printk("gpioled node can't not found\n");
        ret = -EINVAL;
        goto fail_node;
    }else{
        printk("gpioled node has been found\n");
    }

    /* 2.获取设备树中的gpio属性，得到LED的编号 */
    gpioled.led_gpio = of_get_named_gpio(gpioled.nd,"led-gpio",0);//根据设备树实际命名更改
    if(gpioled.led_gpio < 0){
        printk("can't get led-gpio\n");
        ret = -EINVAL;
        goto fail_node;
    }
    printk("led-gpio num = %d\n",gpioled.led_gpio);

    /* 3.请求gpio */
    ret = gpio_request(gpioled.led_gpio,"led-gpio");
    if(ret){
        printk("can't request led-gpio\n");
        goto fail_node;
    }

    /* 4.设置 GPIO1_IO03 为输出，并且输出高电平，默认关闭 LED 灯 */
    ret = gpio_direction_output(gpioled.led_gpio,1);
    if(ret < 0){
        printk("can't set gpio\n");
        goto fail_setoutput;
    }

    return 0;

fail_setoutput:
    gpio_free(gpioled.led_gpio);
fail_node:
    device_destroy(gpioled.class,gpioled.devid);
fail_device:
    class_destroy(gpioled.class);
fail_class:
    cdev_del(&gpioled.cdev);
fail_cdev:
    unregister_chrdev_region(gpioled.devid,GPIOLED_CNT);
fail_devid:
    return ret;
}

/* 驱动出口函数 */
static void __exit led_exit(void)
{
    /* 关闭LED */
    gpio_set_value(gpioled.led_gpio,LEDOFF);
    gpio_free(gpioled.led_gpio);

    /* 注销字符设备驱动 */
    cdev_del(&gpioled.cdev);    //删除cdev
    unregister_chrdev_region(gpioled.devid,GPIOLED_CNT);    //注销设备号


    device_destroy(gpioled.class,gpioled.devid);    //摧毁设备
    class_destroy(gpioled.class);   //摧毁类

    return;
}

/* 注册驱动和卸载驱动 */
module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("bcl");