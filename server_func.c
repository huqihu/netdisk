#include "net_work_disk.h"
int que_insert(pQue_t pQue, pTask_t pNewNode)
{
    if (NULL == pQue->pTail)
    {
        pQue->pHead = pQue->pTail = pNewNode;
#ifdef DEBUG
        printf("que_insert :pHead %p pTail %p\n", pQue->pHead, pQue->pTail);
#endif
        pQue->queSize++;
    }
    else
    {
        pQue->pTail->pNext = pNewNode;
        pQue->pTail = pNewNode;
        pQue->queSize++;
    }
    return 0;
}
int que_get(pQue_t pQue, pTask_t pNode)
{ //判断是否为空，为空则返回-1，成功拿到则返回0.拿到之后判断一下是否是唯一一个节点在做相应处理
    if (0 == pQue->queSize)
    {
        return -1;
    }
    *pNode = *(pQue->pHead);
#ifdef DEBUG
    printf("in que_et fd_client :%d\n", pNode->fd_client);
#endif
    pQue->pHead = pQue->pHead->pNext;
    if (NULL == pQue->pHead)
    { //如果现在的NULL是空的，则说明是最后一个节点被拿走了，此时应该将pTail置为NULL
        pQue->pTail = NULL;
    }
    pQue->queSize--;
    return 0;
}
int thread_pool_init(pThreadPool_t pPool)
{ //初始化的时候，需要对传进来的pool进行修改
    pPool->pthreadCnt = THREAD_CNT;
    pPool->pThid = (pthread_t *)calloc(THREAD_CNT, sizeof(pthread_t));
    pPool->startFlag = NOT_START;
    pPool->que.pHead = NULL;
    pPool->que.pTail = NULL;
    pPool->que.queSize = 0;
    pthread_mutex_init(&pPool->que.mutex, NULL);
    pthread_cond_init(&pPool->que.cond, NULL);
    return 0;
}
int thread_pool_start(pThreadPool_t pPool)
{
    pthread_t thid = 0;
    if (pPool->pthreadCnt > 20)
    { //如果输错数据，导致要创建的线程过多，就提示并且退出进程
        printf("two many threads, exit\n");
        exit(0);
    }
    for (int i = 0; i != pPool->pthreadCnt; ++i)
    { //创建子线程，记录thid
        pthread_create(&thid, NULL, thread_func, &pPool->que);
        pPool->pThid[i] = thid;
    }
    pPool->startFlag = STARTED;
    return 0;
}
void *thread_func(void *p)
{ //循环中，尝试加锁，加锁成功之后如果没有任务则等待在条件变量处
    pQue_t pQue = (pQue_t)p;
    Task_t task;
    int ret = 0;
    while (1)
    {
        pthread_mutex_lock(&pQue->mutex); //1处
        if (0 == pQue->queSize)
        {
            pthread_cond_wait(&pQue->cond, &pQue->mutex); //2处
        }
        ret = que_get(pQue, &task);
        pthread_mutex_unlock(&pQue->mutex);
        //占用公共资源的时间要尽可能得短
        //要判断一下que_get的返回值，在某些情况下，cond_wait返回之后队列中也没有任务
        //eg：A,B两个线程，来了一个任务，A线程在1处抢占了锁，同时B在2处也被唤醒但是没有抢到锁
        //A执行完任务之后，解开锁，B此时加锁进给队列，刚刚来的唯一一个任务已经被A拿走了，所以需要判断一下ret
        // 意思是一个pthread_cond_singal可能会唤醒多个条件变量
        if (-1 == ret)
        {
            continue;
        }
        else
        {
            printf("in son thread\n");
            printf("tast %s %ld %s %ld %d\n", task.order, task.file_size, task.md5_code, task.file_off, task.fd_client);
            if (0 == strcmp("puts", task.order))
            {
                recv_file_from_client(task);
            }
            else if (0 == strcmp("gets", task.order))
            {
                int fd_file = open(task.md5_code, O_RDWR);
                // 零拷贝的api mmap/sendfile 最大支持 2GB的文件
                if (task.file_size < 200 * 1024 * 1024 || task.file_size > 2147483647)
                {
                    send_file_by_train(task.fd_client, task.file_size, task.file_off, fd_file);
                }
                else
                {
                    send_file_by_mmap(task.fd_client, task.file_size, task.file_off, fd_file);
                }
            }
        }
    }
}
int recv_file_from_client(Task_t task)
{
    //接收文件,修改文件表,返回 标志位
    File_info_t file_info;
    bzero(&file_info, sizeof(file_info));
    char tem_file[50] = {0};
    char query[500] = {0};
    sprintf(tem_file, "%s%s", task.md5_code, task.usr_name);
    int fd_file = open(tem_file, O_RDWR | O_CREAT, 0666);
    ftruncate(fd_file, task.file_size);
    char *pMap = (char *)mmap(NULL, task.file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_file, 0);
    off_t ret = recv(task.fd_client, pMap, task.file_size, MSG_WAITALL);
    if (ret == task.file_size)
    { //文件接收成功 /将临时文件改名为 md5码文件,然后将信息插入文件表
    // 注意这里的rename函数的作用哈,如果new 文件已经存在,那么会直接删除原来的new 文件,然后将
    // 这个文件命名为 new文件
        rename(tem_file, task.md5_code);
        printf("文件接收完成,开始插入文件表\n");
        sprintf(query, "insert into file_table (precode,file_name,owner,md5_code,file_size,file_type,file_cnt)values (%d,'%s','%s','%s',%ld,'%s',%d)",
                task.cur_dir_code, task.file_name, task.usr_name, task.md5_code, task.file_size, "f", 1);
        printf("query = %s\n", query);
        int query_ret = mysql_query(task.task_mysql, query);
        if (query_ret == 0)
        { //插入chengong
            printf("文件插入文件表成功\n");
            file_info.file_size = 1;
            send(task.fd_client, &file_info, sizeof(file_info), 0);
            close(task.fd_client);
            return 0;
        }
    }
    else
    {
        file_info.file_size = -1;
        send(task.fd_client, &file_info, sizeof(file_info), 0);
        close(task.fd_client);
        return -1;
    }
}

