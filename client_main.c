#include "net_work_disk.h"
//#include "md5.h"
int main()
{
  chdir("personal_folder");
  //登陆之前
  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(IP);
  server_addr.sin_port = htons(atoi(PORT));
  Login_t my_login;
  char token[64] = {0};
  int fd_server = -1;
  char salt[16] = {0};
  char passwd[32] = {0};
  char order[10] = {0};
  char usr[16] = {0};
  int ret = 0;
  //开始登陆
  printf("login usr(15char max)/register usr \n");
  
  while (1)
  {
    printf("----------------------------------------------------------\n");
    printf("net_disk-> ");
    scanf("%s%s", order, usr);
    //printf("\n");
    strncpy(passwd, getpass("enter your key:"), sizeof(passwd) - 1);
    printf("debug : %s %s %s\n", order, usr, passwd);
    //连接tcp
    fd_server = socket(AF_INET, SOCK_STREAM, 0);
    int ret = connect(fd_server, (struct sockaddr *)&server_addr, sizeof(server_addr));
    ERROR_CHECK(ret, -1, "connect to server");
    //登陆 客户端发送用户名,接收盐值,生成crypt密文,发给服务器,接收服务器token值并且判断是否登陆成功
    if (0 == strcmp("login", order))
    {

      bzero(&my_login, sizeof(my_login));
      strncpy(my_login.order, order, sizeof(my_login.order) - 1);
      strncpy(my_login.usr, usr, sizeof(my_login.usr) - 1);
      ret = send(fd_server, &my_login, sizeof(my_login), 0);
      ERROR_CHECK(ret, -1, "send1");
      ret = recv(fd_server, &salt, sizeof(salt), MSG_WAITALL);
      ERROR_CHECK(ret, -1, "recv1");
      strcpy(my_login.crypt_passwd, crypt(passwd, salt));
      printf("crypt_passwd = %s\n", crypt(passwd, salt));
      ret = send(fd_server, &my_login, sizeof(my_login), 0);
      ERROR_CHECK(ret, -1, "send2");
      ret = recv(fd_server, token, sizeof(token), MSG_WAITALL);
      printf("token = %s\n", token);
      if (0 != token[0])
      {
        break;
      }
      else
      {
        printf("登陆失败\n");
      }
      close(fd_server);
    } //注册 发送用户名,密码,接收token值,收到正确token值表示注册成功并且登陆,token不正确显示注册失败
    else if (0 == strcmp("register", order))
    {
      bzero(&my_login, sizeof(my_login));
      strncpy(my_login.order, order, sizeof(my_login.order) - 1);
      strncpy(my_login.usr, usr, sizeof(my_login.usr) - 1);
      strncpy(my_login.crypt_passwd, passwd, sizeof(passwd) - 1);
      ret = send(fd_server, &my_login, sizeof(my_login), 0);
      ERROR_CHECK(ret, -1, "send1");
      ret = recv(fd_server, token, sizeof(token), MSG_WAITALL);
      printf("token = %s\n", token);
      ERROR_CHECK(ret, -1, "recv1");
      if (0 != token[0])
      {
        printf("succeed to register\n");
        break;
      }
      else
      {
        printf("注册失败\n");
      }
      close(fd_server);
    } //输错命令了
    else
    {
      printf("wrong order,try again!\n");
      close(fd_server);
    }
  }
  printf("login success! cd /gets /puts /ls /pwd ------\n");
  char my_order[9];
  char my_args[128];
  Short_order_t short_order;
  File_info_t file_info;
  int file_cnt = 0;

  while (1)
  {
    bzero(my_order, sizeof(my_order));
    bzero(my_args, sizeof(my_args));
    bzero(&short_order, sizeof(short));
    //刷新输入缓冲区
    // while ((ch = getchar()) != '\n' && ch != EOF)
    // {
    //   ;
    // }
    printf("----------------------------------------------------------\n");
    printf("net_disk-> ");
    scanf("%s", my_order);
    my_order[8] = 0;
    //处理指令字符串
    //判断指令,接收信息 目前的 ls,cd,pwd都是接收一样的结构体 可以放在一起写
    if (0 == strcmp(my_order, "exit"))
    {
      break;
    }
    else if (0 == strcmp(my_order, "ls"))
    {
      strcpy(short_order.order, my_order);
      ret = send(fd_server, &short_order, sizeof(short_order), 0);
      ERROR_CHECK(ret, -1, "ls");
      printf("ls failed \n");
      while (1)
      {
        bzero(&file_info, sizeof(file_info));
        ret = recv(fd_server, &file_info, sizeof(file_info), MSG_WAITALL);
        if(0 == ret){
          printf("connection ends\n");
          break;
        }
        if (-1 == file_info.file_size)
        {
          printf("no file in this dir \n");
          break;
        }
        else if (-2 == file_info.file_size)
        {
          printf("those are all files\n");
          break;
        }
        printf("%30s  %12ld %2s\n", file_info.file_name, file_info.file_size, file_info.file_type);
      }
    }
    else if (0 == strcmp(my_order, "cd"))
    {
      scanf("%s", my_args);
      my_args[127] = 0;
      strcpy(short_order.order, my_order);
      strcpy(short_order.args, my_args);
      ret = send(fd_server, &short_order, sizeof(short_order), 0);
      ERROR_CHECK(ret, -1, "send");
      printf("send ret = %d\n",ret);
      while (1)
      {
        bzero(&file_info, sizeof(file_info));
        file_info.file_size = -1;
        recv(fd_server, &file_info, sizeof(file_info), MSG_WAITALL);
        if (0 == file_info.file_size)
        {
          printf("cd succeed\n");
          break;
        }
        else if (-3 == file_info.file_size)
        {
          printf("cd failed! you are already at root\n");
          break;
        }
        else if (-4 == file_info.file_size)
        {
          printf("cd failed! wrong dir name\n");
          break;
        }
        else if (-5 == file_info.file_size)
        {
          printf("cd failed!it is a file,not a dir\n");
          break;
        }
        else
        {
          break;
        }
        printf("%30s  %12ld %2s\n", file_info.file_name, file_info.file_size, file_info.file_type);
      }
    }
    else if (0 == strcmp(my_order, "pwd"))
    {
      strcpy(short_order.order, my_order);
      ret = send(fd_server, &short_order, sizeof(short_order), 0);
      ERROR_CHECK(ret, -1, "send");
      bzero(&file_info, sizeof(file_info));
      int ret = recv(fd_server, &file_info, sizeof(file_info), MSG_WAITALL);
      printf("pwd =%s\n", file_info.file_path);
    }
    else if (0 == strcmp(my_order, "puts"))
    {
      scanf("%s", my_args);
      int fd_file = open(my_args, O_RDWR);
      if (-1 != fd_file)
      { //如果文件存在,那么将认证信息以及文件名传输给子线程处理
        close(fd_file);
        Login_t puts_request;
        bzero(&puts_request, sizeof(puts_request));
        strncpy(puts_request.order, my_order, sizeof(puts_request.order));
        strncpy(puts_request.usr, usr, sizeof(puts_request.usr));
        strncpy(puts_request.file_name, my_args, sizeof(puts_request.file_name));
        strncpy(puts_request.crypt_passwd, token, sizeof(puts_request.crypt_passwd));
        pthread_t thid;
        pthread_create(&thid, NULL, client_thread_func, &puts_request);
      }
      else
      {
        printf("wrong file_name\n");
      }
    }
    else if (0 == strcmp(my_order, "gets"))
    {
      scanf("%s", my_args); 
      Login_t gets_request;
      bzero(&gets_request, sizeof(gets_request));
      strncpy(gets_request.order, my_order, sizeof(gets_request.order));
      strncpy(gets_request.usr, usr, sizeof(gets_request.usr));
      strncpy(gets_request.file_name, my_args, sizeof(gets_request.file_name));
      strncpy(gets_request.crypt_passwd, token, sizeof(gets_request.crypt_passwd));
      pthread_t thid;
      pthread_create(&thid, NULL, client_thread_func_gets, &gets_request);
    }
    else if (0 == strcmp(my_order, "mgets"))
    {
      scanf("%s", my_args);
      //5 .1 无需断点续传的多机下载
      //首先向src_server发送请求,server如果找到这个文件之后,则发送
      // 文件大小,从server上下载的偏移值,下载量,从server_1上下载的偏移值,下载量
      //客户端在下载的时候,需要不停的偏移文件中的ptr,并且对两个文件的偏移值,下载量 进行分别计数
      //当两部分文件都下载完成的时候,打印一个下载完成
      Login_t gets_request;
      Multi_point_t multi_info;
      bzero(&gets_request, sizeof(gets_request));
      bzero(&multi_info, sizeof(multi_info));
      strncpy(gets_request.order, my_order, sizeof(gets_request.order));
      strncpy(gets_request.usr, usr, sizeof(gets_request.usr));
      strncpy(gets_request.file_name, my_args, sizeof(gets_request.file_name));
      strncpy(gets_request.crypt_passwd, token, sizeof(gets_request.crypt_passwd));
      struct sockaddr_in src_addr;
      bzero(&src_addr, sizeof(src_addr));
      src_addr.sin_family = AF_INET;
      src_addr.sin_addr.s_addr = inet_addr(IP);
      src_addr.sin_port = htons(atoi("4000")); //资源管理服务器端口是4000
      int fd_src = socket(AF_INET, SOCK_STREAM, 0);
      int ret = connect(fd_src, (struct sockaddr *)&src_addr, sizeof(src_addr));
      ERROR_CHECK(-1, ret, "connect");
      ret = send(fd_src, &gets_request, sizeof(gets_request), 0);
      ERROR_CHECK(ret, -1, "send");
      recv(fd_src, &multi_info, sizeof(multi_info), MSG_WAITALL);
      if (-1 == multi_info.flag)
      {
        printf("no such file ,try again in other dirs\n");
      }
      else
      {
        printf("recv src info file_size = %ld file_off_1 %ld file_len_1 %ld  file_off_2 %ld file_len_2 %ld",
               multi_info.file_size, multi_info.file_off_1, multi_info.file_len_1, multi_info.file_off_2, multi_info.file_len_2);
      }
    }
  }
  close(fd_server);
  printf("exit\n");
  return 0;
}

