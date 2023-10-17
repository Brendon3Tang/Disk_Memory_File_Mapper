#include "mmap_file_op.h"

static int debug = 1;
using namespace std;

namespace qiniu {
	namespace largefile {
		int MMapFileOperation::mmap_file(const largefile::MMapOption& mmap_option)
		{
			if (mmap_option.max_mmap_size_ < mmap_option.first_mmap_size_
				|| mmap_option.max_mmap_size_ <= 0)	return TFS_ERROR;

			int fd = check_file();//保证文件一定已经打开了
			if (fd < 0) {
				fprintf(stderr, "MMapFileOperation::mmap_file - check_file failed!\n");
				return TFS_ERROR;
			}

			if (!is_Mapped_) {//如果没有映射
				if (map_file_) {//没有映射但是有map_file_指针，那么说明是之前的指针
					delete(map_file_);
				}
				map_file_ = new MMapFile(mmap_option, fd);
				is_Mapped_ = map_file_->map_file(true);
			}

			if (is_Mapped_)
				return TFS_SUCCESS;
			else
				return TFS_ERROR;
		}

		int MMapFileOperation::munmap_file()
		{
			//if (!is_Mapped_)	return TFS_SUCCESS;
			if (is_Mapped_ && map_file_) {
				delete(map_file_);
				map_file_ = NULL;
				is_Mapped_ = false;
			}
			return TFS_SUCCESS;
		}

		// pRead_file()可以把内存中的文件内容直接拷贝到到buf上，而不需要经过调用read使用页缓存
		int MMapFileOperation::pRead_file(char* buf, const int32_t size, const int64_t offset)
		{
			//情况一：当文件已经映射到内存时
			if (is_Mapped_ && map_file_) {
				//a.当要访问的地址大于内存已映射的地址时，我们可以为内存映射扩容一次
				if (size + offset > map_file_->get_size()) {
					if (debug)
						fprintf(stdout, "MMapFileOperation: pRead, size: %d, offset; %" __PRI64_PREFIX"d, map file size: %d. need remap\n.",size, offset, map_file_->get_size());
					map_file_->remap_file();
				}
				//b.当要访问的地址在内存已映射的地址内部时，我们可以直接访问
				if (size + offset <= map_file_->get_size()) {
					memcpy(buf, (char*)map_file_->get_data() + offset, size);	//把map_file_的数据放到buf上以便读取，不再使用read的页缓存读取
					return TFS_SUCCESS;
				}
			}
			//情况二：当文件没有被映射到内存上时，使用read函数，经过页缓存
			return FileOperation::pRead_file(buf, size, offset);	
		}

		//pWrite_file会把buf里的内容写入到内存中，而不需要使用write经过页缓存
		int MMapFileOperation::pWrite_file(const char* buf, const int32_t size, const int64_t offset)
		{
			//情况一：当文件已经映射到内存时
			if (is_Mapped_ && map_file_) {
				//a.当要访问的地址大于内存已映射的地址时，我们可以为内存映射扩容一次
				if (size + offset > map_file_->get_size()) {
					if (debug)
						fprintf(stdout, "MMapFileOperation: pWrite, size: %d, offset; %" __PRI64_PREFIX"d, map file size: %d. need remap\n.", size, offset, map_file_->get_size());
					map_file_->remap_file();
				}
				//b.当要访问的地址在内存已映射的地址内部时，我们可以直接访问
				if (size + offset <= map_file_->get_size()) {
					memcpy((char*)map_file_->get_data() + offset, buf, size);	//此处不同：把buf的数据放到map_file_上以便之后保存
					return TFS_SUCCESS;
				}
			}
			//情况二：当文件没有被映射到内存上时，使用write函数，经过页缓存
			return FileOperation::pWrite_file(buf, size, offset);
		}
		void* MMapFileOperation::get_map_data() const
		{
			if (is_Mapped_)	return map_file_->get_data();

			return NULL;
		}
		int MMapFileOperation::flush_file()
		{
			if (is_Mapped_ && map_file_) {//如果文件已经映射到内存了，那么直接用同步的方式保存
				if (map_file_->sync_file())	return TFS_SUCCESS;
				else return TFS_ERROR;
			}

			return FileOperation::flush_file();	//否则如果没映射到内存则直接调用基类的flush_file
		}
	}
}