int tcp_init(int *pFd)
{
    //socket
    *pFd = socket(AF_INET, SOCK_STREAM, 0);
    //reuse
    int reuseAddr = 1; 
    // 设置成服务器绑定的那个端口,即使使用 ctrl +c 结束服务器
    // 然后服务器处于time_waited状态,也能够接受新的客户端的连接
    setsockopt(*pFd, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));
    //bind
    struct sockaddr_in serverAddr;
    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(IP);
    serverAddr.sin_port = htons(atoi(PORT));
    int ret = bind(*pFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    ERROR_CHECK(ret, -1, "bind");
    //listen
    listen(*pFd, LISTEN_CNT);
    return ret;
}
int epoll_init(int *pFd, int fd_listen, int fd_timeout)
{
    *pFd = epoll_create(1); // 这个参数没有作用,但是必须大于0
    struct epoll_event event;
    bzero(&event, sizeof(event));
    event.data.fd = fd_listen;
    event.events = EPOLLIN;
    epoll_ctl(*pFd, EPOLL_CTL_ADD, fd_listen, &event);

    event.data.fd = fd_timeout;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(*pFd, EPOLL_CTL_ADD, fd_timeout, &event);
    return 0;
}
//上面的代码是线程池的,下面是网盘新加的
int add_fd_to_epfd(int epfd, int fd, int operator)
{
    struct epoll_event event;
    bzero(&event, sizeof(event));
    event.data.fd = fd;
    event.events = operator;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
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
int long_order_handle(pUsr_t pUsr, MYSQL *conn, pThreadPool_t pPool, int epfd, pTimeout_disconnet_t pCir)
{ //处理 register login puts gets
    printf("in long order handle\n");
    printf("cur fd = %d\n", pUsr->fd_sock);
    Login_t new_login;
    bzero(&new_login, sizeof(new_login));
    int ret = recv(pUsr->fd_sock, &new_login, sizeof(new_login), MSG_WAITALL);
    // if (-1 == ret)
    // { // 发生错误的时候会返回-1(不是对方主动断开连接的情况)
    //     perror("recv ret =-1");
    //     close(pUsr->fd_sock);
    //     pUsr->fd_sock = 0;
    //     pUsr->login_stat = 0;
    // }
    if (0 == ret)
    {
        // 对方断开连接会返回0
        printf("fd =%d exit\n", pUsr->fd_sock);
        circlue_queue_delete(pUsr, pCir);
        close(pUsr->fd_sock);
        pUsr->fd_sock = 0;
        pUsr->login_stat = 0;
        return -1;
    }
    if (0 == strcmp("register", new_login.order))
    {
        register_handle(pUsr, conn, new_login.crypt_passwd, new_login.usr, pCir);
    }
    else if (0 == strcmp("login", new_login.order))
    {
        login_handle(pUsr, conn, &new_login, pCir);
    }
    else if (0 == strcmp("gets", new_login.order))
    {
        gets_handle(pUsr, conn, &new_login, pPool, epfd, pCir);
    }
    else if (0 == strcmp("puts", new_login.order))
    {
        puts_handle(pUsr, conn, &new_login, pPool, epfd, pCir);
    }
}
int gets_handle(pUsr_t pUsr, MYSQL *net_disk_mysql, pLogin_t pNewLogin, pThreadPool_t pPool, int epfd, pTimeout_disconnet_t pCir)
{

    printf("in gets handle\n");
    //首先判断权限,然后判断文件是否存在,如果存在,则获取md5码,根据文件的偏移值开始传输文件
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
    if (0 == query_ret) //查询语句没有出错
    {
        res = mysql_store_result(net_disk_mysql);
        row = mysql_fetch_row(res);
        if (NULL != row) //查找到结果
        {
            if (0 == strcmp(row[0], pNewLogin->crypt_passwd)) //crypt 密码 验证成功
            {
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
                    {
                        send(pUsr->fd_sock, &file_size, sizeof(file_size), 0);
                        printf("send file_size = %ld\n", file_size);
                        //验证成功,开始发文件

                        pTask_t pTask = (pTask_t)calloc(1, sizeof(Task_t));
                        pTask->cur_dir_code = cur_dir_code;
                        pTask->fd_client = pUsr->fd_sock;
                        pTask->file_off = pNewLogin->file_off;
                        pTask->file_size = file_size;
                        strncpy(pTask->md5_code, md5_code, 33); //注意这里不能使用sizeof来判断大小
                        strncpy(pTask->order, pNewLogin->order, 10);
                        //pTask->task_mysql = net_disk_mysql;
                        //strncpy(pTask->usr_name, pNewLogin->usr, 16);
                        //strncpy(pTask->file_name, pNewLogin->file_name, 128);

                        que_insert(&pPool->que, pTask); //插入队列,唤醒一个子线程来完成传输任务
                        pthread_cond_signal(&pPool->que.cond);
                        //清理资源,但是不关闭描述符 需要将描述符从epfd监听集合中删除掉
                        //printf("epfd = %d\n",epfd);
                        //传文件的连接是重新开一个线程,在主线程中不需要保存这个连接的信息了
                        circlue_queue_delete(pUsr, pCir);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, pUsr->fd_sock, NULL);
                        bzero(pUsr, sizeof(Usr_t));
                        return 0;

                        // int fd_file = open(md5_code, O_RDWR);
                        // if (file_size < 200 * 1024 * 1024 || file_size > 2147483647)
                        // {
                        //     send_file_by_train(pUsr->fd_sock, file_size, pNewLogin->file_off, fd_file);
                        // }
                        // else
                        // {
                        //     send_file_by_mmap(pUsr->fd_sock, file_size, pNewLogin->file_off, fd_file);
                        // }
                    }
                }
            }
        }
    }
    //如果上面的确认过程中,有任何一个不符合的情况,则断开连接
    circlue_queue_delete(pUsr, pCir);
    close(pUsr->fd_sock);
    bzero(pUsr, sizeof(Usr_t));
    return -1;
}

