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
#include <linux/of_irq.h>
#include <linux/ide.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fcntl.h>

#define IMX6UIRQ_CNT     1                  /* 设备号个数 */
#define IMX6UIRQ_NAME    "asyncnoti"        /* 名字 */

/* 定义按键值 */
#define KEY0VALUE         0x00              /* 按键值 */
#define INVAKEY           0xff              /* 无效按键值 */
#define KEY_NUM           1                 /* 按键数量 */

/* 中断IO描述结构体 */
struct irq_keydesc{
    int gpio;                               /* key使用的GPIO编号 */
    int irqnum;                             /* 中断号 */
    unsigned char value;                    /* 按键对应的键值 */
    char name[10];                          /* 名字 */
    irqreturn_t (*handler)(int,void *);     /* 中断服务函数 */
};

/* imx6uirq设备结构体 */
struct imx6uirq_dev{
    dev_t devid;                            /* 设备号 */
    int major;                              /* 主设备号 */
    int minor;                              /* 次设备号 */

    struct cdev cdev;                       /* cdev */
    struct class *class;                    /* 类 */
    struct device *device;                  /* 设备 */
    struct device_node *nd;                 /* 设备节点 */

    atomic_t keyvalue;                      /* 按键值 */
    atomic_t releasekey;                    /* 标记是否完成一次完成的按键 */
    struct irq_keydesc irqkeydesc[KEY_NUM]; /* 按键描述数组 */
    unsigned char curkeynum;                /* 当前按键号 */

    struct timer_list timer;                /* 定义一个定时器 */

    wait_queue_head_t   r_wait;             /* 读等待队列头 */

    struct fasync_struct *async_queue;      /* 异步相关结构体 */
};

struct imx6uirq_dev imx6uirq; /* irq设备 */

/* key0中断服务函数 */
static irqreturn_t key0_handler(int irq,void *dev_id)
{
    struct imx6uirq_dev *dev = (struct imx6uirq_dev *)dev_id;

    dev->curkeynum = 0; //当前为0号按键
    dev->timer.data = (volatile unsigned long)dev_id;   //将设备结构体传入定时器服务函数
    mod_timer(&dev->timer,jiffies + msecs_to_jiffies(10));  //延时10ms

    return IRQ_RETVAL(IRQ_HANDLED);
}

/* 定时器服务函数 */
void timer_function(unsigned long arg)
{
    unsigned char value;
    unsigned char num;
    struct irq_keydesc *keydesc;
    struct imx6uirq_dev *dev = (struct imx6uirq_dev *)arg;  //类型变换

    num = dev->curkeynum;
    keydesc = &dev->irqkeydesc[num];

    value = gpio_get_value(keydesc->gpio);  //读取io值
    if(value == 0){ //按下按键
        atomic_set(&dev->keyvalue,keydesc->value);
    }else{          //松开按键
        atomic_set(&dev->keyvalue,0x80|keydesc->value);
        atomic_set(&dev->releasekey,1); //标记一次按键完成
    }

    if(atomic_read(&dev->releasekey)){  //完成一次按键过程
        if(dev->async_queue){
            kill_fasync(&dev->async_queue,SIGIO,POLL_IN);
            printk("kill_fasync\n");
            atomic_set(&dev->releasekey,0); //标志位清零
        }
    }
#if 0
    /* 唤醒进程 */
    if(atomic_read(&dev->releasekey)){  //完成一次按键过程
        wake_up_interruptible(&dev->r_wait);
    }
#endif

}

