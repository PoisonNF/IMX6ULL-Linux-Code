#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/**
 * ./ledAPP <filename> <0:1> 0表示关灯，1表示开灯
 * ./ledAPP /dev/newchrled 0 关灯
 * ./ledAPP /dev/newchrled 1 开灯
*/

#define LEDOFF 0 /* 关闭 */
#define LEDON  1 /* 开启 */

int main(int argc,char *argv[])
{
    int fd;
    int ret;
    int databuf[1];
    char *filename;

    /* 参数数量检测 */
    if (argc != 3)
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

    /* 写入led状态 */
    databuf[0] = atoi(argv[2]);
    ret = write(fd,databuf,1);
    if(ret < 0){
        perror("write");
        close(fd);
        return -1;
    }

    /* 关闭文件 */
    ret = close(fd);
    if(ret){
        perror("close");
        return -1;
    }
    
    return 0;
}