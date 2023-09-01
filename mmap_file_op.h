#ifndef LARGEFILE_MMAP_FILE_OP_H_
#define LARGEFILE_MMAP_FILE_OP_H_
#include "common.h"
#include "mmap_file.h"
#include "file_op.h"

namespace qiniu {
	namespace largefile {
		class MMapFileOperation : public largefile::FileOperation {
		public:
			MMapFileOperation(const std::string& file_name, const int open_flags = O_CREAT | O_RDWR | O_LARGEFILE) :
				FileOperation(file_name, open_flags), map_file_(NULL), is_Mapped_(false) {}

			~MMapFileOperation() {
				if (map_file_) {
					delete(map_file_);
					map_file_ = NULL;
				}
			}

			int mmap_file(const largefile::MMapOption& mmap_option);
			int munmap_file();

			int pRead_file(char* buf, const int32_t size, const int64_t offset);
			int pWrite_file(const char* buf, const int32_t size, const int64_t offset);

			void* get_map_data() const;
			int flush_file();

		private:
			MMapFile* map_file_;
			bool is_Mapped_;
		};
	}
}
#endif // LARGEFILE_MMAP_FILE_OP_H_