/* 按键初始化函数 */
static int keyio_init(void)
{
    u8 i = 0;
    int ret = 0;

    /* 获取设备节点 */
    imx6uirq.nd = of_find_node_by_path("/key");
    if(imx6uirq.nd == NULL){
        printk("key node not find!\n");
        return -EINVAL;
    }

    /* 提取GPIO */
    for(i = 0;i < KEY_NUM;i++){
        imx6uirq.irqkeydesc[i].gpio = of_get_named_gpio(imx6uirq.nd,"key-gpio",i);
        if(imx6uirq.irqkeydesc[i].gpio < 0){
            printk("can't get key%d\n",i);
        }
    }

    /* 初始化 key 所使用的 IO，并且设置成中断模式 */
    for(i = 0;i < KEY_NUM;i++){
        memset(imx6uirq.irqkeydesc[i].name,0,sizeof(imx6uirq.irqkeydesc[i].name));
        sprintf(imx6uirq.irqkeydesc[i].name,"KEY%d",i);

        //请求gpio
        gpio_request(imx6uirq.irqkeydesc[i].gpio,imx6uirq.irqkeydesc[i].name);
        //指定为输入模式
        gpio_direction_input(imx6uirq.irqkeydesc[i].gpio);
        //获取中断号
        imx6uirq.irqkeydesc[i].irqnum = irq_of_parse_and_map(imx6uirq.nd,i);
#if 0    
        imx6uirq.irqkeydesc[i].irqnum = gpio_to_irq(imx6uirq.irqkeydesc[i].gpio);
#endif

        printk("key%d:gpio=%d,irqmun=%d\n",i,
                                imx6uirq.irqkeydesc[i].gpio,
                                imx6uirq.irqkeydesc[i].irqnum);
    }

    /* 申请中断 */
    //指定对应的中断服务函数
    imx6uirq.irqkeydesc[0].handler = key0_handler;
    imx6uirq.irqkeydesc[0].value = KEY0VALUE;

    for(i = 0;i < KEY_NUM;i++){
        ret = request_irq(imx6uirq.irqkeydesc[i].irqnum,
                          imx6uirq.irqkeydesc[i].handler,
                          IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,
                          imx6uirq.irqkeydesc[i].name,&imx6uirq);
        if(ret < 0){
            printk("irq %d request failed!\n",imx6uirq.irqkeydesc[i].irqnum);
            return -EFAULT;
        }
    }

    /* 创建定时器 */
    init_timer(&imx6uirq.timer);
    imx6uirq.timer.function = timer_function;

    /* 初始化等待队列头 */
    init_waitqueue_head(&imx6uirq.r_wait);

    return 0;
}


static int imx6uirq_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &imx6uirq;   //设置私有属性
    return 0;
}

static ssize_t imx6uirq_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;
#if 0
    unsigned char keyvalue = 0;
    unsigned char releasekey = 0;
    struct imx6uirq_dev *dev = (struct imx6uirq_dev *)filp->private_data;


    if(filp->f_flags & O_NONBLOCK){     /* 非阻塞访问 */
        if(atomic_read(&dev->releasekey) == 0)  /* 如果没有按键按下 */
            return -EAGAIN;
    }else{                              /* 阻塞访问 */
        /* 加入等待队列，等待被唤醒，也就是按键按下 */
        ret = wait_event_interruptible(dev->r_wait,atomic_read(&dev->releasekey));
        if(ret){
            goto wait_error;
        }
    }

    keyvalue = atomic_read(&dev->keyvalue);
    releasekey = atomic_read(&dev->releasekey);

    //如果按键按下
    if(releasekey){
        if(keyvalue & 0x80){
            keyvalue &= ~0x80;
            ret = copy_to_user(buf,&keyvalue,sizeof(keyvalue));
        }else{
            goto data_error;
        }

        atomic_set(&dev->releasekey,0); //标志位清零
    }else{
        goto data_error;
    }

    return ret;

wait_error:
    return ret;
data_error:
    return -EINVAL;
#endif
    return ret;

}

static unsigned int imx6uirq_poll(struct file *filp, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
#if 0
    struct imx6uirq_dev *dev = (struct imx6uirq_dev *)filp->private_data;

    poll_wait(filp,&dev->r_wait,wait);
    if(atomic_read(&dev->releasekey)){  //如果按键按下
        mask = POLLIN | POLLRDNORM;     //返回POLLIN
    }
#endif
    return mask;
}

