#include "kernel/types.h"
#include "user.h"

//父进程向管道中写入数据，子进程从管道将其读出并打印<pid>: received ping from pid <father pid>
//子进程从父进程收到数据后，通过写入另一个管道向父进程传输数据，然后由父进程从该管道读取并打印<pid>: received pong from pid <child pid>

//???管道在关闭后能否再进行关闭
int main(int argc, char* argv[]){
    int c2f[2];//0是读取，1是写入
    int f2c[2];
    pipe(c2f);
    pipe(f2c);
    int pid = fork();
    if(pid==0){
        //从f2c中读取父进程pid
        char msg[20];
        close(f2c[1]);
        read(f2c[0], msg, 20);
        close(f2c[0]);
        printf("%d: received ping from pid %s\n", getpid(), msg);

        //通过c2f与父进程通信
        char kpid[20];
        itoa(getpid(), kpid);
        close(c2f[0]);
        write(c2f[1], kpid, 20);
        close(c2f[1]);
        exit(0);
    }
    else{
        char fpid[20];
        itoa(getpid(), fpid);
        close(f2c[0]);//关闭读取端
        write(f2c[1], fpid, 20);
        close(f2c[1]);//关闭写入端

        char msgc[20];
        close(c2f[1]);
        read(c2f[0], msgc, 20);
        printf("%d: received pong from pid %s\n", getpid(), msgc);
        close(c2f[0]);
        exit(0);
    }
}