void *client_thread_func_gets(void *p)
{
  printf("in client_thread_func_gets\n ");
  Login_t gets_request = *((Login_t *)p);
  char log_file_path[150] = {0};
  sprintf(log_file_path, ".logfile_%s", gets_request.file_name);
  off_t file_size = 0, file_off = 0;
  struct stat my_stat;
  bzero(&my_stat, sizeof(my_stat));
  //先判断本地有没有这个文件,如果 存在 则获取文件大小,偏移值大小,获取之后开始
  int fd_file = open(gets_request.file_name, O_RDWR);
  int fd_log_file = open(log_file_path, O_RDWR);
  if (-1 == fd_file)
  {
    file_size = 0;
    file_off = 0;
  }
  else if (-1 == fd_log_file)
  {
    // 有文件,但是没有日志,说明文件是已经传输完成的了,不用再传输了
    printf("file already exist\n");
    close(fd_file);
    pthread_exit(0);
  }
  else
  { // 都存在
    fstat(fd_file, &my_stat);
    file_size = my_stat.st_size;
    read(fd_log_file, &file_off, sizeof(file_off)); //只要文件存在,并且日志存在,那么日志中一定有内容
    off_t tem_0 = file_off % 4096;// 注意必须是  4096的整数倍的偏移量
    file_off -= tem_0;
    close(fd_file);
    close(fd_log_file);
  }
  gets_request.file_size = file_size; //文件不存在或者文件大小为0,都当做不存在,重新 创建日志 截断 创建文件
  gets_request.file_off = file_off;
  // 创建tcp连接
  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(IP);
  server_addr.sin_port = htons(atoi(PORT));
  int fd_server = socket(AF_INET, SOCK_STREAM, 0);
  int ret = connect(fd_server, (struct sockaddr *)&server_addr, sizeof(server_addr));
  ERROR_CHECK(ret, -1, "connect");

  send(fd_server, &gets_request, sizeof(gets_request), 0); //发送request请求
  //此处接收一个文件大小 如果原来的文件大小是0,则按照这个截断,如果不是 则无需截断
  if (0 == file_size)
  {

    recv(fd_server, &file_size, sizeof(file_size), MSG_WAITALL);
    printf("zero file recv file_size = %ld\n", file_size);
    if (file_size <= 0)
    {
      printf("error!\n");
      pthread_exit(0);
    }
    fd_log_file = open(log_file_path, O_RDWR | O_CREAT, 0666);
    off_t tem = 0;
    write(fd_log_file, &tem, sizeof(tem)); //日志中,写入0,注意 一定要写入0,写入之后才能创建文件
    fd_file = open(gets_request.file_name, O_RDWR | O_CREAT, 0666);
    ftruncate(fd_file, file_size);
  }
  else
  {
    printf("duan_dian\n");
    off_t tem;
    recv(fd_server, &tem, sizeof(file_size), MSG_WAITALL);
    if (tem <= 0) // 文件大小,<=0 就是对方发过来的标志位了
    {
      printf("error!\n");
      pthread_exit(0);
    }
    printf("tem = %ld\n", tem); //文件大小不为0,说明已近截断过了,那么此时直接传输即可

    fd_log_file = open(log_file_path, O_RDWR | O_CREAT, 0666);//没有就创建
    fd_file = open(gets_request.file_name, O_RDWR | O_CREAT, 0666);
  }

  close(fd_file);
  close(fd_log_file);
  //开始接收文件
  printf("file_size = %ld file_off = %ld \n", file_size, file_off);
  if (file_size < 200 * 1024 * 1024 || file_size > 2147483647)
  {
    recv_file_by_train(gets_request.file_name, file_off, file_size, log_file_path, fd_server);
  }
  else
  {
    recv_file_by_splice(gets_request.file_name, file_off, file_size, log_file_path, fd_server);
  }
}


