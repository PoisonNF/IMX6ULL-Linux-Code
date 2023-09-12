#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <linux/watchdog.h>

#define WDOG_DEV "/dev/watchdog0"

int main(int argc,char *argv[])
{
    struct watchdog_info info;
    int timeout;
    int time;
    int fd;
    int op;

    if(argc != 2)
    {
        fprintf(stderr,"usage:%s<timeout>\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    /* 打开看门狗 */
    fd = open(WDOG_DEV,O_RDWR);
    if(fd < 0)
    {
        fprintf(stderr,"open error:%s:%s\n",WDOG_DEV,strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* 获取设备支持的功能 */
    if(ioctl(fd,WDIOC_GETSUPPORT,&info) < 0)
    {
        fprintf(stderr,"ioctl error:WDIOC_GETSUPPORT:%s\n",strerror(errno));
        return -1;
    }

    printf("identity:%s\n",info.identity);
    printf("version:%u\n",info.firmware_version);

    if(info.options & WDIOF_KEEPALIVEPING)
        printf("KEEPALIVEPING\n");
    if(info.options & WDIOF_SETTIMEOUT)
        printf("SETTIMEOUT\n");

    /* 打开之后看门狗计时器会开启，先停止 */
    op = WDIOS_DISABLECARD;
    if(ioctl(fd,WDIOC_SETOPTIONS,&op) < 0)
    {
        fprintf(stderr,"ioctl error:WDIOC_SETOPTIONS:%s\n",strerror(errno));
        exit(EXIT_FAILURE);
    }

    timeout = atoi(argv[1]);
    if(timeout < 1)
        timeout = 1;

    /* 设置超时时间 */
    printf("timeout:%ds\n",timeout);
    if(ioctl(fd,WDIOC_SETTIMEOUT,&timeout))
    {
        fprintf(stderr,"ioctl error:WDIOC_SETTIMEOUT:%s\n",strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }

    /* 开启看门狗计时器 */
    op = WDIOS_ENABLECARD;
    if(ioctl(fd,WDIOC_SETOPTIONS,&op) < 0)
    {
        fprintf(stderr,"ioctl error:WDIOC_SETOPTIONS:%s\n",strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);        
    }

    /* 喂狗 */
    time = timeout * 1000 - 100; //喂狗时间，提前喂狗

    while(1)
    {
        usleep(time);
        ioctl(fd,WDIOC_KEEPALIVE,NULL);
    }
}
