#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/**
 * ./chrdevbaseAPP <filename> <1:2> 1��ʾ����2��ʾд
 * ./chrdevbaseAPP /dev/chrdevbase 1 ��ʾ���������������
 * ./chrdevbaseAPP /dev/chrdevbase 2 ��ʾ����������д����
*/
int main(int argc,char *argv[])
{
    int fd;
    int ret;
    char *filename;
    char readBuf[100],writeBuf[100];
    static char usrdata[] = {"usr data!\n"};

    filename = argv[1];

    if(argc != 3){
        printf("Error usage!\n");
        return -1;
    }

    /* open */
    fd = open(filename,O_RDWR);
    if(fd < 0){
        perror("open");
        return -1;
    }

    /* read */
    if(atoi(argv[2]) == 1)
    {
        ret = read(fd,readBuf,100);
        if(ret < 0){
            perror("read");
            return -1;
        }
        else{
            printf("APP read data:%s",readBuf);
        }
    }

    /* write */
    if(atoi(argv[2]) == 2)
    {
        memcpy(writeBuf,usrdata,sizeof(usrdata));   //����usrdata����
        ret = write(fd,writeBuf,100);
        if(ret < 0){
            perror("write");
            return -1;
        }
    }

    /* close */
    ret = close(fd);
    if(ret < 0){
        perror("close");
        return -1;
    }

    return 0;
}