off_t send_file_by_mmap(int fd_client, off_t file_size, off_t file_off, int fd_file)
{
    printf("filesize %ld file_off %ld fd_file %d fd_client %d\n", file_size, file_off, fd_file, fd_client);
    //测试
    //mmap的偏移值必须是4096的倍数

    char *pMap = mmap(NULL, file_size - file_off, PROT_READ | PROT_WRITE, MAP_SHARED, fd_file, file_off);
    ERROR_CHECK(pMap, (char *)-1, "mmap");
    int ret = send(fd_client, pMap, file_size - file_off, 0);
    printf("mmap send ret =%d\n", ret);
    close(fd_client);
}
int send_file_by_train(int fd_client, off_t file_size, off_t file_off, int fd_file)
{
    Train_t train;
    int ret = 0;
    //使用lseek对文件对象进行偏移
    lseek(fd_file, file_off, SEEK_SET);
    while (1)
    {
        bzero(&train, sizeof(train));
        ret = read(fd_file, train.buf, sizeof(train.buf));
        if (0 == ret)
        {
            printf("文件发送完毕\n");
            break;
        }
        train.trainLen = ret;
        ret = send(fd_client, &train, train.trainLen + sizeof(train.trainLen), 0);
        if (-1 == ret)
        {
            perror("send");
        }
    }
    close(fd_client);
}

