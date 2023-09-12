/* 
    ./read_key /dev/input/event1
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

    /* ����У�� */
    if(argc != 2)
    {
        fprintf(stderr,"usage:%s<input-dev>\n",argv[0]);
        exit(-1);
    }

    /* ���ļ� */
    if((fd = open(argv[1],O_RDONLY)) < 0)
    {
        perror("open error");
        exit(-1);
    }

    while(1)
    {
        /* ѭ����ȡ���� */
        if(sizeof(struct input_event) != 
                read(fd,&in_ev,sizeof(struct input_event)))
        {
            perror("read error");
            exit(-1);
        }

        if(in_ev.type == EV_KEY)
        {
            switch(in_ev.value)
            {
                case 0:
                    printf("code<%d>: release\n",in_ev.code);
                    break;
                case 1:
                    printf("code<%d>: press\n",in_ev.code);
                    break;
                case 2:
                    printf("code<%d>: hold\n",in_ev.code);
                    break;
            }
        }
    }
}