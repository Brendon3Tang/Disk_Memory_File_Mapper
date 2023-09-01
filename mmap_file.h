#ifndef LARGEFILE_MMAPFILE_H_
#define LARGEFILE_MMAPFILE_H_
#include "common.h"

namespace qiniu{
    namespace largefile{
        class MMapFile{
        public:
            MMapFile();
            explicit MMapFile(const int fd);
            MMapFile(const MMapOption& mmap_option, const int fd);
            ~MMapFile();

            bool sync_file();//同步内存数据到文件
            bool map_file(const bool write = false);    //映射文件到内存，同时设置访问权限write
            void* get_data() const;   //获取映射到内存的数据的首地址，由于不知道是什么类型的数据，所以使用void*指针
            int32_t get_size() const;   //数据的size

            bool munmap_file(); //解除映射
            bool remap_file();  //重新映射

        private:
            bool ensure_file_size(const int32_t size);    //调整大小

        private:
            int32_t size_;
            int fd_;
            void* data_;
            struct MMapOption mmap_file_option_;
        };
    }
}

#endif