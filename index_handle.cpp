#include "index_handle.h"
#include <sstream>

namespace qiniu {
	namespace largefile {
		IndexHandler::IndexHandler(const std::string& base_path, const uint32_t main_block_id)
		{
			std::stringstream tmp_stream;
			tmp_stream << base_path << INDEX_DIR_PREFIX << main_block_id;

			std::string index_path;
			tmp_stream >> index_path;

			mmap_file_op_ = new MMapFileOperation(index_path);
			is_loaded = false;
		}

		IndexHandler::~IndexHandler()
		{
			if(is_loaded && mmap_file_op_){
				delete(mmap_file_op_);
				mmap_file_op_ = NULL;
			}
		}

		int IndexHandler::create(const uint32_t logic_block_id, const int32_t bucket_size, const MMapOption map_option)
		{
			int ret = TFS_SUCCESS;

			if (is_loaded) {
				return EXIT_INDEX_ALREADY_LOADED_ERROR;
			}

			int64_t file_size = mmap_file_op_->get_file_size();
			if (file_size < 0) {
				return TFS_ERROR;
			}
			else if (file_size > 0) {
				return EXIT_META_UNEXPECT_FOUND_ERROR;
			}
			else {//if file_size == 0����ʾ��ǰû��ӳ�������ļ����ڴ�
				IndexHeader init_header;	//�Ȳ�ֱ�ӷ���IndexHeader�Ŀռ䣬�Ժ�һ�����IndexHeader+bucket�Ŀռ�
				init_header.block_info_.block_id_ = logic_block_id;
				init_header.block_info_.seq_no_ = 1;
				init_header.bucket_size_ = bucket_size;

				init_header.index_file_size_ = sizeof(IndexHeader) + sizeof(int32_t)*bucket_size;

				//һ�����IndexHeader+bucket�Ŀռ䣬���ҳ�ʼ��
				/*
				init_data�ǰ������������ļ���Ϣ����ʱ�ڴ����ʼ��ַ���Ժ�init_data�����ʱ�ڴ棨buf��
				��pWrite_fileд�뵽�ڴ��е�ӳ�����򣬾�����������ļ��Ĵ�����
				*/
				char* init_data = new char[init_header.index_file_size_];
				memcpy(init_data, &init_header, sizeof(IndexHeader));
				memset(init_data + sizeof(IndexHeader), 0, init_header.index_file_size_ - sizeof(IndexHeader));

				//�����ҳ�ʼ��IndexHandle�����ļ���Ϣ֮����Щ�����ļ���Ϣд�������ļ���
				ret = mmap_file_op_->pWrite_file(init_data, init_header.index_file_size_, 0);
				if (ret != TFS_SUCCESS) {
					return ret;
				}

				//�������ļ����浽����
				ret = mmap_file_op_->flush_file();
				if (ret != TFS_SUCCESS) {
					return ret;
				}

				//���init_data����ֹ�ڴ�й©
				delete[] init_data;
				init_data = NULL;
			}

			//ӳ�������ļ����ڴ����
			ret = mmap_file_op_->mmap_file(map_option);
			if (ret != TFS_SUCCESS) {
				return ret;
			}

			is_loaded = true;

			if (debug) {
				printf("init block id: %d index successful. data file size: %d, index file size: %d. bucket_size: %d, free head offset: %d, seqno: %d, size: %d, file count: %d, del_size: %d, del_file_coount: %d, version: %d\n",
					logic_block_id, index_header()->data_file_offset_, index_header()->index_file_size_,
					index_header()->bucket_size_, index_header()->free_head_offset_, index_header()->block_info_.seq_no_,
					index_header()->block_info_.size_, index_header()->block_info_.file_count_,
					index_header()->block_info_.del_size_, index_header()->block_info_.del_file_count_,
					index_header()->block_info_.version_);

				printf("mmapped file size: %ld", mmap_file_op_->get_file_size());
			}

			return TFS_SUCCESS;
		}

