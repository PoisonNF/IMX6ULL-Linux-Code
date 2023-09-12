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

#define DTSLED_CNT  1           /* �豸�Ÿ��� */
#define DTSLED_NAME "dtsled"    /* �豸���� */

/* �����ַ */
static void __iomem *IMX6ULL_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

#define LEDOFF 0 /* �ر� */
#define LEDON  1 /* ���� */

/* led�豸�ṹ�� */
struct dtsled_dev{
    dev_t devid;            /* �豸�� */
    int major;              /* ���豸�� */
    int minor;              /* ���豸�� */
    struct cdev cdev;       /* �豸�ṹ�� */
    struct class *class;    /* �� */
    struct device *device;  /* �豸 */
    struct device_node *nd; /* �豸�ڵ� */
};

struct dtsled_dev dtsled;   /* led�豸 */

static void led_switch(u8 sta)
{
    u32 val = 0;

    if(sta == LEDON){
        val = readl(GPIO1_DR);
        val &= ~(1 << 3);  //bit3��0������LED
        writel(val,GPIO1_DR);  
    }else{
        val = readl(GPIO1_DR);
        val |= (1 << 3);  //bit3��1��Ϩ��LED
        writel(val,GPIO1_DR);  
    }
}

static int dtsled_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &dtsled;   //����˽������
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

    //����dataBuf�жϿ��ƻ��ǹص�
    led_switch(dataBuf[0]);
    
    return 0;
}

/* �豸�������� */
static const struct file_operations dtsled_fops = {
    .owner   = THIS_MODULE,
    .open    = dtsled_open,
    .release = dtsled_close,
    .write   = dtsled_write,
};

/* ��� */
static int __init dtsled_init(void)
{
    int ret = 0;
    struct property *proper;
    const char *str;
    u32 regdata[14];
    u32 val;

    /* ע���ַ��豸 */
    /* 1.�����豸�� */
    dtsled.major = 0;   //���ں˷���
    if(dtsled.major){   //�����豸��
        dtsled.devid = MKDEV(dtsled.major,0);
        ret = register_chrdev_region(dtsled.devid,DTSLED_CNT,DTSLED_NAME);
    }else{  //û�и����豸��
        ret = alloc_chrdev_region(&dtsled.devid,0,DTSLED_CNT,DTSLED_NAME);
        dtsled.major = MAJOR(dtsled.devid);
        dtsled.minor = MINOR(dtsled.devid);
    }
    printk("dtsled major = %d,minor = %d\n",dtsled.major,dtsled.minor);
    if(ret < 0){
        goto fail_devid;
    }

    /* 2.����ַ��豸 */
    dtsled.cdev.owner = THIS_MODULE;
    cdev_init(&dtsled.cdev,&dtsled_fops);
    ret = cdev_add(&dtsled.cdev,dtsled.devid,DTSLED_CNT);
    if(ret < 0){
        goto fail_cdev;
    }

    /* �Զ�����豸�ڵ� */
    /* 1.����� */
    dtsled.class = class_create(THIS_MODULE,DTSLED_NAME);
    if(IS_ERR(dtsled.class)){
        ret = PTR_ERR(dtsled.class);
        goto fail_class;
    }
    /* 2.����豸 */
    dtsled.device = device_create(dtsled.class,NULL,dtsled.devid,NULL,DTSLED_NAME);
    if(IS_ERR(dtsled.device)){
        ret = PTR_ERR(dtsled.device);
        goto fail_device;
    }

    /* ��ȡ�豸������ */
    /* 1.��ȡ�豸�ڵ� */
    dtsled.nd = of_find_node_by_path("/alphaled");
    if(dtsled.nd == NULL){
        ret = -EINVAL;
        goto fail_findnd;
    }

    /* 2.��ȡcompatible���� */
    proper = of_find_property(dtsled.nd,"compatible",NULL);
    if(proper == NULL){
        printk("compatible property find failed\r\n");
        goto fail_rs;
    }else{
        printk("compatible = %s\n",(char *)proper->value);
    }

    /* 3.��ȡstatus���� */
    ret = of_property_read_string(dtsled.nd,"status",&str);
    if(ret < 0){
        printk("status read failed\n");
        goto fail_rs;
    }else{
        printk("status = %s\n",str);
    }

    /* 4.��ȡreg���� */
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
    
    /* ��ʼ��LED�ơ���ַӳ�䡢32λ��4���ֽ� */
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

    /* ʹ�� GPIO1 ʱ�� */
    val = readl(IMX6ULL_CCM_CCGR1);
    val &= ~(3 << 26);  //�����bit26��27λ
    val |= (3 << 27);   //bit26��27λ��1
    writel(val,IMX6ULL_CCM_CCGR1);

    /* ���� GPIO1_IO03 �ĸ��ù��ܣ�������� IO ���� */
    writel(0x5,SW_MUX_GPIO1_IO03);  //���ø���
    writel(0x10b0,SW_PAD_GPIO1_IO03); //���õ�������

    /* ���� GPIO1_IO03 Ϊ������� */
    val = readl(GPIO1_GDIR);
    val |= 1 << 3;  //bit3��1������Ϊ���
    writel(val,GPIO1_GDIR);

    /* Ĭ�Ϲر� LED */
    val = readl(GPIO1_DR);
    val |= (1 << 3);  //bit3��1��Ĭ��Ϩ��LED
    writel(val,GPIO1_DR);

    return 0;

fail_rs:

fail_findnd:
    device_destroy(dtsled.class,dtsled.devid);  //�ݻ��豸
fail_device:
    class_destroy(dtsled.class);    //�ݻ���
fail_class:
    cdev_del(&dtsled.cdev); //ɾ��cdev
fail_cdev:
    unregister_chrdev_region(dtsled.devid,DTSLED_CNT);
fail_devid:
    return ret;
}

/* ���� */
static void __exit dtsled_exit(void)
{
    /* �ر�LED */
    led_switch(LEDOFF);

    /* ȡ����ַӳ�� */
    iounmap(IMX6ULL_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

    /* ע���ַ��豸���� */
    cdev_del(&dtsled.cdev); //ɾ��cdev
    unregister_chrdev_region(dtsled.devid,DTSLED_CNT);  //ע���豸��

    device_destroy(dtsled.class,dtsled.devid);  //�ݻ��豸
    class_destroy(dtsled.class);    //�ݻ���

    return;
}

/* ע��������ж������ */
module_init(dtsled_init);
module_exit(dtsled_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("bcl");