int puts_handle(pUsr_t pUsr, MYSQL *net_disk_mysql, pLogin_t pNewLogin, pThreadPool_t pPool, int epfd, pTimeout_disconnet_t pCir)
{
    //puts和gets是使用新的tcp连接来完成的,首先确定tcp连接请求的权限,验证usr_name 以及 token值
    printf("in puts handle\n");
    char query[128] = {0};
    MYSQL_RES *res;
    MYSQL_ROW row;
    File_info_t file_info;
    int cur_dir_code = 0;
    sprintf(query, "select token,cur_dir_code from usr_info where usr_name = '%s'", pNewLogin->usr);
    printf("1");
    int query_ret = mysql_query(net_disk_mysql, query);
    printf("query = %s\n", query);
    if (0 == query_ret)
    {
        res = mysql_store_result(net_disk_mysql);
        row = mysql_fetch_row(res);
        if (NULL != row)
        {
            if (0 == strcmp(row[0], pNewLogin->crypt_passwd))
            {
                cur_dir_code = atoi(row[1]);
                printf("rwo[0]=%s  recvtoken = %s \n", row[0], pNewLogin->crypt_passwd);
                //身份验证成功,判断服务器上面有没有这个文件,如果有的话,立即发送信息并且返回,没有则将任务发送到任务队列中
                printf("recv md5 = %s\n", pNewLogin->md5_code);
                sprintf(query, "select md5_code from file_table where md5_code = '%s' limit 1", pNewLogin->md5_code);
                mysql_query(net_disk_mysql, query);
                res = mysql_store_result(net_disk_mysql);
                row = mysql_fetch_row(res);
                if (NULL != row)
                {
                    bzero(&file_info, sizeof(file_info));
                    file_info.file_size = -1; 
                    //表示相同文件已经存在了,发送标志位,关闭连接,清理资源
                    send(pUsr->fd_sock, &file_info, sizeof(file_info), 0);
                    close(pUsr->fd_sock);
                    bzero(pUsr, sizeof(Usr_t));
                    // 增加引用计数的值的大小
                    {
                        // 将记录加入到 当前用户的文件表信息之中,然后
                       // sprintf(query,"update file_table set file_cnt = file_cnt +1 where md5_code = %s",)
                        //sprintf(query, "insert into file_table (precode,file_name,owner,md5_code,file_size,file_type,file_cnt)values (%d,'%s','%s','%s',%ld,'%s',%d)");
                    }
                    return 0;
                }
                else //没有相同文件
                {
                    bzero(&file_info, sizeof(file_info));
                    file_info.file_size = 1; //没有相同文件,打包传输任务发送到任务队列,通过条件变量激活一个线程
                    send(pUsr->fd_sock, &file_info, sizeof(file_info), 0);

                    pTask_t pTask = (pTask_t)calloc(1, sizeof(Task_t));
                    pTask->cur_dir_code = cur_dir_code;
                    pTask->fd_client = pUsr->fd_sock;
                    pTask->file_off = 0;
                    pTask->file_size = pNewLogin->file_size;
                    strncpy(pTask->md5_code, pNewLogin->md5_code, 33); //注意这里不能使用sizeof来判断大小
                    strncpy(pTask->order, pNewLogin->order, 10);
                    pTask->task_mysql = net_disk_mysql;
                    strncpy(pTask->usr_name, pNewLogin->usr, 16);
                    strncpy(pTask->file_name, pNewLogin->file_name, 128);
                    que_insert(&pPool->que, pTask); //插入队列,唤醒一个子线程来完成传输任务
                    pthread_cond_signal(&pPool->que.cond);
                    //清理资源,但是不关闭描述符 需要将描述符从epfd监听集合中删除掉
                    //printf("epfd = %d\n",epfd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, pUsr->fd_sock, NULL);
                    bzero(pUsr, sizeof(Usr_t));
                    return 0;
                }
            }
        }
        //没有查找结果,立即断开
        circlue_queue_delete(pUsr, pCir);
        close(pUsr->fd_sock);
        bzero(pUsr, sizeof(Usr_t));
        return -1;
    }
}

