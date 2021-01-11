#pragma once
#include <myhead.h>
#define DEBUG
#define IP "172.24.237.106"
#define PORT "5555"
#define THREAD_CNT 5
#define NOT_START 0
#define STARTED 1
#define LISTEN_CNT 15
#define FILE "back"
//长命令结构体:登陆,注册,长命令请求
typedef struct
{
    char order[9]; //login,register,gets,puts
    char usr[16];
    char crypt_passwd[128];
    char md5_code[33];
    char file_name[128];
    off_t file_size;
    off_t file_off;
} Login_t, *pLogin_t;
//短命令结构体:ls cd pwd rm mkdir ,短命令是在已经建立的tcp连接上进行的
typedef struct
{
    char order[9];
    char args[128];
} Short_order_t, *pShort_order_t;
//文件信息结构体
typedef struct
{
    char file_name[128];
    char file_path[256];
    off_t file_size;
    char file_type[2];
} File_info_t, *pFile_info_t;
//任务结构体
typedef struct Node
{
    int cur_dir_code;
    char usr_name[16];
    char file_name[128];
    int fd_client;
    char md5_code[33];
    off_t file_size;
    off_t file_off;
    char order[10];
    MYSQL *task_mysql;
    struct Node *pNext;
} Task_t, *pTask_t;
//队列信息以及访问队列的保护措施:头尾指针，队列大小，锁和条件变量
typedef struct
{
    pTask_t pHead;
    pTask_t pTail;
    int queSize;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Que_t, *pQue_t;
//线程池信息
typedef struct
{
    Que_t que;
    int pthreadCnt;
    char startFlag;
    pthread_t *pThid;
} ThreadPool_t, *pThreadPool_t;
//小火车
typedef struct
{
    off_t trainLen;
    char buf[1000];
} Train_t;
//在线用户结构体
typedef struct
{
    int fd_sock;
    char login_stat; //登录状态,
    int in_which_slot;
    char usr_name[33];
    //标记这个描述符在环形队列的哪一个队列节点指向的数组里面,这个描述符新的相应的时候,移动到pPre
} Usr_t, *pUsr_t;
//环形队列结构体
typedef struct
{
    int pCir_que[30][30];
    int que_len;
    int que_node_size;
    int cur; //清理位置
    int pre; //重置位置
    int fd_timeout;
} Timeout_disconnect_t, *pTimeout_disconnet_t;
//多点下载 资源服务器回复资源消息的结构体
typedef struct
{
    char flag;
    off_t file_size;
    off_t file_off_1;
    off_t file_len_1;
    char ip_1[32];
    char port_1[8];
    off_t file_off_2;
    off_t file_len_2;
    char ip_2[32];
    char port_2[8];
} Multi_point_t, *pMulti_point_t;

int timeout_circule_queue_init(pTimeout_disconnet_t pCir_que);
int circule_queue_move(pUsr_t pUsr, pTimeout_disconnet_t pCir_que);
int circule_queue_update(pUsr_t pUsr, int max_online, pTimeout_disconnet_t pCir_que);
int circule_queue_add(pUsr_t pUsr, pTimeout_disconnet_t pCir_que);
int circlue_queue_delete(pUsr_t pUsr, pTimeout_disconnet_t pCir_que);

int thread_pool_init(pThreadPool_t pPool);
int thread_pool_start(pThreadPool_t pPool);
void *thread_func(void *p);
void *client_thread_func(void *p);
int que_insert(pQue_t pQue, pTask_t pNode);
int que_get(pQue_t pQue, pTask_t pNode);
int tcp_init(int *pFd_listen);
int epoll_init(int *pFd, int fd_listen, int fd_timeout);
int add_fd_to_epfd(int epfd, int fd, int operator);
int database_init(MYSQL **ppConn);
int long_order_handle(pUsr_t pUsr, MYSQL *conn, pThreadPool_t pPool, int epfd, pTimeout_disconnet_t pCir);
int register_handle(pUsr_t pUsr, MYSQL *conn, char *, char *, pTimeout_disconnet_t pCir);
void generate_str(char *str, int len);
int login_handle(pUsr_t pUsr, MYSQL *conn, pLogin_t pNewLogin, pTimeout_disconnet_t pCir);
int short_order_handle(pUsr_t pUsr, MYSQL *net_disk_mysql, pTimeout_disconnet_t pCir);
int generate_token(char *usr_name, char *token);
int puts_handle(pUsr_t pUsr, MYSQL *net_diks_mysql, pLogin_t pNewLogin, pThreadPool_t pPool, int epfd, pTimeout_disconnet_t pCir);
int Compute_file_md5(const char *file_path, char *md5_str);
off_t send_file_by_mmap(int fd_to, off_t file_size, off_t file_off, int fd_file);
int recv_file_from_client(Task_t task);
int recv_file_by_train(char *file_path, off_t file_off, off_t file_size, char *log_file_path, int fd_recv_from);
int recvCycle(int fd_recv, void *buf, off_t requestLen);
int recv_file_by_splice(char *file_path, off_t file_off, off_t file_size, char *log_file_path, int fd_recv_from);
void *client_thread_func_gets(void *p);
int gets_handle(pUsr_t pUsr, MYSQL *net_disk_mysql, pLogin_t pNewLogin, pThreadPool_t pPool, int epfd, pTimeout_disconnet_t pCir);
off_t send_file_by_mmap(int fd_client, off_t file_size, off_t file_off, int fd_file);
int send_file_by_train(int fd_client, off_t file_size, off_t file_off, int fd_file);