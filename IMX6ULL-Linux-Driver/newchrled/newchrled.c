#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define NEWCHRLED_NAME "newchrled"

/* 寄存器物理地址 */
#define CCM_CCGR1_BASE              (0X020C406C)
#define SW_MUX_GPIO1_IO03_BASE      (0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE      (0X020E02F4)
#define GPIO1_DR_BASE               (0X0209C000)
#define GPIO1_GDIR_BASE             (0X0209C004)

/* 虚拟地址 */
static void __iomem *IMX6ULL_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

#define LEDOFF 0 /* 关闭 */
#define LEDON  1 /* 开启 */

/* LED设备结构体 */
struct newchrled_dev{
    struct cdev cdev;       /* 字符设备 */
    dev_t devid;            /* 设备号 */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    int major;              /* 主设备号 */
    int minor;              /* 次设备号 */
};

struct newchrled_dev newchrled; /* led设备 */

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

static int newchrled_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int newchrled_close(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t newchrled_write(struct file *filp, const char __user *buf,
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
static const struct file_operations newchrled_fops = {
    .owner   = THIS_MODULE,
    .open    = newchrled_open,
    .release = newchrled_close,
    .write   = newchrled_write,
};

/* 入口函数 */
static int __init newchrled_init(void)
{
    int ret = 0;
    u32 val = 0;

    printk("newchrled_init\n");

    /* 初始化LED灯、地址映射、32位是4个字节 */
    IMX6ULL_CCM_CCGR1 = ioremap(CCM_CCGR1_BASE,4);
    SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE,4);
    SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE,4);
    GPIO1_DR = ioremap(GPIO1_DR_BASE,4);
    GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE,4);

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
    if(newchrled.major)
    {
        newchrled.devid = MKDEV(newchrled.major,0);
        ret = register_chrdev_region(newchrled.devid,1,NEWCHRLED_NAME);
    }else{
        ret = alloc_chrdev_region(&newchrled.devid,0,1,NEWCHRLED_NAME);
        newchrled.major = MAJOR(newchrled.devid);
        newchrled.minor = MINOR(newchrled.devid);
    }

    if(ret < 0){
        printk("newchrled chrdev_region err!\n");
        goto fail_devid;
    }

    printk("newchrled major = %d,minor = %d\n",newchrled.major,newchrled.minor);
    
    /* 注册字符设备 */
    newchrled.cdev.owner = THIS_MODULE;
    cdev_init(&newchrled.cdev,&newchrled_fops);
    ret = cdev_add(&newchrled.cdev, newchrled.devid, 1);
    if(ret < 0)
        goto fail_cdev;

    /* 自动添加设备节点 */
    /* 添加类 */
    newchrled.class = class_create(THIS_MODULE,NEWCHRLED_NAME);
    if(IS_ERR(newchrled.class)){
        ret = PTR_ERR(newchrled.class);
        goto fail_class;
    }

    /* 添加设备 */
    newchrled.device = device_create(newchrled.class, NULL,
			     newchrled.devid, NULL,
			     NEWCHRLED_NAME);
    if(IS_ERR(newchrled.device)){
        ret = PTR_ERR(newchrled.device);
        goto fail_device;
    }

    return 0;

fail_device:
    device_destroy(newchrled.class,newchrled.devid);
fail_class:
    class_destroy(newchrled.class);
fail_cdev:
    unregister_chrdev_region(newchrled.devid,1);
fail_devid:
    return ret;
}

/* 出口函数 */
static void __exit newchrled_exit(void)
{
    printk("newchrled_exit\n");

    /* 取消地址映射 */
    iounmap(IMX6ULL_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

    /* 删除字符设备 */
    cdev_del(&newchrled.cdev);

    /* 注销设备号 */
    unregister_chrdev_region(newchrled.devid,1);

    /* 摧毁设备 */
    device_destroy(newchrled.class,newchrled.devid);

    /* 摧毁类 */
    class_destroy(newchrled.class);
}

/* 注册和卸载驱动 */
module_init(newchrled_init);
module_exit(newchrled_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("bcl");
