#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#define LONGPATH (200+DIRSIZ+3)
/**
 * @path:对应路径
 */
void search(char *path, char *target) {
  // printf("查找%s\n", path);
  if(strlen(path)>200){
    fprintf(2, "find: path(%s) too long\n", path);
    return;
  }
  char buf[LONGPATH], *p;
  int fd;
  struct dirent de;
  struct stat st;

  // 获取文件描述符
  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  // 获取path对应文件或者目录的stat
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  // path不是一个目录
  if (st.type != T_DIR) {
    fprintf(2, "find: %s is not a directory\n", path);
    close(fd);
    return;
  }

  // path是一个目录
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    struct stat content;  // de 和content都是表示该目录下的内容
    // 跳过无效文件和.和..
    if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
      continue;
    }

    // 获取该目录下每个项目的完整路径名保存在buf中
    memset(buf, 0, sizeof(buf));
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;

    // 根据完整路径名字获取文件类型，来决定是否需要递归
    if (stat(buf, &content) < 0) {
      printf("ls: cannot stat %s\n", buf);
      continue;
    }
  
    if (content.type == T_FILE) {
      if (strcmp(de.name, target) == 0) {
        printf("%s\n", buf);
      }
    } else if (content.type == T_DIR) {
      if (strcmp(de.name, target) == 0) {
        printf("%s\n", buf);
      }
      char newfolder[LONGPATH];
      strcpy(newfolder, buf);
      // printf("进入%s\n", newfolder);
      // printf("长度%d\n", strlen(newfolder));
      search(newfolder, target);
    } else {
      continue;
    }
  }
  close(fd);
  return;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(2, "expect 2 args, but get %d\n", argc - 1);
    exit(0);
  }
  search(argv[1], argv[2]);
  exit(0);
}

