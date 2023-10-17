#include "mmap_file_op.h"

static int debug = 1;
using namespace std;

namespace qiniu {
	namespace largefile {
		int MMapFileOperation::mmap_file(const largefile::MMapOption& mmap_option)
		{
			if (mmap_option.max_mmap_size_ < mmap_option.first_mmap_size_
				|| mmap_option.max_mmap_size_ <= 0)	return TFS_ERROR;

			int fd = check_file();//��֤�ļ�һ���Ѿ�����
			if (fd < 0) {
				fprintf(stderr, "MMapFileOperation::mmap_file - check_file failed!\n");
				return TFS_ERROR;
			}

			if (!is_Mapped_) {//���û��ӳ��
				if (map_file_) {//û��ӳ�䵫����map_file_ָ�룬��ô˵����֮ǰ��ָ��
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

		// pRead_file()���԰��ڴ��е��ļ�����ֱ�ӿ�������buf�ϣ�������Ҫ��������readʹ��ҳ����
		int MMapFileOperation::pRead_file(char* buf, const int32_t size, const int64_t offset)
		{
			//���һ�����ļ��Ѿ�ӳ�䵽�ڴ�ʱ
			if (is_Mapped_ && map_file_) {
				//a.��Ҫ���ʵĵ�ַ�����ڴ���ӳ��ĵ�ַʱ�����ǿ���Ϊ�ڴ�ӳ������һ��
				if (size + offset > map_file_->get_size()) {
					if (debug)
						fprintf(stdout, "MMapFileOperation: pRead, size: %d, offset; %" __PRI64_PREFIX"d, map file size: %d. need remap\n.",size, offset, map_file_->get_size());
					map_file_->remap_file();
				}
				//b.��Ҫ���ʵĵ�ַ���ڴ���ӳ��ĵ�ַ�ڲ�ʱ�����ǿ���ֱ�ӷ���
				if (size + offset <= map_file_->get_size()) {
					memcpy(buf, (char*)map_file_->get_data() + offset, size);	//��map_file_�����ݷŵ�buf���Ա��ȡ������ʹ��read��ҳ�����ȡ
					return TFS_SUCCESS;
				}
			}
			//����������ļ�û�б�ӳ�䵽�ڴ���ʱ��ʹ��read����������ҳ����
			return FileOperation::pRead_file(buf, size, offset);	
		}

		//pWrite_file���buf�������д�뵽�ڴ��У�������Ҫʹ��write����ҳ����
		int MMapFileOperation::pWrite_file(const char* buf, const int32_t size, const int64_t offset)
		{
			//���һ�����ļ��Ѿ�ӳ�䵽�ڴ�ʱ
			if (is_Mapped_ && map_file_) {
				//a.��Ҫ���ʵĵ�ַ�����ڴ���ӳ��ĵ�ַʱ�����ǿ���Ϊ�ڴ�ӳ������һ��
				if (size + offset > map_file_->get_size()) {
					if (debug)
						fprintf(stdout, "MMapFileOperation: pWrite, size: %d, offset; %" __PRI64_PREFIX"d, map file size: %d. need remap\n.", size, offset, map_file_->get_size());
					map_file_->remap_file();
				}
				//b.��Ҫ���ʵĵ�ַ���ڴ���ӳ��ĵ�ַ�ڲ�ʱ�����ǿ���ֱ�ӷ���
				if (size + offset <= map_file_->get_size()) {
					memcpy((char*)map_file_->get_data() + offset, buf, size);	//�˴���ͬ����buf�����ݷŵ�map_file_���Ա�֮�󱣴�
					return TFS_SUCCESS;
				}
			}
			//����������ļ�û�б�ӳ�䵽�ڴ���ʱ��ʹ��write����������ҳ����
			return FileOperation::pWrite_file(buf, size, offset);
		}
		void* MMapFileOperation::get_map_data() const
		{
			if (is_Mapped_)	return map_file_->get_data();

			return NULL;
		}
		int MMapFileOperation::flush_file()
		{
			if (is_Mapped_ && map_file_) {//����ļ��Ѿ�ӳ�䵽�ڴ��ˣ���ôֱ����ͬ���ķ�ʽ����
				if (map_file_->sync_file())	return TFS_SUCCESS;
				else return TFS_ERROR;
			}

			return FileOperation::flush_file();	//�������ûӳ�䵽�ڴ���ֱ�ӵ��û����flush_file
		}
	}
}