		int IndexHandler::load(const uint32_t logic_block_id, const int32_t bucket_size, const MMapOption map_option)
		{
			int ret = TFS_SUCCESS;
			if (is_loaded) {
				return EXIT_INDEX_ALREADY_LOADED_ERROR;
			}

			int64_t mapped_size = mmap_file_op_->get_file_size();	//�õ�ӳ�䵽�ڴ���ļ��Ĵ�С
			if (mapped_size < 0) {
				return mapped_size;
			}
			else if (mapped_size == 0) {//empty file
				return EXIT_INDEX_CORRUPT_ERROR;
			}

			//ֻ��file_size����0�Ż�������У����������������
			//���file_size�ں����ӳ�����䣬��ô��ӳ��������״�ӳ��Ĵ�С�ĳ�file_size��
			//���file_sizeС���״�ӳ���С����ô����Ҫ�ı䣬ֱ�Ӱ����״�ӳ���С��ֵ��ӳ��
			//���file_size�������ӳ���С����ô�����޷�ӳ�䣬ֻ��ֱ�ӴӴ��̶�ȡ(mmap�������Զ��Ӵ��̶�ȡ)��Ҳ����Ҫ�������
			MMapOption tmp_map_op = map_option;
			if (mapped_size > tmp_map_op.first_mmap_size_ && mapped_size <= tmp_map_op.max_mmap_size_) {
				tmp_map_op.first_mmap_size_ = mapped_size;
			}

			ret = mmap_file_op_->mmap_file(tmp_map_op);//����ӳ�䣬���ص��ڴ���
			if (ret != TFS_SUCCESS) {
				return ret;
			}

			if (debug)	printf("indexHandle: load - bucket_size: %d, bucket size :%d, block id: %d\n", index_header()->bucket_size_ ,get_bucket_size(), block_info()->block_id_);

			//���ӳ����ļ���bucket size��block id�Ƿ�Ϸ�
			if (get_bucket_size() == 0 || block_info()->block_id_ == 0) {
				fprintf(stderr, "index corrupt error. block id: %u, bucket size: %d\n", block_info()->block_id_, get_bucket_size());
				return EXIT_INDEX_CORRUPT_ERROR;
			}

			//���file size�Ƿ�������ļ��Ĵ�С�Ƿ����
			int32_t index_file_size = sizeof(IndexHeader) + sizeof(int32_t) * get_bucket_size();
			if (mapped_size < index_file_size) {
				//fprintf(stderr, "IndexHeader: %lu, int32_t: %lu, get_bucket_size():%d\n", sizeof(IndexHeader), sizeof(int32_t), get_bucket_size());
				fprintf(stderr, "index corrupt error. block id: %u, bucket size: %d, file size: %ld, index file size: %d\n",
					block_info()->block_id_, get_bucket_size(), mapped_size, index_file_size);
				return EXIT_INDEX_CORRUPT_ERROR;
			}

			//��������id��ӳ���ļ���id�Ƿ����
			if (logic_block_id != block_info()->block_id_) {
				fprintf(stderr, "block id conflict error. block id: %u, index block id: %u\n",
					logic_block_id,block_info()->block_id_);
				return EXIT_BLOCK_ID_CONFLICT_ERROR;
			}

			//��������bucket size��ӳ���ļ���bucket size�Ƿ����
			if (bucket_size != get_bucket_size()) {
				fprintf(stderr, "Index configure error. old bucket size: %d, new bucket size: %d\n",
					get_bucket_size(), bucket_size);
				return EXIT_BUCKET_CONFIGURE_ERROR;
			}

			if(debug)
				printf("Load block id: %d index successful. data file size: %d, index file size: %d. bucket_size: %d, free head offset: %d, seqno: %d, size: %d, file count: %d, del_size: %d, del_file_coount: %d, version: %d\n",
					logic_block_id, index_header()->data_file_offset_, index_header()->index_file_size_,
					index_header()->bucket_size_, index_header()->free_head_offset_, index_header()->block_info_.seq_no_,
					index_header()->block_info_.size_, index_header()->block_info_.file_count_,
					index_header()->block_info_.del_size_, index_header()->block_info_.del_file_count_,
					index_header()->block_info_.version_);

			return TFS_SUCCESS;
		}

