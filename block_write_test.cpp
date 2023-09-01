#include "file_op.h"
#include "index_handle.h"
#include <sstream>

using namespace std;
using namespace qiniu;

const static largefile::MMapOption mmap_option = { 1024000, 4096, 4096 };
const static uint32_t main_block_size = 1024 * 1024 * 64;//�����ļ��Ĵ�С��64MB
const static uint32_t bucket_size = 1000;//��ϣͰ�Ĵ�С

int main(int argc, char** argv) {
	uint32_t block_id;
	string main_block_path;
	string index_path;
	int32_t ret = largefile::TFS_SUCCESS;

	cout << "Type your block id: " << endl;
	cin >> block_id;

	if (block_id < 1) {
		cerr << "��Ч��block id" << endl;
		exit(-1);
	}

	//1.���������ļ�
	largefile::IndexHandler* index_handler = new largefile::IndexHandler(".", block_id);
	if (debug)	printf("Load index ..\n");

	ret = index_handler->load(block_id, bucket_size, mmap_option);
	if (ret != largefile::TFS_SUCCESS) {
		fprintf(stderr, "Load index: %d failed\n", block_id);
		delete index_handler;
		exit(-2);
	}

	//2.����һ���ڴ���Ϊ�����ļ�����д�����ݵ������ļ���
	stringstream tmp_stream;
	tmp_stream << "." << largefile::MAINBLOCK_DIR_PREFIX << block_id;
	tmp_stream >> main_block_path;
	largefile::FileOperation* main_block = new largefile::FileOperation(main_block_path, O_RDWR | O_LARGEFILE | O_CREAT);
	//main_block->open_file();
	char buffer[4096];
	memset(buffer, '6', 4096);

	int32_t data_file_offset = index_handler->get_data_file_offset();
	uint32_t file_no = index_handler->block_info()->seq_no_;

	//������д�뵽�ڴ��е������ļ�
	ret = main_block->pWrite_file(buffer, sizeof(buffer), data_file_offset);
	if (ret != largefile::TFS_SUCCESS) {
		fprintf(stderr, "Write to main block %s failed. Reason: %s\n", main_block_path.c_str(), strerror(errno));
		main_block->close_file();

		delete index_handler;
		delete main_block;
		exit(-3);
	}

	//3.�ɹ�д�������ļ�����������ļ���ͷ����Ϣ�����Ϣ�����������ļ���д��MetaInfo
	//��д��Ҫ���������ļ���meta_info
	largefile::MetaInfo meta_info;
	meta_info.set_file_id(file_no);
	meta_info.set_offset(data_file_offset);
	meta_info.set_size(sizeof(buffer));
	
	//��meta_infoд�������ļ���
	ret = index_handler->write_segment_meta(meta_info.get_key(), meta_info);
	//���д��ɹ��ˣ���ôҪ��������ͷ����Ϣ�����Ϣ
	if (ret == largefile::TFS_SUCCESS) {
		//���������ļ�ͷ����Ϣ�е�block_data_offset����ʾ���ļ�������ƫ����
		index_handler->commit_block_data_offset(sizeof(buffer));
		//���������ļ�ͷ����Ϣ�еĿ��ļ���Ϣ
		index_handler->update_block_info(largefile::C_OPER_INSERT, sizeof(buffer));

		//�������ļ��Ϳ��ļ����浽������
		ret = index_handler->flush();
		if (ret != largefile::TFS_SUCCESS) {
			fprintf(stderr, "flush mainblock %d failed, file no: %u\n", block_id, file_no);
		}
	}
	else {
		fprintf(stderr, "write_segment_meta - mainblock %d failed, file no: %u\n", block_id, file_no);
	}

	if (ret != largefile::TFS_SUCCESS) {
		fprintf(stderr, "write to mainblock %d failed, file no: %u\n", block_id, file_no);
	}
	else {
		if (debug)
			printf("write to mainblock %d successfully, file no: %u\n", block_id, file_no);
	}

	main_block->close_file();
	delete main_block;
	delete index_handler;
	return 0;
}