#ifndef _COMMON_H_INCLUDED_
#define _COMMON_H_INCLUDED_

#include <iostream>
#include <string>
#include <string.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

namespace qiniu {
	namespace largefile {
		const int32_t EXIT_DISK_OPER_INCOMPLETE = -8012;	//read or write length is less than the required.
        const int32_t EXIT_INDEX_ALREADY_LOADED_ERROR = -8013;
        const int32_t EXIT_META_UNEXPECT_FOUND_ERROR = -8014;
        const int32_t EXIT_INDEX_CORRUPT_ERROR = -8015;
        const int32_t EXIT_BLOCK_ID_CONFLICT_ERROR = -8016;
        const int32_t EXIT_BUCKET_CONFIGURE_ERROR = -8017;
        const int32_t EXIT_META_NOT_FOUND = -8018;
        const int32_t EXIT_BLOCK_ID_ZERO_ERROR = -8019;

		const int32_t TFS_SUCCESS = 0;	
		const int32_t TFS_ERROR = -1;

        static const std::string MAINBLOCK_DIR_PREFIX = "/mainblock/";
        static const std::string INDEX_DIR_PREFIX = "/index/";
        static const mode_t DIR_MODE = 0755;

        enum OperationType {
            C_OPER_INSERT = 1,
            C_OPER_DELETE
        };

        struct MMapOption {
            int32_t max_mmap_size_; //内存映射的最大大小
            int32_t first_mmap_size_;   //第一次分配的大小
            int32_t per_mmap_size_; // 之后分配每次要追加的大小
        };

        //块文件信息
        struct BlockInfo
        {
            uint32_t block_id_;             //块编号   1 ......2^32-1  TFS = NameServer + DataServer
            int32_t version_;               //块当前版本号
            int32_t file_count_;            //当前已保存文件总数
            int32_t size_;                  //当前已保存文件数据总大小
            int32_t del_file_count_;        //已删除的文件数量
            int32_t del_size_;              //已删除的文件数据总大小
            uint32_t seq_no_;               //下一个可分配的文件编号  1 ...2^64-1    

            //构造函数
            BlockInfo() {
                memset(this, 0, sizeof(BlockInfo));//直接把所有的变量清零（初始化）
            }

            inline bool operator==(const BlockInfo& rhs) const {
                return block_id_ == rhs.block_id_ && version_ == rhs.version_ &&
                    file_count_ == rhs.file_count_ && size_ == rhs.size_ &&
                    del_size_ == rhs.del_size_ && del_file_count_ == rhs.del_file_count_ &&
                    seq_no_ == rhs.seq_no_;
            }
        };

        //文件哈希索引块
        struct MetaInfo {
            MetaInfo() {    //构造函数
                init();
            }

            MetaInfo(const uint64_t file_id, const int32_t inner_offset,
                const int32_t file_size, const int32_t next_meta_offset) {
                file_id_ = file_id;
                location_.inner_offset_ = inner_offset;
                location_.size_ = file_size;
                next_meta_offset_ = next_meta_offset;
            }

            //拷贝构造函数
            MetaInfo(const MetaInfo& meta_info) {
                memcpy(this, &meta_info, sizeof(MetaInfo));
            }

            // key就是file_id，但是此处的key强调是哈希的键值（且以后key可能改为别的东西而非file_id_）
            uint64_t get_key()  const {
                return file_id_;
            }

            void set_key(const uint64_t key) {
                file_id_ = key;
            }

            // 此处的file_id_特指file id本身
            uint64_t get_file_id()  const {
                return file_id_;
            }

            void set_file_id(const uint64_t file_id) {
                file_id_ = file_id;
            }

            int32_t get_offset() const {
                return location_.inner_offset_;
            }

            void set_offset(const int32_t offset) {
                location_.inner_offset_ = offset;
            }

            int32_t get_size() const {
                return location_.size_;
            }

            void set_size(const int32_t size) {
                location_.size_ = size;
            }

            int32_t get_next_meta_offset() const {
                return next_meta_offset_;
            }

            void set_next_meta_offset(const int32_t meta_offset) {
                next_meta_offset_ = meta_offset;
            }

            MetaInfo& operator=(const MetaInfo& meta_info) {
                if (this == &meta_info)  return *this;

                file_id_ = meta_info.file_id_;
                location_.inner_offset_ = meta_info.location_.inner_offset_;
                location_.size_ = meta_info.location_.size_;
                next_meta_offset_ = meta_info.next_meta_offset_;

                return *this;
            }

            //克隆函数
            MetaInfo& clone(const MetaInfo& meta_info) {
                assert(this != &meta_info); //不允许自己克隆自己

                file_id_ = meta_info.file_id_;
                location_.inner_offset_ = meta_info.location_.inner_offset_;
                location_.size_ = meta_info.location_.size_;
                next_meta_offset_ = meta_info.next_meta_offset_;

                return *this;
            }

            bool operator==(const MetaInfo& rhs)    const {
                return file_id_ == rhs.file_id_&&
                location_.inner_offset_ == rhs.location_.inner_offset_&&
                location_.size_ == rhs.location_.size_&&
                next_meta_offset_ == rhs.next_meta_offset_;
            }

        private:    //由于很多地方会涉及MetaInfo，因此将成员变量设置成私有比较好，通过接口访问
            uint64_t file_id_;                 //文件编号
            struct                            //文件元数据
            {
                int32_t inner_offset_;        //文件在块内部的偏移量
                int32_t size_;                //文件大小
            } location_;
            int32_t next_meta_offset_;        //当前哈希链下一个节点在索引文件中的偏移量

        private:
            void init() {
                file_id_ = 0;
                location_.inner_offset_ = 0;
                location_.size_ = 0;
                next_meta_offset_ = 0;
            }
        };
	}
}

#endif