#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
int main(int argc,char *argv[1]){
    char  log_file_name[128]={0};
    sprintf(log_file_name,"%s%s",".logfile_",argv[1]);
    int fd = open(log_file_name,O_RDWR);
    off_t file_off =0;
    read(fd,&file_off,sizeof(file_off));
    printf("file_off = %ld\n",file_off);
    close(fd);
    return 0;
}