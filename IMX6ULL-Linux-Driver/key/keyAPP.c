#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/**
 * ./keyAPP <filename> 
 * ./keyAPP /dev/key 
*/

/* 定义按键值 */
#define KEY0VALUE         0xf0           /* 按键值 */
#define INVAKEY           0x00           /* 无效按键值 */

int main(int argc,char *argv[])
{
    int fd;
    int ret;
    char *filename;
    unsigned char keyvalue;

    /* 参数数量检测 */
    if (argc != 2)
    {
        printf("Error usage\n");
        return -1;
    }

    filename = argv[1];

    /* 打开文件 */
    fd = open(filename,O_RDWR);
    if(fd < 0){
        perror("open");
        return -1;
    }

    /* 循环读取按键值数据！ */
    while(1)
    {
        read(fd,&keyvalue,sizeof(keyvalue));
        if(keyvalue == KEY0VALUE)
        {
            printf("KEY0 Press, value = %#X\r\n",keyvalue);
        }
        
    }

    /* 关闭文件 */
    ret = close(fd);
    if(ret){
        perror("close");
        return -1;
    }
    
    return 0;
}