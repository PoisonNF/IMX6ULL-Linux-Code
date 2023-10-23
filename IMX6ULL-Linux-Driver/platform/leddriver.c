#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#define LEDDEV_CNT      1
#define LEDDEV_NAME     "platformled"   /* 设备名字 */
#define LEDOFF 0                        /* 关闭 */
#define LEDON  1                        /* 开启 */

/* 虚拟地址 */
static void __iomem *IMX6ULL_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

/* LED设备结构体 */
struct leddev_dev{
    struct cdev cdev;       /* 字符设备 */
    dev_t devid;            /* 设备号 */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    int major;              /* 主设备号 */
    int minor;              /* 次设备号 */
};

struct leddev_dev leddev; /* led设备 */

static void led_switch(u8 sta)
{
    u32 val = 0;

    if(sta == LEDON){
        val = readl(GPIO1_DR);
        val &= ~(1 << 3);  //bit3置0，点亮LED
        writel(val,GPIO1_DR);  
    }else{
        val = readl(GPIO1_DR);
        val |= (1 << 3);  //bit3置1，熄灭LED
        writel(val,GPIO1_DR);  
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
    int i = 0;
    struct resource *ledresource[5];
    int ressize[5];
    u32 val = 0;
    int ret = 0;

    printk("led driver and device has matched!\n");
    printk("get resource num = %d\n",dev->num_resources);

    /* 获取设备资源 */
    for(i = 0;i < dev->num_resources;i++){
        ledresource[i] = platform_get_resource(dev,IORESOURCE_MEM,i);
        if(!ledresource[i]){
            dev_err(&dev->dev,"No MEM resource for always on\n");
            return -ENXIO;
        }
        ressize[i] = resource_size(ledresource[i]);
    }

    /* 初始化LED */

    /* 初始化LED灯、地址映射、32位是4个字节 */
    IMX6ULL_CCM_CCGR1 = ioremap(ledresource[0]->start,ressize[0]);
    SW_MUX_GPIO1_IO03 = ioremap(ledresource[1]->start,ressize[1]);
    SW_PAD_GPIO1_IO03 = ioremap(ledresource[2]->start,ressize[2]);
    GPIO1_DR = ioremap(ledresource[3]->start,ressize[3]);
    GPIO1_GDIR = ioremap(ledresource[4]->start,ressize[4]);

    /* 初始化时钟 */
    val = readl(IMX6ULL_CCM_CCGR1);
    val &= ~(3 << 26);  //先清除bit26、27位
    val |= (3 << 27);   //bit26、27位置1
    writel(val,IMX6ULL_CCM_CCGR1);

    writel(0x5,SW_MUX_GPIO1_IO03);  //设置复用
    writel(0x10b0,SW_PAD_GPIO1_IO03); //设置电气属性

    val = readl(GPIO1_GDIR);
    val |= 1 << 3;  //bit3置1，设置为输出
    writel(val,GPIO1_GDIR);

    val = readl(GPIO1_DR);
    val &= ~(1 << 3);  //bit3置0，点亮LED
    writel(val,GPIO1_DR);

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

    return 0;

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
    /* 取消地址映射 */
    iounmap(IMX6ULL_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

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

/* platform 驱动结构体 */
static struct platform_driver led_driver = {
    .driver = {
        .name = "platform-led",
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