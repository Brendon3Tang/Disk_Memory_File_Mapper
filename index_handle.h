#ifndef LARGE_FILE_INDEX_HANDLE_H
#define LARGE_FILE_INDEX_HANDLE_H

#include "common.h"
#include "mmap_file_op.h"

static int debug = 1;

namespace qiniu {
	namespace largefile {
		struct IndexHeader
		{
			IndexHeader() {
				memset(this, 0, sizeof(IndexHeader));
			}

			BlockInfo block_info_;		//meta�Ŀ���Ϣ
			int32_t bucket_size_;		//��ϣͰ��С
			int32_t data_file_offset_;	//δʹ�����ݵ���ʼƫ��
			int32_t index_file_size_;	//�����ļ��Ĵ�С��ͬʱҲ��ƫ����(offset after index_header+all buckets),ָ����һ��������������ļ�λ��
			int32_t free_head_offset_;	//�����õ�����ڵ㣨��֮ǰɾ���Ľڵ㣬���ǲ�δ��ı�ɾ����ֻ�Ǳ�����ˣ�
		};

		class IndexHandler {
		public:
			IndexHandler(const std::string& base_path, const uint32_t main_block_id);
			~IndexHandler();
			int create(const uint32_t logic_block_id, const int32_t bucket_size, const MMapOption map_option);
			int load(const uint32_t logic_block_id, const int32_t bucket_size, const MMapOption map_option);
			int remove(const uint32_t logic_block_id);
			int flush();
			int32_t write_segment_meta(const uint64_t key, MetaInfo& meta_info);
			int32_t read_segment_meta(const uint64_t key, MetaInfo& meta_info);
			int32_t delete_segment_meta(const uint64_t key);
			int hash_find(const uint64_t key, int32_t& current_offset, int32_t& previous_offset);
			int hash_insert(const uint64_t key, int32_t previous_offset, MetaInfo& meta);
			void commit_block_data_offset(const int file_size) {
				reinterpret_cast<IndexHeader*>(mmap_file_op_->get_map_data())->data_file_offset_ += file_size;
			}
			int update_block_info(const OperationType oper_type, const int32_t file_size);

			//mmap_file_op_����map��֮��mmap_file_op_���data���������ļ����������ļ���IndexHeader��ͷ�����Կ���ǿ��ת��
			IndexHeader* index_header() {
				return reinterpret_cast<IndexHeader*>(mmap_file_op_->get_map_data());
			}
			//mmap_file_op_����map��֮��mmap_file_op_���data���������ļ����������ļ���IndexHeader��ͷ��IndexHeader����BlockInfo��ͷ�����Կ���ǿ��ת��
			BlockInfo* block_info() {
				return reinterpret_cast<BlockInfo*>(mmap_file_op_->get_map_data());
			}

			int32_t get_bucket_size() const {
				return reinterpret_cast<IndexHeader*>(mmap_file_op_->get_map_data())->bucket_size_;
			}

			int32_t get_data_file_offset() const {
				return reinterpret_cast<IndexHeader*>(mmap_file_op_->get_map_data())->data_file_offset_;
			}

			//����Ƶ�е�bucket_slot
			int32_t* get_first_meta_info() const {
				//��ת����char����ָ�룬����ͨ���ֽ����ƶ���1��charռ1���ֽ�Byte�������ת����int32_t����ָ��
				return reinterpret_cast<int32_t*>(reinterpret_cast<char*>(mmap_file_op_->get_map_data()) + sizeof(IndexHeader));
			}

			bool hash_compare(const uint64_t left_key, const uint64_t right_key) {
				return left_key == right_key;
			}
		private:
			MMapFileOperation* mmap_file_op_;
			bool is_loaded;
		};
	}
}

#endif // !LARGE_FILE_INDEX_HANDLE_H
