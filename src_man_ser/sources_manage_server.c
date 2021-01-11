#include "net_work_disk.h"
int main()
{
    MYSQL *net_disk_mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    int ret = database_init(&net_disk_mysql); //初始化数据库连接 里面有三个表
    int fd_listen;
    tcp_init(&fd_listen); //初始化tcp连接
    printf("fd_listen = %d\n", fd_listen);
    int fd_client;
    Login_t request;
    Multi_point_t multi_info;
    off_t divider = 0;
    while (1)
    {
        bzero(&multi_info, sizeof(multi_info));
        fd_client = accept(fd_listen, NULL, NULL);
        bzero(&request, sizeof(request));
        recv(fd_client, &request, sizeof(request), MSG_WAITALL);
        pLogin_t pNewLogin = &request;
        //验证登录权限
        char query[500] = {0};
        MYSQL_RES *res;
        MYSQL_ROW row;
        int cur_dir_code = 0;
        off_t file_size = 0;
        char md5_code[33] = {0};
        char file_type[2] = {0};
        sprintf(query, "select token,cur_dir_code from usr_info where usr_name = '%s'", pNewLogin->usr);
        int query_ret = mysql_query(net_disk_mysql, query);
        printf("query = %s\n", query);
        if (0 == query_ret)
        {
            res = mysql_store_result(net_disk_mysql);
            row = mysql_fetch_row(res);
            if (NULL != row) //查找到结果
            {
                if (0 == strcmp(row[0], pNewLogin->crypt_passwd)) //验证成功
                {
                    //开始搜索文件资源情况
                    cur_dir_code = atoi(row[1]);
                    printf("rwo[0]=%s  recvtoken = %s cur_dis_coed = %d\n", row[0], pNewLogin->crypt_passwd, cur_dir_code);
                    sprintf(query, "select md5_code,file_size,file_type from file_table where owner = '%s' and precode =%d and file_name = '%s'", pNewLogin->usr, cur_dir_code, pNewLogin->file_name);
                    query_ret = mysql_query(net_disk_mysql, query);
                    res = mysql_store_result(net_disk_mysql);
                    row = mysql_fetch_row(res);
                    printf("%s\n", query);
                    if (NULL != row)
                    {
                        strncpy(md5_code, row[0], sizeof(md5_code));
                        file_size = atoi(row[1]);
                        strncpy(file_type, row[2], sizeof(file_type));
                        printf("in table file_size =%ld md5_code = %s \nfile_size = %ld file_off = %ld file_name = %s",
                               file_size, md5_code, pNewLogin->file_size, pNewLogin->file_off, pNewLogin->file_name);
                        if (0 == strcmp(file_type, "f") && (0 == pNewLogin->file_size || file_size == pNewLogin->file_size))
                        { //查找成功
                            //查找成功,开始计算文件 多点下载情况 并且传输给客户端
                            //搜索文件服务器情况
                            divider = file_size / 2;
                            multi_info.flag = 1;
                            multi_info.file_size = file_size;
                            multi_info.file_off_1 = 0; //0到divider-1 总共长度为 divider的文件内容由第一个服务器提供
                            multi_info.file_len_1 = divider;
                            multi_info.file_off_2 = divider; // divider到file_size-1个长度的文件内容由第二个服务器提供
                            multi_info.file_len_2 = file_size - divider;
                            strcpy(multi_info.ip_1, IP);
                            strcpy(multi_info.ip_2, IP);
                            strcpy(multi_info.port_1, "5555");
                            strcpy(multi_info.port_2, "5000");
                            send(fd_client, &multi_info, sizeof(multi_info), 0);
                            printf("send src situtations to %s\n", pNewLogin->usr);
                            close(fd_client);
                            continue;
                        }
                    }
                }
            }
        }
        multi_info.flag = -1;
        send(fd_client, &multi_info, sizeof(multi_info), 0);
        close(fd_client);
    }
}
int database_init(MYSQL **ppConn)
{
    MYSQL *conn;
    MYSQL_RES *result;
    MYSQL_ROW row;
    char *server = "localhost";
    char *user = "root";
    char *password = "123456";
    char *database = "net_disk";
    conn = mysql_init(NULL);
    //失败的时候返回空指针,成功的时候返回非空指针
    if (mysql_real_connect(conn, server, user, password, database, 0, NULL, 0))
    {
        *ppConn = conn; //将初始化的MYSQL指针传出去
        printf("mysql connected\n");
        return 0;
    }
    else
    {
        return -1;
    }
}
int tcp_init(int *pFd)
{
    //socket
    *pFd = socket(AF_INET, SOCK_STREAM, 0);
    //reuse
    int reuseAddr = 1;
    setsockopt(*pFd, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));
    //bind
    struct sockaddr_in serverAddr;
    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(IP);
    serverAddr.sin_port = htons(atoi("4000"));
    int ret = bind(*pFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    ERROR_CHECK(ret, -1, "bind");
    //listen
    listen(*pFd, LISTEN_CNT);
    return ret;
}
