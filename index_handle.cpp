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
			else {//if file_size == 0，表示当前没有映射索引文件到内存
				IndexHeader init_header;	//先不直接分配IndexHeader的空间，稍后一起分配IndexHeader+bucket的空间
				init_header.block_info_.block_id_ = logic_block_id;
				init_header.block_info_.seq_no_ = 1;
				init_header.bucket_size_ = bucket_size;

				init_header.index_file_size_ = sizeof(IndexHeader) + sizeof(int32_t)*bucket_size;

				//一起分配IndexHeader+bucket的空间，并且初始化
				/*
				init_data是包含整个索引文件信息的临时内存的起始地址，稍后将init_data这个临时内存（buf）
				用pWrite_file写入到内存中的映射区域，就完成了索引文件的创建。
				*/
				char* init_data = new char[init_header.index_file_size_];
				memcpy(init_data, &init_header, sizeof(IndexHeader));
				memset(init_data + sizeof(IndexHeader), 0, init_header.index_file_size_ - sizeof(IndexHeader));

				//分配且初始化IndexHandle索引文件信息之后将这些索引文件信息写入索引文件中
				ret = mmap_file_op_->pWrite_file(init_data, init_header.index_file_size_, 0);
				if (ret != TFS_SUCCESS) {
					return ret;
				}

				//将索引文件保存到磁盘
				ret = mmap_file_op_->flush_file();
				if (ret != TFS_SUCCESS) {
					return ret;
				}

				//清空init_data，防止内存泄漏
				delete[] init_data;
				init_data = NULL;
			}

			//映射索引文件到内存访问
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

			int64_t mapped_size = mmap_file_op_->get_file_size();	//得到映射到内存的文件的大小
			if (mapped_size < 0) {
				return mapped_size;
			}
			else if (mapped_size == 0) {//empty file
				return EXIT_INDEX_CORRUPT_ERROR;
			}

			//只有file_size大于0才会继续运行，但仍有三种情况：
			//如果file_size在合理的映射区间，那么把映射规则中首次映射的大小改成file_size。
			//如果file_size小于首次映射大小，那么不需要改变，直接按照首次映射大小的值来映射
			//如果file_size大于最大映射大小，那么由于无法映射，只能直接从磁盘读取(mmap函数会自动从磁盘读取)，也不需要另外操作
			MMapOption tmp_map_op = map_option;
			if (mapped_size > tmp_map_op.first_mmap_size_ && mapped_size <= tmp_map_op.max_mmap_size_) {
				tmp_map_op.first_mmap_size_ = mapped_size;
			}

			ret = mmap_file_op_->mmap_file(tmp_map_op);//进行映射，加载到内存中
			if (ret != TFS_SUCCESS) {
				return ret;
			}

			if (debug)	printf("indexHandle: load - bucket_size: %d, bucket size :%d, block id: %d\n", index_header()->bucket_size_ ,get_bucket_size(), block_info()->block_id_);

			//检查映射的文件的bucket size和block id是否合法
			if (get_bucket_size() == 0 || block_info()->block_id_ == 0) {
				fprintf(stderr, "index corrupt error. block id: %u, bucket size: %d\n", block_info()->block_id_, get_bucket_size());
				return EXIT_INDEX_CORRUPT_ERROR;
			}

			//检查file size是否和索引文件的大小是否相等
			int32_t index_file_size = sizeof(IndexHeader) + sizeof(int32_t) * get_bucket_size();
			if (mapped_size < index_file_size) {
				//fprintf(stderr, "IndexHeader: %lu, int32_t: %lu, get_bucket_size():%d\n", sizeof(IndexHeader), sizeof(int32_t), get_bucket_size());
				fprintf(stderr, "index corrupt error. block id: %u, bucket size: %d, file size: %ld, index file size: %d\n",
					block_info()->block_id_, get_bucket_size(), mapped_size, index_file_size);
				return EXIT_INDEX_CORRUPT_ERROR;
			}

			//检查输入的id与映射文件的id是否相等
			if (logic_block_id != block_info()->block_id_) {
				fprintf(stderr, "block id conflict error. block id: %u, index block id: %u\n",
					logic_block_id,block_info()->block_id_);
				return EXIT_BLOCK_ID_CONFLICT_ERROR;
			}

			//检查输入的bucket size与映射文件的bucket size是否相等
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
			if (is_loaded) {//如果确实loaded了，那么检查id是否匹配
				if (logic_block_id != block_info()->block_id_) {
					fprintf(stderr, "index corrupt error. logic id: %d, mapped idL %d", logic_block_id, block_info()->block_id_);
					return largefile::EXIT_INDEX_CORRUPT_ERROR;
				}
			}
			//如果没load，也可以照常unmap和unlink，因为这两个method内部会判断是否load再决定是否执行
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
			if (ret != EXIT_META_NOT_FOUND) {//如果对应的key值已存在或者有别的问题
				if (ret == TFS_SUCCESS) {//如果当前的key已存在，返回error
					return EXIT_META_UNEXPECT_FOUND_ERROR;
				}
				//否则，返回那个具体问题的编号
				return ret;
			}
			//如果对应的key不存在，那么可以插入。
			ret = hash_insert(key, previous_offset, meta_info);
			if (ret != TFS_SUCCESS) {
				return ret;
			}
			return TFS_SUCCESS;
		}

		// 通过key找到内存中对应的meta文件并且将内存中的meta文件存入引用：meta_info
		int32_t IndexHandler::read_segment_meta(const uint64_t key, MetaInfo& meta_info)
		{
			int32_t current_offset = 0, previous_offset = 0;
			int ret = hash_find(key, current_offset, previous_offset);

			if (ret == TFS_SUCCESS) {//如果当前的key已存在
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
			int ret = hash_find(key, current_offset, previous_offset);	//判断是否能找到key，并得到offsets

			if (ret != TFS_SUCCESS) {//如果当前的key不存在，返回error
				return ret;
			}

			MetaInfo meta_info;
			ret = mmap_file_op_->pRead_file(reinterpret_cast<char*>(&meta_info), sizeof(MetaInfo), current_offset);
			if (ret != TFS_SUCCESS) {
				return ret;
			}

			int32_t next_pos = meta_info.get_next_meta_offset();

			if (previous_offset != 0) {//如果有前置节点的话
				//得到前一个索引块节点的信息
				MetaInfo previous_meta_info;
				ret = mmap_file_op_->pRead_file(reinterpret_cast<char*>(&previous_meta_info), sizeof(MetaInfo), previous_offset);
				if (ret != TFS_SUCCESS) {
					return ret;
				}
				previous_meta_info.set_next_meta_offset(next_pos);//把待删除节点的前节点和后节点连接，保持链表完整

				//把修改之后的索引块信息写入索引块节点
				ret = mmap_file_op_->pWrite_file(reinterpret_cast<char*>(&previous_meta_info), sizeof(MetaInfo), previous_offset);
				if (ret != TFS_SUCCESS) {
					return ret;
				}
			}
			else {//如果没有前置节点，要删除的节点就是第一个节点的话，把对应哈希桶的内容设置为下一个结点的offset
				int32_t slot = static_cast<uint32_t>(key) % get_bucket_size();
				get_first_meta_info()[slot] = next_pos;	
			}

			//将删除的节点加入可重用节点列表
			meta_info.set_next_meta_offset(index_header()->free_head_offset_);//用头插法将当前节点插入可重用节点列表的前方，使被删除节点的下一个结点指向之前的可重用节点列表中的头节点
			ret = mmap_file_op_->pWrite_file(reinterpret_cast<char*>(&meta_info), sizeof(MetaInfo), current_offset);//保存对被删除节点的修改
			if (ret != TFS_SUCCESS) {
				return ret;
			}
			index_header()->free_head_offset_ = current_offset;	//更新可重用节点列表中的头节点，使被删除节点成为新节点
			if (debug)	printf("Delete_segment_meta - Reuse meta_info, current offset: %d\n", current_offset);

			//更新块信息
			update_block_info(C_OPER_DELETE, meta_info.get_size());	//在这里调用而不是在test文件里调用是因为test文件里无法得到meta_info的信息

			return TFS_SUCCESS;
		}

		int IndexHandler::hash_find(const uint64_t key, int32_t& current_offset, int32_t& previous_offset)
		{
			int ret = TFS_SUCCESS;
			MetaInfo cur_meta_info;

			current_offset = 0;
			previous_offset = 0;

			int32_t slot = static_cast<uint32_t>(key) % get_bucket_size();	//得到当前key表示桶中第几个链表（即哈希值是多少）
			//注意，由于哈希桶中的所有节点都是一个int32类型的变量，用来指向具体的哈希链表，所以在最开始的时候pos = 0。
			int32_t pos = get_first_meta_info()[slot];	//得到meta_info/哈希桶的数组头（是一个指针），并通过哈希值slot来找到桶对应的哈希链表
			
			/* 如果pos == 0，说明哈希桶中的第slot个链表还是空的，并没有节点，此时不需要做遍历，直接返回NOT FOUND
			只有当pos != 0，才表示哈希桶中第slot个链表中已经有节点了，此时需要用while遍历*/
			while (pos != 0) {
				//读取mmap_file_op中的data（IndexHandler）中的对应的meta_info到cur_meta_info里去，因此offset是刚刚得到的pos，大小是一个MetaInfo的大小
				ret = mmap_file_op_->pRead_file(reinterpret_cast<char*>(&cur_meta_info), sizeof(MetaInfo), pos);
				if (ret != TFS_SUCCESS) {
					return ret;
				}

				if (hash_compare(cur_meta_info.get_key(), key)) {
					current_offset = pos;
					return	TFS_SUCCESS;
				}

				previous_offset = pos;
				pos = cur_meta_info.get_next_meta_offset();//继续向下遍历，直到到最后节点时，next_meta_offset会为0
			}
			return EXIT_META_NOT_FOUND;
		}

		int IndexHandler::hash_insert(const uint64_t key, int32_t previous_offset, MetaInfo& meta)
		{
			int ret = TFS_SUCCESS;
			MetaInfo tmp_meta_info;
			int32_t current_offset = 0;	//current_offset表示索引文件在内存内的位置，meta里面的信息就是要保存至这个位置

			//1.找出放在哪一个哈希桶
			int32_t slot = static_cast<uint32_t>(key) % get_bucket_size();

			//2.确定meta节点存储在文件中的偏移量
			if (index_header()->free_head_offset_ != 0) {//如果可重用节点列表中有节点，则优先添加到这些节点里
				MetaInfo tmp_meta_info;
				ret = mmap_file_op_->pRead_file(reinterpret_cast<char*>(&tmp_meta_info), sizeof(MetaInfo), index_header()->free_head_offset_);	//取得可重用列表中的节点的信息，方便之后修改index_header中free_head_offset的首节点。
				if (ret != TFS_SUCCESS) {
					return ret;
				}
				current_offset = index_header()->free_head_offset_;	//取得可重用列表中的首节点的位置作为索引文件的存放位置
				if (debug)	printf("Reuse meta_info, current offset: %d\n", current_offset);
				index_header()->free_head_offset_ = tmp_meta_info.get_next_meta_offset();	//将可重用节点列表的首节点设置为之前头节点的下一个节点
			}
			else {//否则，可重用节点列表中没有节点时，则在已使用过的索引文件列表的末尾，选择一个新的meta_info
				current_offset = index_header()->index_file_size_;	//找到索引文件链表的末尾的最末尾偏移，作为索引文件的存放位置
				index_header()->index_file_size_ += sizeof(MetaInfo);//索引文件的当前偏移量/大小+1
			}
			//3.将meta节点的信息写入到索引文件里
			meta.set_next_meta_offset(0);//这个新增的末尾节点的next meta offset一定是0
			ret = mmap_file_op_->pWrite_file(reinterpret_cast<char*>(&meta), sizeof(MetaInfo), current_offset);
			if (ret != TFS_SUCCESS) {
				index_header()->index_file_size_ -= sizeof(MetaInfo);	//回滚索引文件的当前偏移量/大小
				return ret;
			}

			//4.将有meta信息索引文件插入到哈希链表中
			if (previous_offset != 0) {//如果存在之前的节点，即当前节点不是首节点，则将前一节点的next offset设置为本节点的offset
				ret = mmap_file_op_->pRead_file(reinterpret_cast<char*>(&tmp_meta_info), sizeof(MetaInfo), previous_offset);
				if (ret != TFS_SUCCESS) {
					index_header()->index_file_size_ -= sizeof(MetaInfo);	//回滚索引文件的当前偏移量/大小
					return ret;
				}
				tmp_meta_info.set_next_meta_offset(current_offset);	//连接链表
				ret = mmap_file_op_->pWrite_file(reinterpret_cast<char*>(&tmp_meta_info), sizeof(MetaInfo), previous_offset);//将之前的节点写回去保存
				if (ret != TFS_SUCCESS) {
					index_header()->index_file_size_ -= sizeof(MetaInfo);	//回滚索引文件的当前偏移量/大小
					return ret;
				}
			}
			else {//不存在前一节点的情况则使当前节点的偏移量设置为current_offset
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