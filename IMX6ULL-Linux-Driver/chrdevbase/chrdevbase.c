#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define CHRDEVBASE_MAJOR    200               //���豸��
#define CHRDEVBASE_NAME     "chrdevbase"    //�豸��

static char readBuf[100];   /* ������ */
static char writeBuf[100];  /* д���� */
static char kernelData[] = {"kernel data!\n"};

static int chrdevbase_open(struct inode *inode,struct file *filp)
{
    printk("chrdevbase_open\n");
    return 0;
}

static int chrdevbase_release(struct inode *inode,struct file *file)
{
    printk("chrdevbase_release\n");
    return 0;
}

static ssize_t chrdevbase_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;

    printk("chrdevbase_read\n");
    memcpy(readBuf,kernelData,sizeof(kernelData));
    ret = copy_to_user(buf,readBuf,count);  //�ں˿ռ����ݵ��û��ռ�ĸ���
    if(ret != 0){
        printk("copy_to_user failed\n");
    }
    return ret;
}

static ssize_t chrdevbase_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;

    printk("chrdevbase_write\n");
    ret = copy_from_user(writeBuf,buf,count);   //�û��ռ����ݵ��ں˿ռ�ĸ���
    if(!ret){
        printk("kernel recevdata:%s\n",writeBuf);
    }else{
        printk("copy_from_user failed\n");
    }
    return ret;
}

struct file_operations chrdevbase_fops = {
    .owner   = THIS_MODULE,
    .open    = chrdevbase_open,
    .release = chrdevbase_release,
    .read    = chrdevbase_read,
    .write   = chrdevbase_write,
};

static int __init chrdevbase_init(void)
{
    int ret = 0;
    printk("chrdevbase_init\n");
    ret = register_chrdev(CHRDEVBASE_MAJOR,CHRDEVBASE_NAME,
                  &chrdevbase_fops);

    if(ret < 0){
        printk("chrdevbase init failed\n");
    }
    return 0;
}

static void __exit chrdevbase_exit(void)
{
    printk("chrdevbase_exit\n");
    unregister_chrdev(CHRDEVBASE_MAJOR,CHRDEVBASE_NAME);
}

/**
 * ģ����������
*/
module_init(chrdevbase_init);
module_exit(chrdevbase_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("bcl");
