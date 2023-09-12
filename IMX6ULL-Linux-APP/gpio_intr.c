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

    /* �ж�ָ����ŵ�IO�Ƿ񱻵��� */
    sprintf(gpio_path,"/sys/class/gpio/gpio%s",argv[1]);
    if(access(gpio_path,F_OK))  //���Ŀ¼�����ڣ�����Ҫ����
    {
        int fd;
        int len;

        if(0 > (fd = open("/sys/class/gpio/export",O_WRONLY)))//exportֻд
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

    /* ����Ϊ����ģʽ */
    if(gpio_config("direction","in"))
        exit(-1);
    
    /* ����ģʽ */
    if(gpio_config("active_low","0"))
        exit(-1);

    /* ����Ϊ�ж�ģʽ�������غ��½��� */
    if(gpio_config("edge","both"))
        exit(-1);

    /* ��value�����ļ� */
    sprintf(file_path,"%s/%s",gpio_path,"value");

    if((pfd.fd = open(file_path,O_RDONLY)) < 0)
    {
        perror("open error");
        exit(-1);
    }

    pfd.events = POLLPRI;   //ֻ���ĸ����ȼ����ݿɶ����жϣ�
    read(pfd.fd,&val,1);    //�ȶ�ȡһ�����״̬
    while(1)
    {
        ret = poll(&pfd,1,-1);  //��Զ�ȴ�ֱ���¼�����
        if(ret < 0) //����
        {
            perror("poll error");
            exit(-1);
        }
        else if(ret == 0)   //��ʱǰû���κ��¼�����
        {
            fprintf(stderr,"poll timeout.\n");
            continue;
        }

        /* У������ȼ������Ƿ�ɶ� */
        if(pfd.revents & POLLPRI)
        {
            if(lseek(pfd.fd,0,SEEK_SET) < 0)    //����λ���ƶ���ͷ��
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