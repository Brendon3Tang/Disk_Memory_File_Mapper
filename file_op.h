#ifndef LARGE_FILE_OP_H
#define LARGE_FILE_OP_H

#include "common.h"

namespace qiniu{
    namespace largefile{
        /*
        * FileOperation可以直接从磁盘打开文件，并进行读取/写入等操作，是MMapFileOperation的基类
        */
        class FileOperation{
        public:
            FileOperation();
            FileOperation(const std::string &file_name, const int open_flags = O_RDWR | O_LARGEFILE);
            ~FileOperation();

            int open_file();
            void close_file();

            int flush_file();   //把文件立即写入磁盘
            int unlink_file();  //把文件删除
            
            virtual int pRead_file(char *buf, const int32_t nBytes, const int64_t offset);   //精细化读
            virtual int pWrite_file(const char *buf, const int32_t nBytes, const int64_t offset);   //精细化写
            int write_file(const char *buf, const int32_t nBytes);  //从当前位置开始写

            int64_t get_file_size();
            int get_fd()const{
                return fd_;
            }

            int fTruncate_file(const int64_t length);   //缩小/截断文件的函数 （int64_t方便兼容大文件）
            int seek_file(int64_t offset);  //移动文件指针，跳跃式地读取文件
            
        protected:  //后面会涉及到继承
            int fd_;
            int open_flags_;
            char* file_name_;

        protected:
            int check_file();

        protected:
            static const mode_t OPEN_MODE = 0644;   //wr for owner, r for group and other
            static const int MAX_DISK_TIMES = 5;    //最大的磁盘读取次数。以此区分磁盘是临时故障或者永久故障。
        };
    }
}

#endif