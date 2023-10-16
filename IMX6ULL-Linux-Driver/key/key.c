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

#define KEY_CNT     1           /* 设备号个数 */
#define KEY_NAME    "key"       /* 名字 */

/* 定义按键值 */
#define KEY0VALUE         0xf0           /* 按键值 */
#define INVAKEY           0x00           /* 无效按键值 */

/* key设备结构体 */
struct key_dev{
    dev_t devid;                /* 设备号 */
    int major;                  /* 主设备号 */
    int minor;                  /* 次设备号 */
    struct cdev cdev;           /* cdev */
    struct class *class;        /* 类 */
    struct device *device;      /* 设备 */
    struct device_node *nd;     /* 设备节点 */
    int key_gpio;               /* key使用的GPIO编号 */
    atomic_t key_value;         /* 按键值 */
};

struct key_dev keydev; /* key设备 */

static int key_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &keydev;   //设置私有属性
    return 0;
}

static int key_close(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t key_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;
    unsigned char value;
    struct key_dev *dev = (struct key_dev *)filp->private_data;

    if(gpio_get_value(dev->key_gpio) == 0){     /* key0按下 */
        while(!gpio_get_value(dev->key_gpio));  /* 等待按键释放 */
        atomic_set(&dev->key_value,KEY0VALUE);
    }else{
        atomic_set(&dev->key_value,INVAKEY);
    }

    value = atomic_read(&dev->key_value);       /* 保存按键值 */
    ret = copy_to_user(buf,&value,sizeof(value));
    
    return ret;
}

/* 设备操作函数 */
static const struct file_operations key_fops = {
    .owner   = THIS_MODULE,
    .open    = key_open,
    .release = key_close,
    .read    = key_read,
};

/* 驱动入口函数 */
static int __init key_init(void)
{
    int ret;

    /* 初始化原子变量 */
    atomic_set(&keydev.key_value,INVAKEY);

    /* 申请设备号 */
    keydev.major = 0;  //由内核分配
    if(keydev.major){  //给定设备号
        keydev.devid = MKDEV(keydev.major,0);
        ret = register_chrdev_region(keydev.devid,KEY_CNT,KEY_NAME);
    }else{
        ret = alloc_chrdev_region(&keydev.devid,0,KEY_CNT,KEY_NAME);
        keydev.major = MAJOR(keydev.devid);
        keydev.minor = MINOR(keydev.devid);
    }
    printk("key major = %d,minor = %d\n",keydev.major,keydev.minor);
    if(ret < 0){
        goto fail_devid;
    }

    /* 2.添加字符设备 */
    keydev.cdev.owner = THIS_MODULE;
    cdev_init(&keydev.cdev,&key_fops);
    ret = cdev_add(&keydev.cdev,keydev.devid,KEY_CNT);
    if(ret < 0){
        goto fail_cdev;
    }

    /* 自动添加设备节点 */
    /* 1.添加类 */
    keydev.class = class_create(THIS_MODULE,KEY_NAME);
    if(IS_ERR(keydev.class)){
        ret = PTR_ERR(keydev.class);
        goto fail_class;
    }

    /* 2.添加设备 */
    keydev.device = device_create(keydev.class,NULL,keydev.devid,NULL,KEY_NAME);
    if(IS_ERR(keydev.device)){
        ret = PTR_ERR(keydev.device);
        goto fail_device;
    }

    /* 设置KEY所使用的GPIO */
    /* 1.获取设备节点：key */
    keydev.nd = of_find_node_by_path("/key");  //设备节点名
    if(keydev.nd == NULL){
        printk("key node can't not found\n");
        ret = -EINVAL;
        goto fail_node;
    }else{
        printk("key node has been found\n");
    }

    /* 2.获取设备树中的gpio属性，得到KEY的编号 */
    keydev.key_gpio = of_get_named_gpio(keydev.nd,"key-gpio",0);//根据设备树实际命名更改
    if(keydev.key_gpio < 0){
        printk("can't get key-gpio\n");
        ret = -EINVAL;
        goto fail_node;
    }
    printk("key-gpio num = %d\n",keydev.key_gpio);

    /* 3.请求gpio */
    ret = gpio_request(keydev.key_gpio,"key-gpio");
    if(ret){
        printk("can't request key-gpio\n");
        goto fail_node;
    }

    /* 4.设置 GPIO1_IO018 为输入 */
    ret = gpio_direction_input(keydev.key_gpio);
    if(ret < 0){
        printk("can't set gpio\n");
        goto fail_setoutput;
    }

    return 0;

fail_setoutput:
    gpio_free(keydev.key_gpio);
fail_node:
    device_destroy(keydev.class,keydev.devid);
fail_device:
    class_destroy(keydev.class);
fail_class:
    cdev_del(&keydev.cdev);
fail_cdev:
    unregister_chrdev_region(keydev.devid,KEY_CNT);
fail_devid:
    return ret;
}

/* 驱动出口函数 */
static void __exit key_exit(void)
{
    /* 释放KEY的gpio */
    gpio_free(keydev.key_gpio);

    /* 注销字符设备驱动 */
    cdev_del(&keydev.cdev);    //删除cdev
    unregister_chrdev_region(keydev.devid,KEY_CNT);    //注销设备号


    device_destroy(keydev.class,keydev.devid);    //摧毁设备
    class_destroy(keydev.class);   //摧毁类

    return;
}

/* 注册驱动和卸载驱动 */
module_init(key_init);
module_exit(key_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("bcl");