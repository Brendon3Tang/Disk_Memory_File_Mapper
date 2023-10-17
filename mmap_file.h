#ifndef LARGEFILE_MMAPFILE_H_
#define LARGEFILE_MMAPFILE_H_
#include "common.h"

namespace qiniu{
    namespace largefile{
        /**
         *      MMapFile会把文件fd_映射到内存中，通过私有成员data_记录内存地址，size_记录
         * 已经映射的内存的大小，同时mmap_file_option_记录映射规则。使用MMapFile需要显示
         * 地通过fd初始化MMapFile。
         *      提供接口map_file、munmap_file、remap_file分别用于映射fd、解除映射fd_、以及
         * 为映射了的fd_扩容/缩容。
         *      提供接口get_data与get_size用于外部访问映射的内存和得到映射的内存的大小
         *      提供接口sync_file用于同步内存中的数据到磁盘中
        */
        class MMapFile{
        public:
            MMapFile();
            explicit MMapFile(const int fd);
            MMapFile(const MMapOption& mmap_option, const int fd);
            ~MMapFile();

            bool sync_file();//同步内存数据到磁盘
            bool map_file(const bool write = false);    //映射文件到内存，同时设置访问权限write
            void* get_data() const;   //获取映射到内存的数据的首地址，由于不知道是什么类型的数据，所以使用void*指针
            int32_t get_size() const;   //当前已经映射到内存的size

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