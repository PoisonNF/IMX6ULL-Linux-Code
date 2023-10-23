#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <signal.h>

/**
 * ./asyncnotiAPP <filename> 
 * ./asyncnotiAPP /dev/asyncnoti
*/

static int fd;

void sigio_signal_func(int signum)
{
    int err = 0;
    unsigned int keyvalue = 0;

    err = read(fd,&keyvalue,sizeof(keyvalue));
    if(err < 0){
        //读取错误
    }else{
        printf("sigio signal! key value = %d\n",keyvalue);
    }
}

int main(int argc,char *argv[])
{

    int ret;
    int flags = 0;
    char *filename;

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

    /* 设置信号 SIGIO 的处理函数 */
    signal(SIGIO,sigio_signal_func);

    fcntl(fd,F_SETOWN,getpid());    //将当前进程的进程号告诉内核
    flags = fcntl(fd,F_GETFD);      //获取当前的进程状态
    fcntl(fd,F_SETFL,flags|FASYNC); //设置进程启用异步通知功能

    while(1){
        sleep(2);
    }

    /* 关闭文件 */
    ret = close(fd);
    if(ret){
        perror("close");
        return -1;
    }

    return 0;
}