int recv_file_by_train(char *file_path, off_t file_off, off_t file_size, char *log_file_path, int fd_recv_from)
{
  //进入到这里,说明已经有了这两个文件
  off_t len = 0;
  char buf[1000] = {0};
  int ret = 0;
  int fd_file = open(file_path, O_RDWR);
  //打开文件,首先得偏移
  lseek(fd_file, file_off, SEEK_SET);
  int fd_log_file = open(log_file_path, O_RDWR);
  while (file_off < file_size)
  {
    ret = recvCycle(fd_recv_from, &len, sizeof(len)); //先接一个长度
    if (-1 == ret)
    {
      break;
    }
    if (0 == len)
    {
      printf("传输完毕\n");
    }
    ret = recvCycle(fd_recv_from, buf, len);//接受一定数量的文件内容
    if (-1 == ret)
    {
      break;
    }
    int file_ret = write(fd_file, buf, len);
    file_off += len; 
    //printf("file_off = %ld\n", file_off);
    lseek(fd_log_file, 0, SEEK_SET);
    int log_ret = write(fd_log_file, &file_off, sizeof(file_off));
    //printf("file_off = %ld log_ret = %d file_ret = %d\n", file_off, log_ret, file_ret);
  }
  close(fd_file);
  close(fd_log_file);
  if (file_off == file_size)
  { //如果传输结束,则删除日志文件
    remove(log_file_path);
    printf("删除日志文件成功\n");
    return 0;
  }
  else
  {
    return -1;
  }
}
int recvCycle(int fd_recv, void *buf, off_t requestLen)
{
  int downSize = 0;
  int ret = 0;
  while (downSize < requestLen)
  {
    ret = recv(fd_recv, (char *)buf + downSize, requestLen - downSize, 0);
    downSize += ret;
    // recv到0,说明对方切断了
    if (ret == 0)
    {
      return -1;
    }
  }
}