int login_handle(pUsr_t pUsr, MYSQL *conn, pLogin_t pNewLogin, pTimeout_disconnet_t pCir)
{
    printf("in login handle\n");
    MYSQL_RES *res;
    MYSQL_ROW row;
    char usr_name[16] = {0};
    char salt[16] = {0};
    char crypt_passwd[128] = {0};
    char token[64] = {0};
    char query[128] = {0};
    int query_ret = 0;
    strcpy(usr_name, pNewLogin->usr);
    //printf("test: usr_name = %s\n", usr_name);
    //首先判断目标连接目前的状态,-2 未连接 -1 半连接 (全连接的状态不会进入这里)
    if (-2 == pUsr->login_stat)
    {// 查询用户名,发送盐值过去
        printf("fd =%d to check usr_name\n", pUsr->fd_sock);
        sprintf(query, "select salt from usr_info where usr_name = '%s'", usr_name);
        //查询用户名,获取盐值,查到之后将连接状态设置为-1,并且发送盐值,失败则断开连接
        //printf("query = %s\n", query);
        query_ret = mysql_query(conn, query);
        if (0 != query_ret) 
        //这个是用来判断查询语句有没有出问题的,并非是用来判断有没有结果的
        { 
            circlue_queue_delete(pUsr, pCir);
            close(pUsr->fd_sock);
            pUsr->fd_sock = 0; // 将这一块的空间置为0 也是一样的效果
            pUsr->login_stat = 0;
            return -1;
        }
        else
        { //通过fetch_row获取一行,通过num_filed获取列数,send salt,,,,, row本质上是 字符串 数组
            res = mysql_store_result(conn);
            if (res == NULL)
            { //如果没有结果,则断开连接,释放用户资源
                circlue_queue_delete(pUsr, pCir);
                close(pUsr->fd_sock);
                pUsr->fd_sock = 0;
                pUsr->login_stat = 0;
                printf("failed\n");
                return -1;
            }
            row = mysql_fetch_row(res);
            strcpy(salt, row[0]);
            //printf("salt = %s\n", salt);
            pUsr->login_stat = -1; //用户名验证成功,将登陆标志位设置为-1
            send(pUsr->fd_sock, salt, sizeof(salt), 0);
            printf("succeed\n");
            return 0;
        }
    }
    else if (-1 == pUsr->login_stat)
    {
        printf("fd =%d to check crypt_passwd\n", pUsr->fd_sock);
        //查询用户名的crypt_passwd并且对比,如果成功 则将状态位置为1,失败则断开连接
        sprintf(query, "select crypt_passwd,token from usr_info where usr_name = '%s'", usr_name);
        //printf("query = %s\n", query);
        query_ret = mysql_query(conn, query);
        if (0 != query_ret)
        { //查询失败,断开连接
            circlue_queue_delete(pUsr, pCir);
            close(pUsr->fd_sock);
            pUsr->fd_sock = 0; //set fd to zero in order to avoid misjudging in epoll_wait
            pUsr->login_stat = 0;
            printf("failed\n");
            return -1;
        }
        else
        { //通过fetch_row获取一行,通过num_filed获取列数,然后打印 row本质上是 字符串 数组
            //判断读到的密文和接收到的密文是否相等
            res = mysql_store_result(conn);
            if (res == NULL)
            { //如果没有结果,则断开连接,释放用户资源
                circlue_queue_delete(pUsr, pCir);
                close(pUsr->fd_sock);
                pUsr->fd_sock = 0;
                pUsr->login_stat = 0;
                return -1;
            }
            row = mysql_fetch_row(res);
            strcpy(crypt_passwd, row[0]);
            printf("crypt_passwd= %s\n", crypt_passwd);
            int ret = strcmp(crypt_passwd, pNewLogin->crypt_passwd);
            if (0 == ret)
            { //登陆成功 将用户名 标志位 设置 ,,,,,发送token
                printf("login succeed\n");
                pUsr->login_stat = 1; //密文验证成功,将登陆标志位设置为1
                bzero(pUsr->usr_name, 32);
                strncpy(pUsr->usr_name, usr_name, 16);
                generate_token(pUsr->usr_name, token);
                printf("token = %s\n", token);
                sprintf(query, "update usr_info set token = '%s' where usr_name = '%s'", token, pUsr->usr_name);
                ret = mysql_query(conn, query);
                printf("ret = %d query=%s\n", ret, query);
                ERROR_CHECK(ret, 1, "mysql_query");
                send(pUsr->fd_sock, token, sizeof(token), 0);
                printf("succeed\n");
                return 0;
            }
            else
            {
                //从时间轮的二维数组中删除掉,然后清理一下连接资源
                circlue_queue_delete(pUsr, pCir);
                close(pUsr->fd_sock);
                pUsr->fd_sock = 0;
                pUsr->login_stat = 0;
                printf("failed\n");
                return -1;
            }
        }
    }
}
int register_handle(pUsr_t pUsr, MYSQL *conn, char *reg_passwd, char *usr, pTimeout_disconnet_t pCir)
{
    printf("in register handle,fd =%d to register\n", pUsr->fd_sock);
    char salt[16] = {0};
    char str[9] = {0};
    char crypt_passwd[128] = {0};
    char token[64] = {'t', 'o', 'k', 'e', 'n'};
    char pwd[255] = {0};
    int cur_file_code = 0;
    char passwd[32] = {0};
    char usr_name[16] = {0};
    char query[256] = {0};
    strncpy(passwd, reg_passwd, sizeof(passwd) - 1);
    strncpy(usr_name, usr, sizeof(usr_name) - 1);
    generate_str(str, 9);        //生成8位的字符串数组
    sprintf(salt, "$6$%s", str); //使用sha512,合成盐值
    strcpy(crypt_passwd, crypt(passwd, salt));
    strcpy(pwd, "/");
    sprintf(query, "insert into usr_info (usr_name,salt,crypt_passwd,token,pwd,cur_dir_code)values ('%s','%s','%s','%s','%s',%d)",
            usr_name, salt, crypt_passwd, token, pwd, cur_file_code);
    //printf("query : %s", query);
    int query_ret = mysql_query(conn, query); 
    // 对于插入类语句,mysql_query的返回结果可以直接代表是否插入成功
    //(但是查询类语句的返回值只代表查询语句是否正确)
    if (0 == query_ret)
    {
        printf("register succeed\n");

        pUsr->login_stat = 1; //将登陆标志位设置为1 将用户名插入online,更新token
        strcpy(pUsr->usr_name, usr_name);
        generate_token(pUsr->usr_name, token);
        printf("token=%s\n", token);
        sprintf(query, "update usr_info set token = '%s' where usr_name = '%s'", token, pUsr->usr_name);
        int ret = mysql_query(conn, query);
        printf("ret = %d query=%s\n", ret, query);
        ERROR_CHECK(ret, 1, "mysql_query");
        send(pUsr->fd_sock, token, sizeof(token), 0);
        printf("succeed\n");
        return 0;
    }
    else
    { //注册失败
        printf("insert failed,cut the conection\n");
        circlue_queue_delete(pUsr, pCir);
        close(pUsr->fd_sock); //切断tcp连接,将套接字描述符置为0(防止误判)
        pUsr->fd_sock = 0;
        pUsr->login_stat = 0; //将当前登陆标志位置为0
        printf("failed\n");
        return -1;
    }
}
void generate_str(char *str, int len)
{
    str[len - 1] = 0;
    int i, flag;
    srand(time(NULL)); //通过时间函数设置随机数种子，使得每次运行结果随机。  
    for (i = 0; i < len - 1; i++)
    {
        flag = rand() % 3;
        switch (flag)
        {
        case 0:
            str[i] = rand() % 26 + 'a';
            break;
        case 1:
            str[i] = rand() % 26 + 'A';
            break;
        case 2:
            str[i] = rand() % 10 + '0';
            break;
        }
    }
}

