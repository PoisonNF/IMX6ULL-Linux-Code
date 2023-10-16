#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#define LED_MAJOR   200
#define LED_NAME    "led"

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
    return 0;
}

static int led_close(struct inode *inode, struct file *filp)
{
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

static const struct file_operations led_fops = {
    .owner   = THIS_MODULE,
    .open    = led_open,
    .release = led_close,
    .write   = led_write,
};

/* 入口 */
static int __init led_init(void)
{
    u32 ret = 0;
    u32 val = 0;

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

    /* 注册字符设备 */
    ret = register_chrdev(LED_MAJOR,LED_NAME,&led_fops);
    if(ret < 0)
    {
        printk("register_chrdev failed!\n");
        return -EIO;
    }

    printk("led_init\n");
    return 0;
}

/* 出口 */
static void __exit led_exit(void)
{
    /* 取消地址映射 */
    iounmap(IMX6ULL_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

    /* 注销字符设备 */
    unregister_chrdev(LED_MAJOR,LED_NAME);
    printk("led_exit\n");
}

/* 注册驱动加载和卸载 */
module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("bcl");