int recv_file_by_splice(char *file_path, off_t file_off, off_t file_size, char *log_file_path, int fd_recv_from)
{
  char buf[1000] = {0};
  int ret = 0;
  int fd_file = open(file_path, O_RDWR);
  //打开文件,首先得偏移
  lseek(fd_file, file_off, SEEK_SET);
  int fd_log_file = open(log_file_path, O_RDWR);
  //开始接收文件,使用splice来传输文件需要创建一个管道
  int fd_pipe[2] = {0};
  pipe(fd_pipe);
  //偏移到指定位置
  lseek(fd_file, file_off, SEEK_SET);
  while (file_off < file_size)
  {
    ret = splice(fd_recv_from, NULL, fd_pipe[1], NULL, 1000, SPLICE_F_MORE);
    if (0 == ret)
    {
      printf("连接中断\n");
      break;
    }
    splice(fd_pipe[0], NULL, fd_file, NULL, ret, SPLICE_F_MORE);
    file_off += ret;
    lseek(fd_log_file, 0, SEEK_SET);
    write(fd_log_file, &file_off, sizeof(file_off));
    printf("off %ld\n", file_off);
  }
  if (file_off == file_size)
  {
    remove(log_file_path);
    printf("文件传输结束,已删除传输日志\n");
    return 0;
  }
  else
  {
    printf("服务器连接中断\n");
    return -1;
  }
  close(fd_file);
  close(fd_log_file);
  close(fd_pipe[0]);
  close(fd_pipe[1]);
}

