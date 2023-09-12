#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/**
 * ./ledAPP <filename> <0:1> 0��ʾ�صƣ�1��ʾ����
 * ./ledAPP /dev/dtsled 0 �ص�
 * ./ledAPP /dev/dtsled 1 ����
*/

#define LEDOFF 0 /* �ر� */
#define LEDON  1 /* ���� */

int main(int argc,char *argv[])
{
    int fd;
    int ret;
    int *ledStatus;
    char *filename;

    /* ����������� */
    if (argc != 3)
    {
        printf("Error usage\n");
        return -1;
    }

    filename = argv[1];

    /* ���ļ� */
    fd = open(filename,O_RDWR);
    if(fd < 0){
        perror("open");
        return -1;
    }

    /* д��led״̬ */
    *ledStatus = atoi(argv[2]);
    ret = write(fd,ledStatus,1);
    if(ret < 0){
        perror("write");
        close(fd);
        return -1;
    }

    /* �ر��ļ� */
    ret = close(fd);
    if(ret){
        perror("read");
        return -1;
    }
    
    return 0;
}