#include "mmap_file.h"
#include <stdio.h>

static int debug = 1;

namespace qiniu{
    namespace largefile{
        MMapFile::MMapFile():
        size_(0), fd_(-1), data_(NULL)
        {
        }

        MMapFile::MMapFile(const int fd):
        size_(0), fd_(fd), data_(NULL)
        {
        }

        MMapFile::MMapFile(const MMapOption& mmap_option, const int fd):
        size_(0), fd_(fd), data_(NULL)
        {
            mmap_file_option_.max_mmap_size_ = mmap_option.max_mmap_size_;
            mmap_file_option_.first_mmap_size_ = mmap_option.first_mmap_size_;
            mmap_file_option_.per_mmap_size_  = mmap_option.per_mmap_size_;
        }

        MMapFile::~MMapFile(){
            if(data_){
                if(debug){
                    printf("mmap_file destruct, fd: %d, maped size: %d, data: %p\n", fd_, size_, data_);
                }
                msync(data_, size_, MS_SYNC);   //在析构之前确保内存与磁盘中的文件一致，且要选择同步而非异步，保证更新完成之后再继续析构
                munmap(data_, size_);   //取消映射

                size_ = 0;
                data_ = NULL;
                mmap_file_option_.max_mmap_size_ = 0;
                mmap_file_option_.first_mmap_size_ = 0;
                mmap_file_option_.per_mmap_size_  = 0;
            }
        }

        bool MMapFile::sync_file(){
            if(data_ != NULL){
                return msync(data_, size_, MS_ASYNC) == 0;  //msync用于保存映射在内存的文件I/O到磁盘上
            }

            return true;
        }

        bool MMapFile::map_file(const bool write){
            int PROT = PROT_READ;   //默认可读

            if(write){
                PROT |= PROT_WRITE; //设置了的话就是可写
            }

            if(fd_ < 0) return false;   

            //最大的映射大小为0时不能映射
            if(mmap_file_option_.max_mmap_size_ == 0)   return false;

            //size_即表示稍后将要映射的文件的大小（需求大小）
            //选项中最大映射的大小 >= size_的大小）时，可以给size_分配first_mmap_size_，进行第一次映射
            if(size_ < mmap_file_option_.max_mmap_size_ ){
                size_ = mmap_file_option_.first_mmap_size_;
            }
            else{//如果需求的映射大小size_ >= 最大映射大小时，只给需求size_分配max_mmap_size_
                size_ = mmap_file_option_.max_mmap_size_;
            }

            if(!ensure_file_size(size_)){
                fprintf(stderr, "ensure file size failed in map_file, size: %d\n", size_);
                return false;
            }
            
            data_ = mmap(0, size_, PROT, MAP_SHARED, fd_, 0);

            if(MAP_FAILED == data_){
                fprintf(stderr, "map file failed: %s\n", strerror(errno));

                size_ = 0;
                fd_ = -1;
                data_ = NULL;
                return false;
            }

            if(debug)   printf("mmap file successed, fd: %d, maped size: %d, data: %p\n", fd_, size_, data_);
            return true;
        }

        void* MMapFile::get_data() const{   //获取映射到内存的数据的首地址，由于不知道是什么类型的数据，所以使用void*指针
            return data_;
        }

        int32_t MMapFile::get_size() const{   //当前已经映射到内存的size
            return size_;
        }

        bool MMapFile::munmap_file(){ //解除映射
            if(munmap(data_, size_) == 0)  return true;

            return false;
        }

        bool MMapFile::ensure_file_size(const int32_t size){
            struct stat s;  //存放文件状态
            if(fstat(fd_, &s) < 0){ //获取文件状态信息，保存在s中,小于0表示获取失败
                fprintf(stderr, "fstat error, error description: %s\n", strerror(errno));
                return false;
            }

            if(s.st_size < size){   //文件size小于指定的size
                if(ftruncate(fd_, size) < 0){   //给文件大小扩容
                    fprintf(stderr, "ftruncate error, size: %d, error description: %s\n", size, strerror(errno));
                    return false;
                }
            }

            return true;
        }

        //remap_file()会为文件重新映射一个新的更大的内存
        bool MMapFile::remap_file(){  
            if(fd_ < 0 || !data_){
                fprintf(stderr, "mremap not mapped yet\n");
                return false;
            }

            //如果之前的size_已经抵达了极限，不能再重新扩容映射了
            if(size_ == mmap_file_option_.max_mmap_size_){
                fprintf(stderr, "already reached the max map size, now size: %d, max size: %d\n",
                size_, mmap_file_option_.max_mmap_size_);
                return false;
            }

            int32_t new_size = size_ + mmap_file_option_.per_mmap_size_;

            if(new_size > mmap_file_option_.max_mmap_size_){
                new_size = mmap_file_option_.max_mmap_size_;
            }

            if(!ensure_file_size(new_size)){//如果文件扩容失败
                fprintf(stderr, "ensure file size failed in remap_file, size: %d\n", new_size);
                return false;
            }

            if(debug){
                printf("mremap start, fd: %d, now size: %d, new size: %d, old data: %p\n",
                fd_,
                size_,
                new_size,
                data_);
            }

            void *new_map_data = mremap(data_, size_, new_size, MREMAP_MAYMOVE);

            //如果新的映射结果失败了
            if(new_map_data == MAP_FAILED){
                fprintf(stderr, "mremap failed, fd: %d, new size: %d, error description: %s\n",
                fd_,
                new_size,
                strerror(errno));
                return false;
            }
            else{//如果成功了
                if(debug)   printf("mremap success. fd: %d, now size: %d, new size: %d, old data: %p, new data: %p\n",
                fd_,
                size_,
                new_size,
                data_,
                new_map_data);
            }

            //成功后更新数据
            data_ = new_map_data;
            size_ = new_size;
            return true;
        }
    }
}