#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

/**
 * ./timerAPP <filename>    //定时器测试程序
 * ./timerAPP /dev/timer
*/

/* 命令值 */
#define CLOSE_CMD       (_IO(0XEF,0x1)) /* 关闭定时器 */
#define OPEN_CMD        (_IO(0XEF,0x2)) /* 打开定时器 */
#define SETPERIOD_CMD   (_IO(0XEF,0x3)) /* 设置定时器周期命令 */

int main(int argc,char *argv[])
{
    int fd;
    int ret;
    char *filename;

    unsigned int cmd;
    unsigned int arg;
    unsigned char str[100];

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

    while(1)
    {
        printf("Input CMD:");
        ret = scanf("%d",&cmd);
        if(ret != 1){       //参数输入错误
            gets(str);      //防止卡死
        }

        if(cmd == 1)
            cmd = CLOSE_CMD;
        else if(cmd == 2)
            cmd = OPEN_CMD;
        else if(cmd == 3){
            cmd = SETPERIOD_CMD;
            printf("Input Timer Period:");
            ret = scanf("%d",&arg);

            if(ret != 1){       //参数输入错误
                gets(str);      //防止卡死
            }
        }

        ioctl(fd,cmd,arg);

    }

    /* 关闭文件 */
    ret = close(fd);
    if(ret){
        perror("close");
        return -1;
    }
    
    return 0;
}