		int IndexHandler::remove(const uint32_t logic_block_id)
		{
			if (is_loaded) {//���ȷʵloaded�ˣ���ô���id�Ƿ�ƥ��
				if (logic_block_id != block_info()->block_id_) {
					fprintf(stderr, "index corrupt error. logic id: %d, mapped idL %d", logic_block_id, block_info()->block_id_);
					return largefile::EXIT_INDEX_CORRUPT_ERROR;
				}
			}
			//���ûload��Ҳ�����ճ�unmap��unlink����Ϊ������method�ڲ����ж��Ƿ�load�پ����Ƿ�ִ��
			int ret = mmap_file_op_->munmap_file();
			if (ret != largefile::TFS_SUCCESS) {
				return ret;
			}

			ret = mmap_file_op_->unlink_file();
			return ret;
		}

		int IndexHandler::flush()
		{
			int ret = mmap_file_op_->flush_file();
			if (ret != TFS_SUCCESS) {
				fprintf(stderr, "index flush fails. ret: %d, error desc: %s\n", ret, strerror(errno));
				return ret;
			}
			return ret;
		}

		int32_t IndexHandler::write_segment_meta(const uint64_t key, MetaInfo& meta_info)
		{
			int32_t current_offset = 0, previous_offset = 0;
			int ret = hash_find(key, current_offset, previous_offset);
			if (ret != EXIT_META_NOT_FOUND) {//�����Ӧ��keyֵ�Ѵ��ڻ����б������
				if (ret == TFS_SUCCESS) {//�����ǰ��key�Ѵ��ڣ�����error
					return EXIT_META_UNEXPECT_FOUND_ERROR;
				}
				//���򣬷����Ǹ���������ı��
				return ret;
			}
			//�����Ӧ��key�����ڣ���ô���Բ��롣
			ret = hash_insert(key, previous_offset, meta_info);
			if (ret != TFS_SUCCESS) {
				return ret;
			}
			return TFS_SUCCESS;
		}

		// ͨ��key�ҵ��ڴ��ж�Ӧ��meta�ļ����ҽ��ڴ��е�meta�ļ��������ã�meta_info
		int32_t IndexHandler::read_segment_meta(const uint64_t key, MetaInfo& meta_info)
		{
			int32_t current_offset = 0, previous_offset = 0;
			int ret = hash_find(key, current_offset, previous_offset);

			if (ret == TFS_SUCCESS) {//�����ǰ��key�Ѵ���
				ret = mmap_file_op_->pRead_file(reinterpret_cast<char*>(&meta_info), sizeof(MetaInfo), current_offset);
				return ret;
			}
			else {
				return ret;
			}
		}

		int32_t IndexHandler::delete_segment_meta(const uint64_t key)
		{
			int32_t current_offset = 0, previous_offset = 0;
			int ret = hash_find(key, current_offset, previous_offset);	//�ж��Ƿ����ҵ�key�����õ�offsets

			if (ret != TFS_SUCCESS) {//�����ǰ��key�����ڣ�����error
				return ret;
			}

			MetaInfo meta_info;
			ret = mmap_file_op_->pRead_file(reinterpret_cast<char*>(&meta_info), sizeof(MetaInfo), current_offset);
			if (ret != TFS_SUCCESS) {
				return ret;
			}

			int32_t next_pos = meta_info.get_next_meta_offset();

			if (previous_offset != 0) {//�����ǰ�ýڵ�Ļ�
				//�õ�ǰһ��������ڵ����Ϣ
				MetaInfo previous_meta_info;
				ret = mmap_file_op_->pRead_file(reinterpret_cast<char*>(&previous_meta_info), sizeof(MetaInfo), previous_offset);
				if (ret != TFS_SUCCESS) {
					return ret;
				}
				previous_meta_info.set_next_meta_offset(next_pos);//�Ѵ�ɾ���ڵ��ǰ�ڵ�ͺ�ڵ����ӣ�������������

				//���޸�֮�����������Ϣд��������ڵ�
				ret = mmap_file_op_->pWrite_file(reinterpret_cast<char*>(&previous_meta_info), sizeof(MetaInfo), previous_offset);
				if (ret != TFS_SUCCESS) {
					return ret;
				}
			}
			else {//���û��ǰ�ýڵ㣬Ҫɾ���Ľڵ���ǵ�һ���ڵ�Ļ����Ѷ�Ӧ��ϣͰ����������Ϊ��һ������offset
				int32_t slot = static_cast<uint32_t>(key) % get_bucket_size();
				get_first_meta_info()[slot] = next_pos;	
			}

			//��ɾ���Ľڵ��������ýڵ��б�
			meta_info.set_next_meta_offset(index_header()->free_head_offset_);//��ͷ�巨����ǰ�ڵ��������ýڵ��б��ǰ����ʹ��ɾ���ڵ����һ�����ָ��֮ǰ�Ŀ����ýڵ��б��е�ͷ�ڵ�
			ret = mmap_file_op_->pWrite_file(reinterpret_cast<char*>(&meta_info), sizeof(MetaInfo), current_offset);//����Ա�ɾ���ڵ���޸�
			if (ret != TFS_SUCCESS) {
				return ret;
			}
			index_header()->free_head_offset_ = current_offset;	//���¿����ýڵ��б��е�ͷ�ڵ㣬ʹ��ɾ���ڵ��Ϊ�½ڵ�
			if (debug)	printf("Delete_segment_meta - Reuse meta_info, current offset: %d\n", current_offset);

			//���¿���Ϣ
			update_block_info(C_OPER_DELETE, meta_info.get_size());	//��������ö�������test�ļ����������Ϊtest�ļ����޷��õ�meta_info����Ϣ

			return TFS_SUCCESS;
		}

