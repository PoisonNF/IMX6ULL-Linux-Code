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

#define DTSLED_CNT  1           /* 设备号个数 */
#define DTSLED_NAME "dtsled"    /* 设备名字 */

/* 虚拟地址 */
static void __iomem *IMX6ULL_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

#define LEDOFF 0 /* 关闭 */
#define LEDON  1 /* 开启 */

/* led设备结构体 */
struct dtsled_dev{
    dev_t devid;            /* 设备号 */
    int major;              /* 主设备号 */
    int minor;              /* 次设备号 */
    struct cdev cdev;       /* 设备结构体 */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    struct device_node *nd; /* 设备节点 */
};

struct dtsled_dev dtsled;   /* led设备 */

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

static int dtsled_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &dtsled;   //设置私有属性
    return 0;
}

static int dtsled_close(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t dtsled_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
    u32 ret;
    u8 dataBuf[1];

    //struct dtsled_dev *dev = (struct dtsled_dev*)filp->private_data;

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
static const struct file_operations dtsled_fops = {
    .owner   = THIS_MODULE,
    .open    = dtsled_open,
    .release = dtsled_close,
    .write   = dtsled_write,
};

/* 入口 */
static int __init dtsled_init(void)
{
    int ret = 0;
    struct property *proper;
    const char *str;
    u32 regdata[14];
    u32 val;

    /* 注册字符设备 */
    /* 1.申请设备号 */
    dtsled.major = 0;   //由内核分配
    if(dtsled.major){   //给定设备号
        dtsled.devid = MKDEV(dtsled.major,0);
        ret = register_chrdev_region(dtsled.devid,DTSLED_CNT,DTSLED_NAME);
    }else{  //没有给定设备号
        ret = alloc_chrdev_region(&dtsled.devid,0,DTSLED_CNT,DTSLED_NAME);
        dtsled.major = MAJOR(dtsled.devid);
        dtsled.minor = MINOR(dtsled.devid);
    }
    printk("dtsled major = %d,minor = %d\n",dtsled.major,dtsled.minor);
    if(ret < 0){
        goto fail_devid;
    }

    /* 2.添加字符设备 */
    dtsled.cdev.owner = THIS_MODULE;
    cdev_init(&dtsled.cdev,&dtsled_fops);
    ret = cdev_add(&dtsled.cdev,dtsled.devid,DTSLED_CNT);
    if(ret < 0){
        goto fail_cdev;
    }

    /* 自动添加设备节点 */
    /* 1.添加类 */
    dtsled.class = class_create(THIS_MODULE,DTSLED_NAME);
    if(IS_ERR(dtsled.class)){
        ret = PTR_ERR(dtsled.class);
        goto fail_class;
    }
    /* 2.添加设备 */
    dtsled.device = device_create(dtsled.class,NULL,dtsled.devid,NULL,DTSLED_NAME);
    if(IS_ERR(dtsled.device)){
        ret = PTR_ERR(dtsled.device);
        goto fail_device;
    }

    /* 获取设备树内容 */
    /* 1.获取设备节点 */
    dtsled.nd = of_find_node_by_path("/alphaled");
    if(dtsled.nd == NULL){
        ret = -EINVAL;
        goto fail_findnd;
    }

    /* 2.获取compatible属性 */
    proper = of_find_property(dtsled.nd,"compatible",NULL);
    if(proper == NULL){
        printk("compatible property find failed\r\n");
        goto fail_rs;
    }else{
        printk("compatible = %s\n",(char *)proper->value);
    }

    /* 3.获取status属性 */
    ret = of_property_read_string(dtsled.nd,"status",&str);
    if(ret < 0){
        printk("status read failed\n");
        goto fail_rs;
    }else{
        printk("status = %s\n",str);
    }

    /* 4.获取reg属性 */
    ret = of_property_read_u32_array(dtsled.nd,"reg",regdata,10);
    if(ret < 0){
        printk("reg property read failed\n");
        goto fail_rs;
    }else{
        u8 i = 0;
        printk("reg data:\n");
        for(i = 0;i < 10;i++)
            printk("%#X ",regdata[i]);
        printk("\n");
    }
    
    /* 初始化LED灯、地址映射、32位是4个字节 */
#if 0
    IMX6ULL_CCM_CCGR1 = ioremap(regdata[0],regdata[1]);
    SW_MUX_GPIO1_IO03 = ioremap(regdata[2],regdata[3]);
    SW_PAD_GPIO1_IO03 = ioremap(regdata[4],regdata[5]);
    GPIO1_DR = ioremap(regdata[6],regdata[7]);
    GPIO1_GDIR = ioremap(regdata[8],regdata[9]);
#else
    IMX6ULL_CCM_CCGR1 = of_iomap(dtsled.nd,0);
    SW_MUX_GPIO1_IO03 = of_iomap(dtsled.nd,1);
    SW_PAD_GPIO1_IO03 = of_iomap(dtsled.nd,2);
    GPIO1_DR = of_iomap(dtsled.nd,3);
    GPIO1_GDIR = of_iomap(dtsled.nd,4);
#endif

    /* 使能 GPIO1 时钟 */
    val = readl(IMX6ULL_CCM_CCGR1);
    val &= ~(3 << 26);  //先清除bit26、27位
    val |= (3 << 27);   //bit26、27位置1
    writel(val,IMX6ULL_CCM_CCGR1);

    /* 设置 GPIO1_IO03 的复用功能，最后设置 IO 属性 */
    writel(0x5,SW_MUX_GPIO1_IO03);  //设置复用
    writel(0x10b0,SW_PAD_GPIO1_IO03); //设置电气属性

    /* 设置 GPIO1_IO03 为输出功能 */
    val = readl(GPIO1_GDIR);
    val |= 1 << 3;  //bit3置1，设置为输出
    writel(val,GPIO1_GDIR);

    /* 默认关闭 LED */
    val = readl(GPIO1_DR);
    val |= (1 << 3);  //bit3置1，默认熄灭LED
    writel(val,GPIO1_DR);

    return 0;

fail_rs:

fail_findnd:
    device_destroy(dtsled.class,dtsled.devid);  //摧毁设备
fail_device:
    class_destroy(dtsled.class);    //摧毁类
fail_class:
    cdev_del(&dtsled.cdev); //删除cdev
fail_cdev:
    unregister_chrdev_region(dtsled.devid,DTSLED_CNT);
fail_devid:
    return ret;
}

/* 出口 */
static void __exit dtsled_exit(void)
{
    /* 关闭LED */
    led_switch(LEDOFF);

    /* 取消地址映射 */
    iounmap(IMX6ULL_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

    /* 注销字符设备驱动 */
    cdev_del(&dtsled.cdev); //删除cdev
    unregister_chrdev_region(dtsled.devid,DTSLED_CNT);  //注销设备号

    device_destroy(dtsled.class,dtsled.devid);  //摧毁设备
    class_destroy(dtsled.class);    //摧毁类

    return;
}

/* 注册驱动和卸载驱动 */
module_init(dtsled_init);
module_exit(dtsled_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("bcl");