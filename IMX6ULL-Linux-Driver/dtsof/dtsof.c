#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/slab.h>

/*
	backlight {
		compatible = "pwm-backlight";
		pwms = <&pwm1 0 5000000>;
		brightness-levels = <0 4 8 16 32 64 128 255>;
		default-brightness-level = <6>;
		status = "okay";
	};
*/

/* 模块入口 */
static int __init disof_init(void)
{
    int ret = 0;
    struct device_node *bl_nd = NULL; /* 节点 */
    struct property *comppro = NULL;
    const char *str;
    u32 def_value = 0;
    u32 elemsize;
    u32 *brival;
    u8 i = 0;

    printk("disof_init\n");

    /* 找到backlight节点 路径是:/backlight */
    bl_nd = of_find_node_by_path("/backlight");
    if(bl_nd == NULL){
        ret = -EINVAL;
        goto fail_findnd;
    }

    /* 获取字符串属性 */
    comppro = of_find_property(bl_nd,"compatible",NULL);
    if(comppro == NULL){
        ret = -EINVAL;
        goto fail_findpro;
    }else{
        printk("compatible = %s\n",(char *)comppro->value);
    }

    ret = of_property_read_string(bl_nd,"status",&str);
    if(ret < 0){
        goto fail_rs;
    }else{
        printk("status = %s\n",str);
    }

    /* 获取数字属性 */
    ret = of_property_read_u32(bl_nd,"default-brightness-level",&def_value);
    if(ret < 0){
        goto fail_read32;
    }else{
        printk("default-brightness-level = %d\n",def_value);
    }

    /* 获取数组属性 */
    elemsize = of_property_count_elems_of_size(bl_nd,"brightness-levels",sizeof(u32));
    if(elemsize < 0){
        ret = -EINVAL;
        goto fail_readele;
    }else{
        printk("brightness-levels elems size = %d\n",elemsize);
    }

    brival = kmalloc(elemsize*sizeof(u32),GFP_KERNEL);
    if(!brival){
        ret = -EINVAL;
        goto fail_mem;
    }

    ret = of_property_read_u32_array(bl_nd,"brightness-levels",brival,elemsize);
    if(ret < 0){
        goto fail_read32array;
    }else{
        for(i = 0;i < elemsize;i++)
            printk("brightness-levels[%d] = %d\n",i,*(brival+i));
    }
    kfree(brival);

    return 0;

fail_read32array:
    kfree(brival);
fail_mem:
fail_readele:
fail_read32:
fail_rs:
fail_findpro:
fail_findnd:
    return ret;
}

/* 模块出口 */
static void __exit disof_exit(void)
{
    printk("disof_exit\n");
    return;
}

/* 模块入口和出口 */
module_init(disof_init);
module_exit(disof_exit);

/* 注册模块入口和出口 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("bcl");
