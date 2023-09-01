#include "file_op.h"
#include "index_handle.h"
#include <sstream>

using namespace std;
using namespace qiniu;

const static largefile::MMapOption mmap_option = { 1024000, 4096, 4096 };
const static uint32_t main_block_size = 1024 * 1024 * 64;//主块文件的大小，64MB
const static uint32_t bucket_size = 1000;//哈希桶的大小

int main(int argc, char** argv) {
	uint32_t block_id;
	string main_block_path;
	string index_path;
	int32_t ret = largefile::TFS_SUCCESS;

	cout << "Type your block id: " << endl;
	cin >> block_id;

	if (block_id < 1) {
		cerr << "无效的block id" << endl;
		exit(-1);
	}

	//1.加载索引文件
	largefile::IndexHandler* index_handler = new largefile::IndexHandler(".", block_id);
	if (debug)	printf("Load index ..\n");

	ret = index_handler->load(block_id, bucket_size, mmap_option);
	if (ret != largefile::TFS_SUCCESS) {
		fprintf(stderr, "Load index: %d failed\n", block_id);
		delete index_handler;
		exit(-1);
	}

	//2. 读取文件meta info
	uint64_t file_id = 0;
	cout << "Type your file ID: " << endl;
	cin >> file_id;

	if (file_id < 1) {
		cerr << "Invalid file id" << endl;
		exit(-2);
	}

	largefile::MetaInfo meta;
	ret = index_handler->read_segment_meta(file_id, meta);
	if (ret != largefile::TFS_SUCCESS) {
		fprintf(stderr, "read_segement_meta error, file_id: %lu, ret: %d\n", file_id, ret);
		exit(-3);
	}
	
	//3.根据meta info读取文件
	stringstream tmp_stream;
	tmp_stream << "." << largefile::MAINBLOCK_DIR_PREFIX << block_id;
	tmp_stream >> main_block_path;
	largefile::FileOperation* main_block = new largefile::FileOperation(main_block_path, O_RDWR);

	char *buffer = new char[meta.get_size()+1];
	ret = main_block->pRead_file(buffer, meta.get_size(), meta.get_offset());
	if (ret != largefile::TFS_SUCCESS) {
		fprintf(stderr, "Read from main block %s failed. Reason: %s\n", main_block_path.c_str(), strerror(errno));
		main_block->close_file();

		delete index_handler;
		delete main_block;
		exit(-3);
	}
	buffer[meta.get_size()] = '\0';
	printf("read: %s\n", buffer);
	main_block->close_file();

	delete main_block;
  	delete index_handler;
	return 0;
}