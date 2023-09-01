#include "file_op.h"

namespace qiniu {
	namespace largefile {
		FileOperation::FileOperation(const std::string& file_name, const int open_flags) :
			fd_(-1), open_flags_(open_flags)
		{
			file_name_ = strdup(file_name.c_str()); //stdup相当于把参数中的值重新分配内存，然后返回这块内存
		}
		
		FileOperation::~FileOperation()
		{
			if (fd_ > 0) {
				::close(fd_);
			}

			if (file_name_ != NULL) {
				free(file_name_);
				file_name_ = NULL;
			}
		}
		int FileOperation::open_file()
		{
			if (fd_ > 0) {	//如果fd已经打开
				close(fd_);
				fd_ = -1;
			}

			fd_ = ::open(file_name_, open_flags_, OPEN_MODE);
			if (fd_ < 0) {
				return -errno;
			}
			return fd_;
		}
		void FileOperation::close_file()
		{
			if (fd_ < 0)
				return;

			::close(fd_);
			fd_ = -1;
		}
		int FileOperation::flush_file()
		{
			if (open_flags_ && O_SYNC)//O_SYNC以同步地方式打开数据，没必要再flush
				return 0;

			int fd = check_file();
			if (fd < 0)
				return fd;

			return fsync(fd);	//将缓存区（内存）的数据同步到磁盘
		}

		int FileOperation::unlink_file()
		{
			close_file();	//确保文件关闭
			return ::unlink(file_name_);	//删除文件
		}

		int FileOperation::pRead_file(char* buf, const int32_t nBytes, const int64_t offset)
		{
			int32_t left = nBytes;
			int32_t read_len = 0;
			int64_t read_offset = offset;
			char* cur_buf = buf;
			int i = 0;

			while (left > 0) {
				i++;
				if (i >= MAX_DISK_TIMES)	break;

				if (check_file() < 0)	return -errno;

				read_len = pread64(fd_, cur_buf, left, read_offset);

				if (read_len < 0) {
					read_len = -errno;

					if (-read_len == EINTR || -read_len == EAGAIN)	
						continue;
					else if (-read_len == EBADF) {
						fd_ = -1;
						continue;
					}
					else
						return read_len;
				}
				else if (read_len == 0) {	//如果读到了末尾
					break;
				}

				cur_buf += read_len;
				read_offset += read_len;
				left -= read_len;
			}

			if (left != 0) {	//如果循环完成但是还有剩余，说明没有完成
				return EXIT_DISK_OPER_INCOMPLETE;
			}
			return TFS_SUCCESS;
		}

		int FileOperation::pWrite_file(const char* buf, const int32_t nBytes, const int64_t offset)
		{
			int32_t left = nBytes;
			int32_t written_len = 0;
			int64_t written_offset = offset;
			const char* cur_buf = buf;
			int i = 0;

			while (left > 0) {
				i++;
				if (i >= MAX_DISK_TIMES)	break;

				if (check_file() < 0)	return -errno;

				written_len = pwrite64(fd_, cur_buf, left, written_offset);

				if (written_len < 0) {
					written_len = -errno;

					if (-written_len == EINTR || -written_len == EAGAIN)
						continue;
					else if (-written_len == EBADF) {
						fd_ = -1;
						continue;
					}
					else
						return written_len;
				}
				else if (written_len == 0) {	//如果读到了末尾
					break;
				}

				cur_buf += written_len;
				written_offset += written_len;
				left -= written_len;
			}

			if (left != 0) {	//如果循环完成但是还有剩余，说明没有完成
				return EXIT_DISK_OPER_INCOMPLETE;
			}
			return TFS_SUCCESS;
		}

		int FileOperation::write_file(const char* buf, const int32_t nBytes)
		{
			int32_t left = nBytes;
			int32_t written_len = 0;
			const char* cur_buf = buf;
			int i = 0;

			while (left > 0) {
				i++;
				if (i >= MAX_DISK_TIMES)	break;

				if (check_file() < 0)	return -errno;

				written_len = write(fd_, cur_buf, left);

				if (written_len < 0) {
					written_len = -errno;

					if (-written_len == EINTR || -written_len == EAGAIN)
						continue;
					else if (-written_len == EBADF) {
						fd_ = -1;
						continue;
					}
					else
						return written_len;
				}

				cur_buf += written_len;
				left -= written_len;
			}

			if (left != 0) {	//如果循环完成但是还有剩余，说明没有完成
				return EXIT_DISK_OPER_INCOMPLETE;
			}
			return TFS_SUCCESS;
		}

		int64_t FileOperation::get_file_size()
		{
			int fd = check_file();

			if (fd < 0) {
				return -1;
			}

			struct stat buf;
			if (fstat(fd, &buf) != 0) {
				return -1;
			}

			return buf.st_size;
		}

		int FileOperation::fTruncate_file(const int64_t length)
		{
			int fd = check_file();
			if (fd < 0) {
				return -1;
			}

			return ftruncate(fd, length);
		}

		int FileOperation::seek_file(int64_t offset)
		{
			int fd = check_file();

			if (fd < 0)
				return -1;

			return lseek(fd, offset, SEEK_SET);	//SEEK_SET表示offset被设置成当前位置+offset bytes
		}

		int FileOperation::check_file()
		{
			if (fd_ < 0) {
				fd_ = open_file();
			}
			return fd_;
		}
	}
}