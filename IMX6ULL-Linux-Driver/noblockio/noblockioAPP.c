#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <time.h>
#include <sys/types.h>
#include <sys/select.h>


/**
 * ./noblockioAPP <filename> 
 * ./noblockioAPP /dev/noblockio
*/

int main(int argc,char *argv[])
{
    int fd;
    int ret;
    char *filename;
    struct pollfd fds;
    fd_set readfds;
    struct timeval timeout;
    unsigned char data;

    /* 参数数量检测 */
    if (argc != 2)
    {
        printf("Error usage\n");
        return -1;
    }

    filename = argv[1];

    /* 打开文件 */
    fd = open(filename,O_RDWR | O_NONBLOCK);    //非阻塞访问
    if(fd < 0){
        perror("open");
        return -1;
    }

#if 0
    /* 使用poll函数 */
    /* 构造结构体 */
    fds.fd = fd;
    fds.events = POLLIN;

    while(1){
        ret = poll(&fds,1,500);  //监视文件描述符fds，监视的文件描述符个数为1，超时时间为500ms
        if(ret){    //数据有效
            ret = read(fd,&data,sizeof(data));
            if(ret < 0){
                //读取错误
            }else{
                printf("key value = %d\n",data);
            }
        }else if(ret == 0){     //超时

        }else if(ret < 0){      //错误

        }
    }
#endif

    /* 使用select函数 */
    while(1){
        FD_ZERO(&readfds);
        FD_SET(fd,&readfds);

        /* 构造超时时间 */
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;   //500ms

        ret = select(fd+1,&readfds,NULL,NULL,&timeout);

        switch(ret){
            case 0:     //超时
                /* 用户自定义超时处理 */
                break;
            case -1:    //错误
                /* 用户自定义错误处理 */
                break;
            default:    //可以读取数据
                if(FD_ISSET(fd,&readfds)){
                    ret = read(fd,&data,sizeof(data));
                    if(ret < 0){
                        //读取错误
                    }else{
                        printf("key value = %d\n",data);
                    }
                }
                break;
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