int short_order_handle(pUsr_t pUsr, MYSQL *net_disk_mysql, pTimeout_disconnet_t pCir) //短命令比较短,直接在一个函数中完成
{
    //由于在进入这个函数之前已经判断了登陆状态,所以这里不用在判断一次
    //如果有断开连接的,一定要尽快切断连接,将fd置为0 并且将online_usr中的用户状态置为0
    printf("in short order handle fd = %d usr_name = %s\n", pUsr->fd_sock, pUsr->usr_name);
    MYSQL_RES *res;
    MYSQL_ROW row;
    char query[128] = {0};
    int cur_dir_code = 0;
    int precode = 0;
    Short_order_t short_order;
    File_info_t file_info;
    int ret = 0;
    bzero(&short_order, sizeof(short_order));
    ret = recv(pUsr->fd_sock, &short_order, sizeof(short_order), MSG_WAITALL);
    if (0 == ret)
    {
        // recv 到0 说明对方切断tcp连接
        printf("fd = %d user \'%s\' exit\n", pUsr->fd_sock, pUsr->usr_name);
        circlue_queue_delete(pUsr, pCir);
        close(pUsr->fd_sock);
        pUsr->fd_sock = 0;
        pUsr->login_stat = 0;
        bzero(pUsr->usr_name, 32);
        return -1;
    }
    if (0 == strcmp(short_order.order, "pwd"))
    {
        sprintf(query, "select pwd from usr_info where usr_name ='%s'", pUsr->usr_name);
        //printf("query = %s\n", query);
        ret = mysql_query(net_disk_mysql, query);
        res = mysql_store_result(net_disk_mysql); //pwd一定存在,所以不检查 res/row 是否为空
        row = mysql_fetch_row(res);
        bzero(&file_info, sizeof(file_info));
        strcpy(file_info.file_path, row[0]);
        send(pUsr->fd_sock, &file_info, sizeof(file_info), 0);
        printf("pwd = %s\n", file_info.file_path);
        return 0;
    }
    else if (0 == strcmp(short_order.order, "ls"))
    {
        printf("in ls handle\n");
        sprintf(query, "select cur_dir_code from usr_info where usr_name ='%s'", pUsr->usr_name);
        //printf("ls query = %s\n", query);
        ret = mysql_query(net_disk_mysql, query);
        // mysql_query的返回值,主要是判断查询语句是不是合法的,但是不判断查询是否有结果的
        // 判断 res/row是不是null才可以判断是否有结果的

        res = mysql_store_result(net_disk_mysql);
        row = mysql_fetch_row(res);
        cur_dir_code = atoi(row[0]);
        //printf("cur_dir_code = %d\n", atoi(row[0]));
        bzero(query, sizeof(query));
        sprintf(query, "select file_name,file_size,file_type from file_table where precode =%d and owner ='%s'", cur_dir_code, pUsr->usr_name);
        //printf("ls file query = %s\n", query);
        ret = mysql_query(net_disk_mysql, query);
        res = mysql_store_result(net_disk_mysql);
        row = mysql_fetch_row(res);
        if (row == NULL)
        { //当前目录之下没有文件
            bzero(&file_info, sizeof(file_info));
            file_info.file_size = -1;
            send(pUsr->fd_sock, &file_info, sizeof(file_info), 0);
            printf("no file in this dir\n");
            return 0;
        }
        else
        {
            do
            {
                bzero(&file_info, sizeof(file_info));
                strncpy(file_info.file_name, row[0], sizeof(file_info.file_name));
                file_info.file_size = atoi(row[1]);
                strncpy(file_info.file_type, row[2], sizeof(file_info.file_type));
                send(pUsr->fd_sock, &file_info, sizeof(file_info), 0);
                printf("send info:%s %ld %s\n", file_info.file_name, file_info.file_size, file_info.file_type);
            } while ((row = mysql_fetch_row(res)));
            bzero(&file_info, sizeof(file_info));
            file_info.file_size = -2; //文件信息传输完毕,发一个结束标志位
            send(pUsr->fd_sock, &file_info, sizeof(file_info), 0);
            return 0;
        }
    }
    else if (0 == strcmp(short_order.order, "cd"))
    {
        printf("in cd handle\n");
        char args[128] = {0};
        char pwd[256] = {0};
        bzero(&file_info, sizeof(file_info));
        strncpy(args, short_order.args, sizeof(args));
        if (0 == strcmp("..", args))
        { //向上一级cd,先判断是不是根目录,如果是 则返回错误信息,不是则 设置pwd,cur_dir_code,返回正确标识
            sprintf(query, "select cur_dir_code ,pwd from usr_info where usr_name = '%s'", pUsr->usr_name);
            //printf("query =%s\n", query);
            ret = mysql_query(net_disk_mysql, query); //此处一定能查找到,所以不对返回值进行检查
            res = mysql_store_result(net_disk_mysql);
            row = mysql_fetch_row(res);
            cur_dir_code = atoi(row[0]);
            strncpy(pwd, row[1], sizeof(pwd));
            printf(" cur_dir_code = %d pwd = %s\n", cur_dir_code, pwd);
            if (0 == cur_dir_code)
            {
                printf("wrong root\n");
                file_info.file_size = -3; //根目录标志位
                send(pUsr->fd_sock, &file_info, sizeof(file_info), 0);
                return -1;
            }
            else
            {
                sprintf(query, "select precode from file_table where file_code = %d", cur_dir_code);
                ret = mysql_query(net_disk_mysql, query);
                //printf("ret = %d\n", ret);
                res = mysql_store_result(net_disk_mysql);
                row = mysql_fetch_row(res);
                precode = atoi(row[0]);
                //printf("precode = %d\n", precode);
                sprintf(query, "update usr_info set cur_dir_code = %d where usr_name = '%s'", precode, pUsr->usr_name);
                //printf("query =%s\n", query);
                ret = mysql_query(net_disk_mysql, query);
                int str_len = strlen(pwd);

                pwd[str_len - 1] = 0; //将最后的一个斜杠删除,然后删除直到前一个斜杠这之间的所有内容
                for (int i = str_len - 2; i >= 0; i--)
                {
                    if ('/' == pwd[i])
                    {
                        break;
                    }
                    else
                    {
                        pwd[i] = 0;
                    }
                }
                sprintf(query, "update usr_info set pwd = '%s' where usr_name = '%s'", pwd, pUsr->usr_name);
                ret = mysql_query(net_disk_mysql, query);
                //printf("query ret = %d\n", ret);
                file_info.file_size = 0;
                send(pUsr->fd_sock, &file_info, sizeof(file_info), 0);
            }
        }
        else //向下 cd
        {
            sprintf(query, "select cur_dir_code ,pwd from usr_info where usr_name = '%s'", pUsr->usr_name);
            //printf("cd query cur_fir_code and pwd query =%s\n", query);
            ret = mysql_query(net_disk_mysql, query); //此处一定能查找到,所以不对返回值进行检查
            //printf("ret = %d\n", ret);
            res = mysql_store_result(net_disk_mysql);
            row = mysql_fetch_row(res);
            cur_dir_code = atoi(row[0]);
            strncpy(pwd, row[1], sizeof(pwd));
            //printf("cd to next dir cur_dir_code = %d pwd = %s\n", cur_dir_code, pwd);

            sprintf(query, "select file_type,file_code from file_table where owner ='%s' and precode = %d and file_name ='%s'", pUsr->usr_name, cur_dir_code, args);
            //文件类型用来判断是否能够cd进去,只有文件夹才能够cd进去,file_code用来设定cur_dir_code
            //printf("query = %s\n", query);
            ret = mysql_query(net_disk_mysql, query);
            // printf("query ret = %d\n", ret);
            res = mysql_store_result(net_disk_mysql);
            row = mysql_fetch_row(res); //此处最多有一个符合条件的文件
            if (NULL == row)
            { //没有这个文件,返回错误路径标志位
                file_info.file_size = -4;//不存在这个文件
                send(pUsr->fd_sock, &file_info, sizeof(file_info), 0);
                return -1;
            }
            else
            {
                char file_type[2] = {0}; //文件类型
                int file_code = 0;
                strncpy(file_type, row[0], sizeof(file_type));
                file_code = atoi(row[1]);
                printf("file_type = %s file_code =%d\n", file_type, file_code);
                if (0 == strcmp(file_type, "d"))
                { //cd的目标是目录,那么就直接删除

                    sprintf(query, "update usr_info set cur_dir_code = %d where usr_name = '%s'", file_code, pUsr->usr_name);
                    //printf("query =%s\n", query);
                    ret = mysql_query(net_disk_mysql, query);
                    int str_len = strlen(pwd);

                    strcat(pwd, args);
                    strcat(pwd, "/"); //添加下一级目录的名字以及斜杠
                    sprintf(query, "update usr_info set pwd = '%s' where usr_name = '%s'", pwd, pUsr->usr_name);
                    //printf("query =%s\n", query);
                    ret = mysql_query(net_disk_mysql, query);
                    printf("query ret = %d\n", ret);
                    file_info.file_size = 0;
                    send(pUsr->fd_sock, &file_info, sizeof(file_info), 0);
                }
                else
                { //文件类型不对
                    file_info.file_size = -5;//不是文件夹
                    send(pUsr->fd_sock, &file_info, sizeof(file_info), 0);
                    return -1;
                }
            }
        }
    }
}

