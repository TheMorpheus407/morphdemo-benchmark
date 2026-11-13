#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/wait.h>
extern char **environ;
int main(int argc,char**argv){
  if(argc>1 && !strcmp(argv[1],"child")){ char b[256]; int n=readlink("/proc/self/exe",b,255); b[n>0?n:0]=0; printf("child running, exe=%s DISPLAY=%s\n",b,getenv("DISPLAY")); return 0; }
  for(int flags=0; flags<=0x10; flags+=0x10){
    int fd=syscall(SYS_memfd_create,"",flags);
    printf("memfd_create(flags=%#x) -> %d errno=%d\n",flags,fd,fd<0?errno:0);
    if(fd<0) continue;
    int in=open("/proc/self/exe",O_RDONLY); char buf[65536]; int n; while((n=read(in,buf,sizeof buf))>0) write(fd,buf,n); close(in);
    pid_t p=fork();
    if(!p){ char*av[]={argv[0],"child",0}; syscall(SYS_execveat,fd,"",av,environ,0x1000); printf("execveat failed errno=%d\n",errno); _exit(9);}
    int st; waitpid(p,&st,0); printf("  child status %d\n",st);
    close(fd);
  }
  FILE*f=fopen("/proc/sys/vm/memfd_noexec","r"); if(f){int v;fscanf(f,"%d",&v);printf("vm.memfd_noexec=%d\n",v);fclose(f);} else puts("no memfd_noexec sysctl readable");
  return 0;
}
