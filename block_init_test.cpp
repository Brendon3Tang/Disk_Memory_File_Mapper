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

	//1.创建索引文件
	largefile::IndexHandler* index_handler = new largefile::IndexHandler(".", block_id);
	if (debug)	printf("init index ..\n");

	ret = index_handler->create(block_id, bucket_size, mmap_option);
	if (ret != largefile::TFS_SUCCESS) {
		fprintf(stderr, "create index: %d failed\n", block_id);
		delete index_handler;
		exit(-2);
	}

	//2.生成主块文件。
	stringstream tmp_stream;
	tmp_stream << "." << largefile::MAINBLOCK_DIR_PREFIX << block_id;
	tmp_stream >> main_block_path;
	largefile::FileOperation* main_block = new largefile::FileOperation(main_block_path, O_RDWR | O_LARGEFILE | O_CREAT);

	ret = main_block->fTruncate_file(main_block_size);
	if (ret != largefile::TFS_SUCCESS) {
		fprintf(stderr, "create main block %s failed, reason: %s\n", main_block_path.c_str(), strerror(errno));
		delete main_block;
		index_handler->remove(block_id);
		exit(-3);
	}

	main_block->close_file();
	index_handler->flush();

	delete main_block;
	delete index_handler;
	return 0;
}