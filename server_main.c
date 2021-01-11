#include "net_work_disk.h"
// 编译的时候要加上-lpthread ： gcc -o server server.c server_func.c -lpthread
int main(int argc, char **argv)
{
    Timeout_disconnect_t cir_que;
    timeout_circule_queue_init(&cir_que); //初始化超时环形队列,timerfd,堆空间上申请空间等,置位
    printf("fd_outtime = %d\n", cir_que.fd_timeout);
    chdir("server_root");
    MYSQL *net_disk_mysql;
    int ret = database_init(&net_disk_mysql); //初始化数据库连接 里面有三个表
    ERROR_CHECK(ret, -1, "mysql failed to connect");
    ThreadPool_t pool;
    thread_pool_init(&pool);  //线程池初始化，申请空间，清理空间
    thread_pool_start(&pool); //启动线程池，创建子线程，启动标志位置为1
    int fd_listen = 0;
    tcp_init(&fd_listen); //初始化tcp连接
    printf("fd_listen = %d\n", fd_listen);
    //设置好epoll
    //开始执行任务，接收到客户端的请求，就将任务封装传递到任务队列的队尾（只监听fd_listen）
    //并且激活条件变量（pthread_cond_signal一个一个的激活），激活下一个在条件变量上排队的线程
    int epfd = 0;
    epoll_init(&epfd, fd_listen, cir_que.fd_timeout);
    printf("fd_epfd = %d\n", epfd);
    int readyNum = 0;
    struct epoll_event evs[20];
    bzero(evs, sizeof(evs));
    int newFd = 0;
    //使用一个在线用户结构体数组来管理所有连接
    int max_online = LISTEN_CNT;
    pUsr_t online_usr = (pUsr_t)calloc(max_online, sizeof(Usr_t));
    //使用一个Login_t类型的结构体来处理新的连接请求 连接,大文件传输
    Login_t new_login; //每次新的登陆必须收到一个Login_t类型的结构体
    while (1)
    {
        readyNum = epoll_wait(epfd, evs, 20, -1);
#ifdef DEBUG
        printf("epoll_wait,readyNum = %d\n", readyNum);
#endif
        // for (int i = 0; i < readyNum; i++)
        // {
        //     printf(" test fd = %d\n", evs[i].data.fd);
        // }
        for (int i = 0; i != readyNum; ++i)
        {
            if (fd_listen == evs[i].data.fd) //监听套接字仅仅用来连接tcp,登陆以及长短命令是在其他函数中处理的
            {
                printf("new fd\n");
                newFd = accept(fd_listen, NULL, NULL);
                //有新的tcp连接,在online_usr中查找一个空闲(标志位为0),将fd和stat =-2写入登陆状态标志位,并且将描述符加入监听集合
                bzero(&new_login, sizeof(new_login));
                int j = 0;
                for (j = 0; j < max_online; j++)
                {
                    if (0 == online_usr[j].login_stat) // 找出一个空位啊,登录状态不会为1的
                    {
                        online_usr[j].fd_sock = newFd;
                        online_usr[j].login_stat = -2; //还没有验证密码呢,所以 -2
                        add_fd_to_epfd(epfd, newFd, EPOLLIN);
                        //加入超时环形队列
                        circule_queue_add(&online_usr[j], &cir_que); 
                        //注意哈,在这个函数之中,要将目前的槽的位置添加到 在线用户数组中的位置上去
                        break;
                    }
                }
                if (max_online == j)
                { //如果已经达到最大连接数,则不在接受新的客户端连接
                    close(newFd);
                }
            }
            if (cir_que.fd_timeout == evs[i].data.fd)
            { //如果超时
                //printf("fd_timeout active\n");
                circule_queue_update(online_usr, max_online, &cir_que); //清理cur指向的集合,关闭fd,pUsr置为0
            }
            // 下标 I 是返回的 事件数组 的下标
            for (int k = 0; k < 20; ++k)
            {
                if (online_usr[k].fd_sock == evs[i].data.fd)
                { //有新的指令前来,依据这个连接套接字
                    //找到用户,依据用户的登陆状态判断是调用login_handle还是short_order_handle
                    printf("fd = %d\n", online_usr[k].fd_sock);
                    //移动描述符 到pre位置
                    circule_queue_move(&online_usr[k], &cir_que); //移动到新的位置上去
                    if (1 == online_usr[k].login_stat)
                    
                    {//使用完全建立连接的tcp连接
                        short_order_handle(&online_usr[k], net_disk_mysql, &cir_que); //用户状态结构体指针以及数据库结构体
                    }
                    else //注册 ,登录/传输文件都是新连接 标志位 为-2,进入logn_order处理,登陆 标志位为-2/-1
                    {
                        long_order_handle(&online_usr[k], net_disk_mysql, &pool, epfd, &cir_que); //用户状态结构体指针以及数据库结构体
                    }
                }
            }
        }
    }
    return 0;
}