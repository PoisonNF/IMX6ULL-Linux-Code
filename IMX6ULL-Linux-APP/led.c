#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define LED_TRIGGER "/sys/class/leds/sys-led/trigger"
#define LED_BRIGHTNESS "/sys/class/leds/sys-led/brightness"
#define USAGE() fprintf(stderr,"usage:\n"\
                        "%s<on|off>\n"\
                        "%s<trigger><type>\n",argv[0],argv[0])

int main(int argc,char *argv[])
{
    int fd1,fd2;

    /* 参数检验 */
    if(argc < 2)
    {
        USAGE();
        exit(-1);
    }

    /* 打开文件 */
    fd1 = open(LED_TRIGGER,O_RDWR);
    if(fd1 < 0)
    {
        perror("open error");
        exit(-1);
    }

    fd2 = open(LED_BRIGHTNESS,O_RDWR);
    if(fd2 < 0)
    {
        perror("open error");
        exit(-1);
    }

    /* 根据参数控制LED */
    if(!strcmp(argv[1],"on"))
    {
        write(fd1,"none",4);    //先将触发模式改为none
        write(fd2,"1",1);       //点亮LED
    }
    else if(!strcmp(argv[1],"off"))
    {
        write(fd1,"none",4);    //先将触发模式改为none
        write(fd2,"0",1);       //熄灭LED
    }
    else if(!strcmp(argv[1],"trigger"))
    {
        if(argc != 3)
        {
            USAGE();
            exit(-1);
        }
        if(write(fd1,argv[2],strlen(argv[2])) < 0)
            perror("write error");
    }
    else
        USAGE();
    
    exit(0);
}