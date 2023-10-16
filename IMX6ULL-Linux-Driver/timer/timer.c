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
#include <linux/atomic.h>

#define TIMER_CNT       1               /* 设备号个数 */
#define TIMER_NAME      "timer"         /* 名字 */

#define CLOSE_CMD       (_IO(0XEF,0x1)) /* 关闭定时器 */
#define OPEN_CMD        (_IO(0XEF,0x2)) /* 打开定时器 */
#define SETPERIOD_CMD   (_IO(0XEF,0x3)) /* 设置定时器周期命令 */

#define LEDOFF          0           /* 关灯 */
#define LEDON           1           /* 开灯 */

/* led设备结构体 */
struct timer_dev{
    dev_t devid;                /* 设备号 */
    int major;                  /* 主设备号 */
    int minor;                  /* 次设备号 */
    struct cdev cdev;           /* cdev */
    struct class *class;        /* 类 */
    struct device *device;      /* 设备 */
    struct device_node *nd;     /* 设备节点 */
    int led_gpio;               /* led使用的GPIO编号 */
    atomic_t timeperiod;        /* 定时周期，单位为ms */
    struct timer_list timer;    /* 定义一个定时器 */
};

struct timer_dev timerdev; /* timer设备 */

static int timer_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &timerdev;   //设置私有属性

    atomic_set(&timerdev.timeperiod,1000); /* 默认周期为1s */
    //add_timer(&timerdev.timer);
    mod_timer(&timerdev.timer,jiffies + msecs_to_jiffies(atomic_read(&timerdev.timeperiod)));
    return 0;
}

static int timer_close(struct inode *inode, struct file *filp)
{
    return 0;
}

static long timer_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)filp->private_data;

    switch(cmd)
    {
        case CLOSE_CMD:     /* 关闭定时器 */
            del_timer_sync(&dev->timer);
            gpio_set_value(timerdev.led_gpio,LEDOFF);
        case OPEN_CMD:      /* 开启定时器 */
            mod_timer(&dev->timer,jiffies + msecs_to_jiffies(atomic_read(&dev->timeperiod)));
        case SETPERIOD_CMD: /* 设置定时器 */
            atomic_set(&dev->timeperiod,arg);
            mod_timer(&dev->timer,jiffies + msecs_to_jiffies(atomic_read(&dev->timeperiod)));
        default:
            break;
    }

    return 0;
}

/* 设备操作函数 */
static const struct file_operations timer_fops = {
    .owner   = THIS_MODULE,
    .open    = timer_open,
    .release = timer_close,
    .unlocked_ioctl = timer_unlocked_ioctl,
};


/* 定时器回调函数 */
void timer_function(unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)arg;
    static int sta = 1;
    int timerperiod;

    sta = !sta; /* 每次都取反，实现LED翻转 */
    gpio_set_value(dev->led_gpio,sta);

    /* 重启定时器 */
    timerperiod = atomic_read(&dev->timeperiod);
    mod_timer(&dev->timer,jiffies + msecs_to_jiffies(atomic_read(&dev->timeperiod)));
}

/* 驱动入口函数 */
static int __init timer_init(void)
{
    int ret;

    /* 申请设备号 */
    timerdev.major = 0;  //由内核分配
    if(timerdev.major){  //给定设备号
        timerdev.devid = MKDEV(timerdev.major,0);
        ret = register_chrdev_region(timerdev.devid,TIMER_CNT,TIMER_NAME);
    }else{
        ret = alloc_chrdev_region(&timerdev.devid,0,TIMER_CNT,TIMER_NAME);
        timerdev.major = MAJOR(timerdev.devid);
        timerdev.minor = MINOR(timerdev.devid);
    }
    printk("timer major = %d,minor = %d\n",timerdev.major,timerdev.minor);
    if(ret < 0){
        goto fail_devid;
    }

    /* 2.添加字符设备 */
    timerdev.cdev.owner = THIS_MODULE;
    cdev_init(&timerdev.cdev,&timer_fops);
    ret = cdev_add(&timerdev.cdev,timerdev.devid,TIMER_CNT);
    if(ret < 0){
        goto fail_cdev;
    }

    /* 自动添加设备节点 */
    /* 1.添加类 */
    timerdev.class = class_create(THIS_MODULE,TIMER_NAME);
    if(IS_ERR(timerdev.class)){
        ret = PTR_ERR(timerdev.class);
        goto fail_class;
    }

    /* 2.添加设备 */
    timerdev.device = device_create(timerdev.class,NULL,timerdev.devid,NULL,TIMER_NAME);
    if(IS_ERR(timerdev.device)){
        ret = PTR_ERR(timerdev.device);
        goto fail_device;
    }

    /* 设置LED所使用的GPIO */
    /* 1.获取设备节点：gpioled */
    timerdev.nd = of_find_node_by_path("/gpioled");  //设备节点名
    if(timerdev.nd == NULL){
        printk("gpioled node can't not found\n");
        ret = -EINVAL;
        goto fail_node;
    }else{
        printk("gpioled node has been found\n");
    }

    /* 2.获取设备树中的gpio属性，得到LED的编号 */
    timerdev.led_gpio = of_get_named_gpio(timerdev.nd,"led-gpio",0);//根据设备树实际命名更改
    if(timerdev.led_gpio < 0){
        printk("can't get led-gpio\n");
        ret = -EINVAL;
        goto fail_node;
    }
    printk("led-gpio num = %d\n",timerdev.led_gpio);

    /* 3.请求gpio */
    ret = gpio_request(timerdev.led_gpio,"led-gpio");
    if(ret){
        printk("can't request led-gpio\n");
        goto fail_node;
    }

    /* 4.设置 GPIO1_IO03 为输出，并且输出高电平，默认关闭 LED 灯 */
    ret = gpio_direction_output(timerdev.led_gpio,1);
    if(ret < 0){
        printk("can't set gpio\n");
        goto fail_setoutput;
    }

    /* 初始化定时器，设置定时器处理函数，未设置周期，不会激活定时器 */
    init_timer(&timerdev.timer);
    timerdev.timer.function = timer_function;
    timerdev.timer.data = (unsigned long)&timerdev;

    return 0;

fail_setoutput:
    gpio_free(timerdev.led_gpio);
fail_node:
    device_destroy(timerdev.class,timerdev.devid);
fail_device:
    class_destroy(timerdev.class);
fail_class:
    cdev_del(&timerdev.cdev);
fail_cdev:
    unregister_chrdev_region(timerdev.devid,TIMER_CNT);
fail_devid:
    return ret;
}

/* 驱动出口函数 */
static void __exit timer_exit(void)
{
    /* 关闭LED */
    gpio_set_value(timerdev.led_gpio,LEDOFF);
    gpio_free(timerdev.led_gpio);

    /* 删除定时器 */
    del_timer_sync(&timerdev.timer);

    /* 注销字符设备驱动 */
    cdev_del(&timerdev.cdev);    //删除cdev
    unregister_chrdev_region(timerdev.devid,TIMER_CNT);    //注销设备号


    device_destroy(timerdev.class,timerdev.devid);    //摧毁设备
    class_destroy(timerdev.class);   //摧毁类

    return;
}

/* 注册驱动和卸载驱动 */
module_init(timer_init);
module_exit(timer_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("bcl");