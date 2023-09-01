#include "mmap_file_op.h"
#include <iostream>

using namespace std;
using namespace qiniu;
const static largefile::MMapOption mmap_option = { 10240000, 4096, 4096 };    //内存映射参数

int main(void) {
	const char* fileName = "mmap_file_op.txt";
	largefile::MMapFileOperation* mmfo = new largefile::MMapFileOperation(fileName);

	int fd = mmfo->open_file();
	if (fd < 0) {
		fprintf(stderr, "open file %s failed, reason: %s\n", fileName, strerror(-fd));
		exit(-1);
	}

	int ret = mmfo->mmap_file(mmap_option);	//进行映射
	if (ret == largefile::TFS_ERROR) {
		fprintf(stderr, "mmap_file failed, reason: %s\n", strerror(errno));
		mmfo->close_file();
		exit(-2);
	}

	char buffer[129];
	memset(buffer, '6', 128);
	ret = mmfo->pWrite_file(buffer, 128, 8);
	if (ret < 0) {
		if (ret == largefile::EXIT_DISK_OPER_INCOMPLETE)
			fprintf(stderr, "pwrite file: read length is less than required.");
		else
			fprintf(stderr, "pwrite file %s failed. Reason: %s\n", fileName, strerror(-ret));
	}

	memset(buffer, 0, 128);
	ret = mmfo->pRead_file(buffer, 128, 8);
	if (ret < 0) {
		if (ret == largefile::EXIT_DISK_OPER_INCOMPLETE)
			fprintf(stderr, "pRead file: read length is less than required.");
		else
			fprintf(stderr, "pread file %s failed, reason: %s\n", fileName, strerror(-ret));
	}
	else {
		buffer[128] = '\0';
		printf("read: %s\n", buffer);
	}

	ret = mmfo->flush_file();
	if (ret == largefile::TFS_ERROR) {
		fprintf(stderr, "flush file failed, reason：%s\n", strerror(errno));
	}

	memset(buffer, '8', 128);
	ret = mmfo->pWrite_file(buffer, 128, 4096);
	if (ret < 0) {
		if (ret == largefile::EXIT_DISK_OPER_INCOMPLETE)
			fprintf(stderr, "pwrite file: read length is less than required.");
		else
			fprintf(stderr, "pwrite file %s failed. Reason: %s\n", fileName, strerror(-ret));
	}

	mmfo->munmap_file();
	mmfo->close_file();
}