int generate_token(char *usr_name, char *token)
{
    char new_token[63] = {0};
    char str[10] = {0};
    char time_0[25] = {0};
    time_t time_now;
    time(&time_now);
    strcpy(time_0, ctime(&time_now));
    generate_str(str, 10);
    sprintf(token, "%s%s%ld", usr_name, str, time_now);//用户名,随机字符串,时间
}
int timeout_circule_queue_init(pTimeout_disconnet_t pCir)
{
    bzero(pCir, sizeof(Timeout_disconnect_t)); //清空
    pCir->que_len = 30;                        //10s 超时时间
    pCir->que_node_size = 30;                  //每一个节点能够存放的fd数目
    pCir->cur = 1;
    pCir->pre = 0;
    printf("超时时间 %d 每个槽的存放数量 %d \n", pCir->que_len, pCir->que_node_size);
    pCir->fd_timeout = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
    ERROR_CHECK(pCir->fd_timeout, -1, "timer_settime");
    struct itimerspec new_value;
    uint64_t exp;
    new_value.it_value.tv_sec = 2;
    new_value.it_value.tv_nsec = 0;
    new_value.it_interval.tv_sec = 1;
    new_value.it_interval.tv_nsec = 0;
    int ret = timerfd_settime(pCir->fd_timeout, 0, &new_value, NULL);
    //printf("setttime ret = =%d\n", ret);
}
int circule_queue_add(pUsr_t pUsr, pTimeout_disconnet_t pCir)
{
    //在pre槽位中查找一个空的地方,然后将pUsr中的fd放进去
    int pre = pCir->pre;
    int i;
    for (i = 0; i != pCir->que_node_size; i++)
    {
        if (0 == pCir->pCir_que[pre][i])
        {
            pCir->pCir_que[pre][i] = pUsr->fd_sock;
            pUsr->in_which_slot = pre; //记录槽位
            printf("new fd %d add to slot %d debug i =%d\n", pUsr->fd_sock, pre, i);
            break;
        }
    }
    if (i == pCir->que_node_size)
    {
        printf("errorr one slot is full\n");
    }
}
int circule_queue_move(pUsr_t pUsr, pTimeout_disconnet_t pCir)
{
    //printf("usr %s %d slot %d\n", pUsr->usr_name, pUsr->fd_sock, pUsr->in_which_slot);
    int pre = pCir->pre;
    printf("pre = %d\n", pre);
    int i;
    int slot = pUsr->in_which_slot;
    for (i = 0; i != pCir->que_node_size; i++) //将原来的槽位的fd置0
    {
        printf("a %d-", pCir->pCir_que[slot][i]);
        fflush(stdout);
        if (pCir->pCir_que[slot][i] == pUsr->fd_sock)
        {
            pCir->pCir_que[slot][i] = 0;
            break;
        }
    }
    if (i == pCir->que_node_size)
    {
        printf("serious error in move\n");
    }
    i = 0;
    for (i = 0; i != pCir->que_node_size; ++i) ///移动到新的槽位
    {
        printf("b %d-", pCir->pCir_que[pre][i]);
        fflush(stdout);
        if (pCir->pCir_que[pre][i] == 0)
        {
            pCir->pCir_que[pre][i] = pUsr->fd_sock;
            //移动到新的槽位之后一定要更新pUsr中的槽位的值
            pUsr->in_which_slot = pre;
            break;
        }
    }
    printf("move usr %s from %d to %d\n", pUsr->usr_name, slot, pre);
    if (i == pCir->que_node_size)
    {
        printf("serious error full in move\n");
    }
}
int circule_queue_update(pUsr_t online_usr, int max_online, pTimeout_disconnet_t pCir)
{
    //查找当前槽中的fd,清理对应的tcp连接,清空对应的pUsr,然后将这个cur集合全部置为0
    uint64_t tem;
    read(pCir->fd_timeout, &tem, sizeof(uint64_t));
    int cur = pCir->cur;
    int size = pCir->que_node_size;
    printf("in cir uqe update cur = %d pre =%d  next pre = %d\n", cur, pCir->pre, (1 + pCir->pre) % (pCir->que_len));
    int clean_cnt = 0;
    for (int i = 0; i != size; i++)
    {
        if (0 != (pCir->pCir_que)[cur][i]) //找到这个槽里面的所有的fd(不为0的就是连接的fd)
        {
            for (int j = 0; j != max_online; ++j)
            {
                //printf("j = %d\n", j);
                //然后找到这个槽中的fd对应的,在线用户的结构体user
                if ((pCir->pCir_que)[cur][i] == (online_usr[j]).fd_sock) //找到了,清理对应的tcp连接
                {
                    clean_cnt++;
                    printf("disconnect with usr %s fd = %d\n", (online_usr[j]).usr_name, (online_usr[j]).fd_sock);
                    close((online_usr[j]).fd_sock);
                    bzero(&online_usr[j], sizeof(Usr_t));
                    break;
                }
            }
        }
    }
    //usr_t 清空完毕,tcp清空完毕 开始清理循环队列的cur位置
    bzero(pCir->pCir_que[cur], sizeof(int) * size);
    //清理完毕,更新cur,pre的值
    pCir->pre = pCir->cur;
    pCir->cur = (1 + pCir->cur) % (pCir->que_len);
    if (clean_cnt > 0)
    {
        printf("cir qur update done %d has been cleaned\n", clean_cnt);
    }
}
int circlue_queue_delete(pUsr_t pUsr, pTimeout_disconnet_t pCir)
{
    int pre = pCir->pre;
    int slot = pUsr->in_which_slot;
    printf("in circule delete usr %s %d slot %d quit\n", pUsr->usr_name, pUsr->fd_sock, pUsr->in_which_slot);
    for (int i = 0; i != pCir->que_node_size; i++) //将原来的槽位的fd置0
    {
        if (pCir->pCir_que[slot][i] == pUsr->fd_sock)
        {
            pCir->pCir_que[slot][i] = 0;
            return 0;
        }
    }
    printf("serious error in circule delete\n");
    return -1;
}