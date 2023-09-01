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

	//2. 删除指定文件meta info
	uint64_t file_id = 0;
	cout << "Type your file ID: " << endl;
	cin >> file_id;

	if (file_id < 1) {
		cerr << "Invalid file id" << endl;
		exit(-2);
	}

	ret = index_handler->delete_segment_meta(file_id);
	if (ret != largefile::TFS_SUCCESS) {
		fprintf(stderr, "delete index failed, file id: %lu, ret: %d\n", file_id, ret);
	}

	ret = index_handler->flush();
	if (ret != largefile::TFS_SUCCESS) {
		fprintf(stderr, "flushed failed, main block id: %d, file no: %lu\n", block_id, file_id);
		exit(-3);
	}
	printf("delete successfully\n");
	delete index_handler;

	return 0;
}