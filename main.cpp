#include "common.h"
#include "mmap_file.h"

using namespace std;
using namespace qiniu;

static const mode_t OPEN_MODE = 0644; //rw for usr, r for group and other
const static largefile::MMapOption mmap_option = {10240000, 4096, 4096};    //内存映射参数

int open_file(string file_name, int open_flags){
    //fd是文件句柄，在Linux里即每个文件的id
    int fd = open(file_name.c_str(), open_flags, OPEN_MODE);//成功的返回值一定>0

    if(fd < 0){
        return -errno; //errno一定是正数
    }

    return fd;
}

int main(void){
    const char *fileName = "./mapfile_test.txt";
    //1. 打开/创建一个文件，取得文件的句柄：open函数
    int fd = open_file(fileName, O_RDWR | O_CREAT | O_LARGEFILE);
    if(fd < 0){
        fprintf(stderr, "open file failed. file name: %s, error desc: %s\n", fileName, strerror(-fd));
        return -1;
    }

    largefile::MMapFile *map_file = new largefile::MMapFile(mmap_option, fd);

    bool is_loaded = map_file->map_file(true);
    
    if(is_loaded){
        map_file->remap_file();
        memset(map_file->get_data(), '9', map_file->get_size());    //把这个快内存全部设置为8
        map_file->sync_file();  //同步到硬盘里的文件中
        map_file->munmap_file();//解除映射
    }
    else{
        fprintf(stderr, "map file failed\n");
    }

    close(fd);  //关闭文件

    return 0;
}