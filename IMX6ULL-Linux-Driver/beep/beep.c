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

#define BEEP_CNT     1              /* 设备号个数 */
#define BEEP_NAME    "beep"         /* 名字 */
#define BEEPOFF      0              /* 关蜂鸣器 */
#define BEEPON       1              /* 开蜂鸣器 */

/* beep设备结构体 */
struct beep_dev{
    dev_t devid;                /* 设备号 */
    int major;                  /* 主设备号 */
    int minor;                  /* 次设备号 */
    struct cdev cdev;           /* cdev */
    struct class *class;        /* 类 */
    struct device *device;      /* 设备 */
    struct device_node *nd;     /* 设备节点 */
    int beep_gpio;               /* beep使用的GPIO编号 */
};

struct beep_dev beep; /* beep设备 */

static int beep_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &beep;   //设置私有属性
    return 0;
}

static int beep_close(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t beep_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
    u32 ret;
    u8 dataBuf[1];
    struct beep_dev *dev = (struct beep_dev *)filp->private_data;

    //struct beep_dev *dev = (struct beep_dev*)filp->private_data;

    ret = copy_from_user(dataBuf,buf,count);
    if(ret != 0){
        printk("kernel write faibeep\n");
        return -EFAULT;
    }

    //根据dataBuf判断开蜂鸣器还是关蜂鸣器
    if(dataBuf[0] == BEEPON){
        gpio_set_value(dev->beep_gpio,0);
    }else{
        gpio_set_value(dev->beep_gpio,1);
    }

    return 0;
}

/* 设备操作函数 */
static const struct file_operations beep_fops = {
    .owner   = THIS_MODULE,
    .open    = beep_open,
    .release = beep_close,
    .write   = beep_write,
};

/* 驱动入口函数 */
static int __init beep_init(void)
{
    int ret;

    /* 申请设备号 */
    beep.major = 0;  //由内核分配
    if(beep.major){  //给定设备号
        beep.devid = MKDEV(beep.major,0);
        ret = register_chrdev_region(beep.devid,BEEP_CNT,BEEP_NAME);
    }else{
        ret = alloc_chrdev_region(&beep.devid,0,BEEP_CNT,BEEP_NAME);
        beep.major = MAJOR(beep.devid);
        beep.minor = MINOR(beep.devid);
    }
    printk("beep major = %d,minor = %d\n",beep.major,beep.minor);
    if(ret < 0){
        goto fail_devid;
    }

    /* 2.添加字符设备 */
    beep.cdev.owner = THIS_MODULE;
    cdev_init(&beep.cdev,&beep_fops);
    ret = cdev_add(&beep.cdev,beep.devid,BEEP_CNT);
    if(ret < 0){
        goto fail_cdev;
    }

    /* 自动添加设备节点 */
    /* 1.添加类 */
    beep.class = class_create(THIS_MODULE,BEEP_NAME);
    if(IS_ERR(beep.class)){
        ret = PTR_ERR(beep.class);
        goto fail_class;
    }

    /* 2.添加设备 */
    beep.device = device_create(beep.class,NULL,beep.devid,NULL,BEEP_NAME);
    if(IS_ERR(beep.device)){
        ret = PTR_ERR(beep.device);
        goto fail_device;
    }

    /* 设置Beep所使用的GPIO */
    /* 1.获取设备节点：beep */
    beep.nd = of_find_node_by_path("/beep");  //设备节点名
    if(beep.nd == NULL){
        printk("beep node can't not found\n");
        ret = -EINVAL;
        goto fail_node;
    }else{
        printk("beep node has been found\n");
    }

    /* 2.获取设备树中的gpio属性，得到Beep的编号 */
    beep.beep_gpio = of_get_named_gpio(beep.nd,"beep-gpio",0);//根据设备树实际命名更改
    if(beep.beep_gpio < 0){
        printk("can't get beep-gpio\n");
        ret = -EINVAL;
        goto fail_node;
    }
    printk("beep-gpio num = %d\n",beep.beep_gpio);

    /* 3.请求gpio */
    ret = gpio_request(beep.beep_gpio,"beep-gpio");
    if(ret){
        printk("can't request beep-gpio\n");
        goto fail_node;
    }

    /* 4.设置 GPIO5_IO01 为输出，并且输出高电平，默认关闭 BEEP */
    ret = gpio_direction_output(beep.beep_gpio,1);
    if(ret < 0){
        printk("can't set gpio\n");
        goto fail_setoutput;
    }

    return 0;

fail_setoutput:
    gpio_free(beep.beep_gpio);
fail_node:
    device_destroy(beep.class,beep.devid);
fail_device:
    class_destroy(beep.class);
fail_class:
    cdev_del(&beep.cdev);
fail_cdev:
    unregister_chrdev_region(beep.devid,BEEP_CNT);
fail_devid:
    return ret;
}

/* 驱动出口函数 */
static void __exit beep_exit(void)
{
    /* 关闭BEEP */
    gpio_set_value(beep.beep_gpio,BEEPOFF);
    gpio_free(beep.beep_gpio);

    /* 注销字符设备驱动 */
    cdev_del(&beep.cdev);    //删除cdev
    unregister_chrdev_region(beep.devid,BEEP_CNT);    //注销设备号


    device_destroy(beep.class,beep.devid);    //摧毁设备
    class_destroy(beep.class);   //摧毁类

    return;
}

/* 注册驱动和卸载驱动 */
module_init(beep_init);
module_exit(beep_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("bcl");