		int IndexHandler::hash_find(const uint64_t key, int32_t& current_offset, int32_t& previous_offset)
		{
			int ret = TFS_SUCCESS;
			MetaInfo cur_meta_info;

			current_offset = 0;
			previous_offset = 0;

			int32_t slot = static_cast<uint32_t>(key) % get_bucket_size();	//�õ���ǰkey��ʾͰ�еڼ�����������ϣֵ�Ƕ��٣�
			//ע�⣬���ڹ�ϣͰ�е����нڵ㶼��һ��int32���͵ı���������ָ�����Ĺ�ϣ�����������ʼ��ʱ��pos = 0��
			int32_t pos = get_first_meta_info()[slot];	//�õ�meta_info/��ϣͰ������ͷ����һ��ָ�룩����ͨ����ϣֵslot���ҵ�Ͱ��Ӧ�Ĺ�ϣ����
			
			/* ���pos == 0��˵����ϣͰ�еĵ�slot�������ǿյģ���û�нڵ㣬��ʱ����Ҫ��������ֱ�ӷ���NOT FOUND
			ֻ�е�pos != 0���ű�ʾ��ϣͰ�е�slot���������Ѿ��нڵ��ˣ���ʱ��Ҫ��while����*/
			while (pos != 0) {
				//��ȡmmap_file_op�е�data��IndexHandler���еĶ�Ӧ��meta_info��cur_meta_info��ȥ�����offset�Ǹոյõ���pos����С��һ��MetaInfo�Ĵ�С
				ret = mmap_file_op_->pRead_file(reinterpret_cast<char*>(&cur_meta_info), sizeof(MetaInfo), pos);
				if (ret != TFS_SUCCESS) {
					return ret;
				}

				if (hash_compare(cur_meta_info.get_key(), key)) {
					current_offset = pos;
					return	TFS_SUCCESS;
				}

				previous_offset = pos;
				pos = cur_meta_info.get_next_meta_offset();//�������±�����ֱ�������ڵ�ʱ��next_meta_offset��Ϊ0
			}
			return EXIT_META_NOT_FOUND;
		}

