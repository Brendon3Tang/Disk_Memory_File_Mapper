#include "file_op.h"

using namespace std;
using namespace qiniu;

int main(void) {
	const char* fileName = "file_op.txt";

	largefile::FileOperation* fileOP = new largefile::FileOperation(fileName, O_CREAT | O_RDWR | O_LARGEFILE);

	int fd = fileOP->open_file();
	if (fd < 0) {
		fprintf(stderr, "open file %s failed, reason: %s\n", fileName, strerror(-fd));
		exit(-1);
	}
	else
		printf("open file successfully\n");

	char buffer[65];
	memset(buffer, '8', 64);
	int ret = fileOP->pWrite_file(buffer, 64, 128);
	if (ret < 0) {
		if (ret == largefile::EXIT_DISK_OPER_INCOMPLETE)
			fprintf(stderr, "write file: read length is less than required.");
		else
			fprintf(stderr, "pwrite file %s failed. Reason: %s\n", fileName, strerror(-ret));
	}

	memset(buffer, 0, 64);
	ret = fileOP->pRead_file(buffer, 64, 128);
	if (ret < 0) {
		if (ret == largefile::EXIT_DISK_OPER_INCOMPLETE)
			fprintf(stderr, "pRead file: read length is less than required.");
		else
		fprintf(stderr, "pread file %s failed, reason: %s\n", fileName, strerror(-ret));
	}
	else {
		buffer[64] = '\0';
		printf("read: %s\n", buffer);
	}

	memset(buffer, '2', 64);
	ret = fileOP->write_file(buffer, 64);
	if (ret < 0) {
		if (ret == largefile::EXIT_DISK_OPER_INCOMPLETE)
			fprintf(stderr, "pwrite file: read length is less than required.");
		else
		fprintf(stderr, "pwrite file %s failed. Reason: %s\n", fileName, strerror(-ret));
	}
	fileOP->close_file();
}