static int imx6uirq_fasync(int fd, struct file *filp, int on)
{
    struct imx6uirq_dev *dev = (struct imx6uirq_dev *)filp->private_data;

    return fasync_helper(fd,filp,on,&dev->async_queue);
}

static int imx6uirq_close(struct inode *inode, struct file *filp)
{
    return imx6uirq_fasync(-1,filp,0);
}

/* 设备操作函数 */
static const struct file_operations imx6uirq_fops = {
    .owner   = THIS_MODULE,
    .open    = imx6uirq_open,
    .release = imx6uirq_close,
    .read    = imx6uirq_read,
    .poll    = imx6uirq_poll,
    .fasync  = imx6uirq_fasync,
};

/* 驱动入口函数 */
static int __init imx6uirq_init(void)
{
    int ret;

    /* 申请设备号 */
    imx6uirq.major = 0;  //由内核分配
    if(imx6uirq.major){  //给定设备号
        imx6uirq.devid = MKDEV(imx6uirq.major,0);
        ret = register_chrdev_region(imx6uirq.devid,IMX6UIRQ_CNT,IMX6UIRQ_NAME);
    }else{
        ret = alloc_chrdev_region(&imx6uirq.devid,0,IMX6UIRQ_CNT,IMX6UIRQ_NAME);
        imx6uirq.major = MAJOR(imx6uirq.devid);
        imx6uirq.minor = MINOR(imx6uirq.devid);
    }
    printk("key major = %d,minor = %d\n",imx6uirq.major,imx6uirq.minor);
    if(ret < 0){
        goto fail_devid;
    }

    /* 2.添加字符设备 */
    imx6uirq.cdev.owner = THIS_MODULE;
    cdev_init(&imx6uirq.cdev,&imx6uirq_fops);
    ret = cdev_add(&imx6uirq.cdev,imx6uirq.devid,IMX6UIRQ_CNT);
    if(ret < 0){
        goto fail_cdev;
    }

    /* 自动添加设备节点 */
    /* 1.添加类 */
    imx6uirq.class = class_create(THIS_MODULE,IMX6UIRQ_NAME);
    if(IS_ERR(imx6uirq.class)){
        ret = PTR_ERR(imx6uirq.class);
        goto fail_class;
    }

    /* 2.添加设备 */
    imx6uirq.device = device_create(imx6uirq.class,NULL,imx6uirq.devid,NULL,IMX6UIRQ_NAME);
    if(IS_ERR(imx6uirq.device)){
        ret = PTR_ERR(imx6uirq.device);
        goto fail_device;
    }

    /* 初始化按键，初始化原子变量 */
    atomic_set(&imx6uirq.keyvalue,INVAKEY);
    atomic_set(&imx6uirq.releasekey,0);
    keyio_init();

    return 0;

fail_device:
    class_destroy(imx6uirq.class);
fail_class:
    cdev_del(&imx6uirq.cdev);
fail_cdev:
    unregister_chrdev_region(imx6uirq.devid,IMX6UIRQ_CNT);
fail_devid:
    return ret;
}

/* 驱动出口函数 */
static void __exit imx6uirq_exit(void)
{
    u8 i = 0;

    /* 删除定时器 */
    del_timer_sync(&imx6uirq.timer);

    /* 释放中断和gpio */
    for(i = 0;i < KEY_NUM;i++){
        free_irq(imx6uirq.irqkeydesc[i].irqnum,&imx6uirq);
        gpio_free(imx6uirq.irqkeydesc[i].gpio);
    }

    /* 注销字符设备驱动 */
    cdev_del(&imx6uirq.cdev);    //删除cdev
    unregister_chrdev_region(imx6uirq.devid,IMX6UIRQ_CNT);    //注销设备号
    device_destroy(imx6uirq.class,imx6uirq.devid);    //摧毁设备
    class_destroy(imx6uirq.class);   //摧毁类

    return;
}

/* 注册驱动和卸载驱动 */
module_init(imx6uirq_init);
module_exit(imx6uirq_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("bcl");