		int IndexHandler::hash_insert(const uint64_t key, int32_t previous_offset, MetaInfo& meta)
		{
			int ret = TFS_SUCCESS;
			MetaInfo tmp_meta_info;
			int32_t current_offset = 0;	//current_offset��ʾ�����ļ����ڴ��ڵ�λ�ã�meta�������Ϣ����Ҫ���������λ��

			//1.�ҳ�������һ����ϣͰ
			int32_t slot = static_cast<uint32_t>(key) % get_bucket_size();

			//2.ȷ��meta�ڵ�洢���ļ��е�ƫ����
			if (index_header()->free_head_offset_ != 0) {//��������ýڵ��б����нڵ㣬��������ӵ���Щ�ڵ���
				MetaInfo tmp_meta_info;
				ret = mmap_file_op_->pRead_file(reinterpret_cast<char*>(&tmp_meta_info), sizeof(MetaInfo), index_header()->free_head_offset_);	//ȡ�ÿ������б��еĽڵ����Ϣ������֮���޸�index_header��free_head_offset���׽ڵ㡣
				if (ret != TFS_SUCCESS) {
					return ret;
				}
				current_offset = index_header()->free_head_offset_;	//ȡ�ÿ������б��е��׽ڵ��λ����Ϊ�����ļ��Ĵ��λ��
				if (debug)	printf("Reuse meta_info, current offset: %d\n", current_offset);
				index_header()->free_head_offset_ = tmp_meta_info.get_next_meta_offset();	//�������ýڵ��б���׽ڵ�����Ϊ֮ǰͷ�ڵ����һ���ڵ�
			}
			else {//���򣬿����ýڵ��б���û�нڵ�ʱ��������ʹ�ù��������ļ��б��ĩβ��ѡ��һ���µ�meta_info
				current_offset = index_header()->index_file_size_;	//�ҵ������ļ������ĩβ����ĩβƫ�ƣ���Ϊ�����ļ��Ĵ��λ��
				index_header()->index_file_size_ += sizeof(MetaInfo);//�����ļ��ĵ�ǰƫ����/��С+1
			}
			//3.��meta�ڵ����Ϣд�뵽�����ļ���
			meta.set_next_meta_offset(0);//���������ĩβ�ڵ��next meta offsetһ����0
			ret = mmap_file_op_->pWrite_file(reinterpret_cast<char*>(&meta), sizeof(MetaInfo), current_offset);
			if (ret != TFS_SUCCESS) {
				index_header()->index_file_size_ -= sizeof(MetaInfo);	//�ع������ļ��ĵ�ǰƫ����/��С
				return ret;
			}

			//4.����meta��Ϣ�����ļ����뵽��ϣ������
			if (previous_offset != 0) {//�������֮ǰ�Ľڵ㣬����ǰ�ڵ㲻���׽ڵ㣬��ǰһ�ڵ��next offset����Ϊ���ڵ��offset
				ret = mmap_file_op_->pRead_file(reinterpret_cast<char*>(&tmp_meta_info), sizeof(MetaInfo), previous_offset);
				if (ret != TFS_SUCCESS) {
					index_header()->index_file_size_ -= sizeof(MetaInfo);	//�ع������ļ��ĵ�ǰƫ����/��С
					return ret;
				}
				tmp_meta_info.set_next_meta_offset(current_offset);	//��������
				ret = mmap_file_op_->pWrite_file(reinterpret_cast<char*>(&tmp_meta_info), sizeof(MetaInfo), previous_offset);//��֮ǰ�Ľڵ�д��ȥ����
				if (ret != TFS_SUCCESS) {
					index_header()->index_file_size_ -= sizeof(MetaInfo);	//�ع������ļ��ĵ�ǰƫ����/��С
					return ret;
				}
			}
			else {//������ǰһ�ڵ�������ʹ��ǰ�ڵ��ƫ��������Ϊcurrent_offset
				get_first_meta_info()[slot] = current_offset;
			}

			return TFS_SUCCESS;
		}

		int IndexHandler::update_block_info(const OperationType oper_type, const int32_t file_size)
		{
			if (block_info()->block_id_ == 0) {
				return EXIT_BLOCK_ID_ZERO_ERROR;
			}

			if (oper_type == C_OPER_INSERT) {
				block_info()->file_count_++;
				block_info()->version_++;
				block_info()->seq_no_++;
				block_info()->size_ += file_size;
			}
			else if (oper_type == C_OPER_DELETE) {
				block_info()->file_count_--;
				block_info()->version_++;
				block_info()->del_file_count_++;
				block_info()->size_ -= file_size;
				block_info()->del_size_ += file_size;
			}

			if (debug) {
				printf("update succssfully, seqno: %d, size: %d, file count: %d, del_size: %d, del_file_coount: %d, version: %d\n",
					index_header()->block_info_.seq_no_,
					index_header()->block_info_.size_, index_header()->block_info_.file_count_,
					index_header()->block_info_.del_size_, index_header()->block_info_.del_file_count_,
					index_header()->block_info_.version_);
			}
			return TFS_SUCCESS;
		}
	}
}