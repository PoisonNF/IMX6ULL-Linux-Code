/* 
    ./read_input /dev/input/event1
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/input.h>

int main(int argc,char *argv[])
{
    struct input_event in_ev = {0};
    int fd = -1;

    /* 参数校验 */
    if(argc != 2)
    {
        fprintf(stderr,"usage:%s<input-dev>\n",argv[0]);
        exit(-1);
    }

    /* 打开文件 */
    if((fd = open(argv[1],O_RDONLY)) < 0)
    {
        perror("open error");
        exit(-1);
    }

    while(1)
    {
        /* 循环读取数据 */
        if(sizeof(struct input_event) != 
                read(fd,&in_ev,sizeof(struct input_event)))
        {
            perror("read error");
            exit(-1);
        }

        printf("type:%d code:%d value:%d\n",
                    in_ev.type,in_ev.code,in_ev.value);
    }
}