void *client_thread_func(void *p)
{
  Login_t puts_request = *((Login_t *)p); //读取文件大小,md5信息,然后将信息传输给主线程
  Compute_file_md5(puts_request.file_name, puts_request.md5_code);
  //printf("compute done,md5 code is %s",puts_request.md5_code);
  printf("file name =%s md5 =%s\n", puts_request.file_name, puts_request.md5_code);
  int fd_file = open(puts_request.file_name, O_RDWR);
  struct stat my_stat;
  bzero(&my_stat, sizeof(my_stat));
  fstat(fd_file, &my_stat);
  puts_request.file_size = my_stat.st_size;
  //向服务器发送请求
  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(IP);
  server_addr.sin_port = htons(atoi(PORT));

  int fd_server = socket(AF_INET, SOCK_STREAM, 0);
  int ret = connect(fd_server, (struct sockaddr *)&server_addr, sizeof(server_addr));
  ERROR_CHECK(ret, -1, "connect");
  send(fd_server, &puts_request, sizeof(puts_request), 0); //发送request请求
  //接收标志位 如果收到md5已存在的标志位,打印 相同文件存在,上传成功
  //如果收到 的是没有这个文件,则立即开始传输 puts暂不设置断点重传
  File_info_t file_info;
  bzero(&file_info, sizeof(file_info));
  recv(fd_server, &file_info, sizeof(file_info), MSG_WAITALL);
  if (-1 == file_info.file_size)
  {
    printf("file %s puts success,same file on the server\n", puts_request.file_name);
  }
  else if (1 == file_info.file_size)
  {
    //开始传输文件
    printf("start trans file\n");
    off_t send_cnt = 0;
    send_cnt = send_file_by_mmap(fd_server, my_stat.st_size, 0, fd_file);
    bzero(&file_info, sizeof(file_info));
    recv(fd_server, &file_info, sizeof(file_info), MSG_WAITALL);
    if (1 == file_info.file_size)
    {
      printf("puts file %s succeed send a new file\n", puts_request.file_name);
    }
    else
    {
      printf("puts file %s failed,try again\n", puts_request.file_name);
    }
  }
  else //如果没有收到任何信息,那么标志位是0,此时直接打印错误信息
  {
    printf("net error\n");
  }
  pthread_exit(0);
}

off_t send_file_by_mmap(int fd_to, off_t file_size, off_t file_off, int fd_file)
{
  printf("filesize %ld file_off %ld fd_file %d fd_to %d\n", file_size, file_off, fd_file, fd_to);
  //mmap的偏移值必须是4096的倍数
  char *pMap = mmap(NULL, file_size - file_off, PROT_READ | PROT_WRITE, MAP_SHARED, fd_file, file_off);
  ERROR_CHECK(pMap, (char *)-1, "mmap");
  off_t ret = send(fd_to, pMap, file_size - file_off, 0);
  return ret;
}