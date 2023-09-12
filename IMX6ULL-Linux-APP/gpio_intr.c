#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>

static char gpio_path[100];

static int gpio_config(const char *attr,const char *val)
{
    char file_path[100];
    int len;
    int fd;

    sprintf(file_path,"%s/%s",gpio_path,attr);
    if(0 > (fd = open(file_path,O_WRONLY)))
    {
        perror("open error");
        return fd;
    }

    len = strlen(val);
    if(len != write(fd,val,len))
    {
        perror("write error");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int main(int argc,char *argv[])
{
    struct pollfd pfd;
    char file_path[100];
    char val;
    int ret;

    if(2 != argc)
    {
        fprintf(stderr,"usage:%s<gpio>\n",argv[0]);
        exit(-1);
    }

    /* 判断指定编号的IO是否被导出 */
    sprintf(gpio_path,"/sys/class/gpio/gpio%s",argv[1]);
    if(access(gpio_path,F_OK))  //如果目录不存在，则需要导出
    {
        int fd;
        int len;

        if(0 > (fd = open("/sys/class/gpio/export",O_WRONLY)))//export只写
        {
            perror("open error");
            exit(-1);
        }

        len = strlen(argv[1]);
        if(len != write(fd,argv[1],len))
        {
            perror("write error");
            close(fd);
            exit(-1);
        }

        close(fd);
    }

    /* 配置为输入模式 */
    if(gpio_config("direction","in"))
        exit(-1);
    
    /* 极性模式 */
    if(gpio_config("active_low","0"))
        exit(-1);

    /* 配置为中断模式，上升沿和下降沿 */
    if(gpio_config("edge","both"))
        exit(-1);

    /* 打开value属性文件 */
    sprintf(file_path,"%s/%s",gpio_path,"value");

    if((pfd.fd = open(file_path,O_RDONLY)) < 0)
    {
        perror("open error");
        exit(-1);
    }

    pfd.events = POLLPRI;   //只关心高优先级数据可读（中断）
    read(pfd.fd,&val,1);    //先读取一次清除状态
    while(1)
    {
        ret = poll(&pfd,1,-1);  //永远等待直到事件发生
        if(ret < 0) //出错
        {
            perror("poll error");
            exit(-1);
        }
        else if(ret == 0)   //超时前没有任何事件发生
        {
            fprintf(stderr,"poll timeout.\n");
            continue;
        }

        /* 校验高优先级数据是否可读 */
        if(pfd.revents & POLLPRI)
        {
            if(lseek(pfd.fd,0,SEEK_SET) < 0)    //将读位置移动到头部
            {
                perror("lseek error");
                exit(-1);
            }

            if(read(pfd.fd,&val,1) < 0)
            {
                perror("read error");
                exit(-1);
            }

            printf("GPIO_intr<value = %c>\n",val);
        }
    }
    
    exit(0);
}