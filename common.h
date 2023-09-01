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
            int32_t max_mmap_size_; //�ڴ�ӳ�������С
            int32_t first_mmap_size_;   //��һ�η���Ĵ�С
            int32_t per_mmap_size_; // ֮�����ÿ��Ҫ׷�ӵĴ�С
        };

        //���ļ���Ϣ
        struct BlockInfo
        {
            uint32_t block_id_;             //����   1 ......2^32-1  TFS = NameServer + DataServer
            int32_t version_;               //�鵱ǰ�汾��
            int32_t file_count_;            //��ǰ�ѱ����ļ�����
            int32_t size_;                  //��ǰ�ѱ����ļ������ܴ�С
            int32_t del_file_count_;        //��ɾ�����ļ�����
            int32_t del_size_;              //��ɾ�����ļ������ܴ�С
            uint32_t seq_no_;               //��һ���ɷ�����ļ����  1 ...2^64-1    

            //���캯��
            BlockInfo() {
                memset(this, 0, sizeof(BlockInfo));//ֱ�Ӱ����еı������㣨��ʼ����
            }

            inline bool operator==(const BlockInfo& rhs) const {
                return block_id_ == rhs.block_id_ && version_ == rhs.version_ &&
                    file_count_ == rhs.file_count_ && size_ == rhs.size_ &&
                    del_size_ == rhs.del_size_ && del_file_count_ == rhs.del_file_count_ &&
                    seq_no_ == rhs.seq_no_;
            }
        };

        //�ļ���ϣ������
        struct MetaInfo {
            MetaInfo() {    //���캯��
                init();
            }

            MetaInfo(const uint64_t file_id, const int32_t inner_offset,
                const int32_t file_size, const int32_t next_meta_offset) {
                file_id_ = file_id;
                location_.inner_offset_ = inner_offset;
                location_.size_ = file_size;
                next_meta_offset_ = next_meta_offset;
            }

            //�������캯��
            MetaInfo(const MetaInfo& meta_info) {
                memcpy(this, &meta_info, sizeof(MetaInfo));
            }

            // key����file_id�����Ǵ˴���keyǿ���ǹ�ϣ�ļ�ֵ�����Ժ�key���ܸ�Ϊ��Ķ�������file_id_��
            uint64_t get_key()  const {
                return file_id_;
            }

            void set_key(const uint64_t key) {
                file_id_ = key;
            }

            // �˴���file_id_��ָfile id����
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

            //��¡����
            MetaInfo& clone(const MetaInfo& meta_info) {
                assert(this != &meta_info); //�������Լ���¡�Լ�

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

        private:    //���ںܶ�ط����漰MetaInfo����˽���Ա�������ó�˽�бȽϺã�ͨ���ӿڷ���
            uint64_t file_id_;                 //�ļ����
            struct                            //�ļ�Ԫ����
            {
                int32_t inner_offset_;        //�ļ��ڿ��ڲ���ƫ����
                int32_t size_;                //�ļ���С
            } location_;
            int32_t next_meta_offset_;        //��ǰ��ϣ����һ���ڵ��������ļ��е�ƫ����

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