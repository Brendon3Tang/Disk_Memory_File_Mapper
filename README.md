# Disk_Memory_File_Mapper
## 一、块初始化 - block_init_test.cpp
1. 设置mmap_option，块大小，哈希桶大小
2. 生成索引文件：
    1. 输入block_id并且用此初始化IndexHandler。
    2. 调用IndexHandler的create函数，创建索引文件。
3. 生成主块文件：
    1. 用由“./block_id"组成的path，模式选择O_RDWR | O_LARGEFILE | O_CREAT，调用FileOperation初始化主块文件指针，main_block。
    2. 通过指针调用fTruncate_file(main_block_size)，调整大小。（如果没open会先自动调用open函数打开/创建文件）
    3. 调用close()将main_block文件关闭

## 二、块的读取 - block_read_test.cpp
1. 设置mmap_option，块大小，哈希桶大小
2. 加载索引文件：
   1. 输入block_id并且用此初始化IndexHandler。
   2. 调用IndexHandler的load函数，加载索引文件。
3. 读取MetaInfo的信息，定位块文件的位置：
   1. 通过file_id和index_handler中的read_segment_meta(file_id, meta)函数，将MetaInfo的信息写入参数meta中。
4. 创建FileOperation的指针，main_block。并通过pRead_file(buffer, meta.get_size(), meta.get_offset)函数将主块文件的内容读取到参数buffer中。最后在buffer的末尾加入'\0'作为结束符。
5. 最后关闭main_block指向的文件，删掉main_block指针与index_handler指针。

## 三、块的写入 - block_write_test.cpp
1. 设置mmap_option，块大小，哈希桶大小
2. 加载索引文件：
   1. 输入block_id并且用此初始化IndexHandler。
   2. 调用IndexHandler的load函数，加载索引文件。
3. 写入文件内容：
   1. 通过index_handler的get_data_file_offset()找到主块文件中的未使用数据的起始偏移：data_file_offset
   2. 通过index_handler->block_info()->seq_no_找到下一个可分配的主块文件的编号：file_no
   3. 分配一块buffer记录主块文件的信息。
   4. 然后将这个buffer的内容通过FileOperation的函数pWrite_file(buffer, sizeof(buffer), data_file_offset)，以data_file_offset为偏移量，写入主块文件。
4. 更新索引文件的头部信息（Index Header）（即块信息（block_info），未使用数据的起始偏移量，索引文件当前偏移量，可重用结点链表等），然后在索引文件中写入MetaInfo
   1. 通过write_segment_meta(meta_info.get_key(), meta_info)写入信息到MetaInfo中
   2. index_handler->commit_block_data_offset(sizeof(buffer))更新索引文件头部信息中的block_data_offset，表示块文件的最新偏移量。
   3. index_handler->update_block_info(largefile::C_OPER_INSERT, sizeof(buffer))更新索引文件头部信息中的块文件信息
5. 用index_handler调用flush把当前内存中的更改保存在磁盘中

## 四、块的删除 - block_delete_test.cpp
1. 设置mmap_option，块大小，哈希桶大小
2. 加载索引文件：
   1. 输入block_id并且用此初始化IndexHandler。
   2. 调用IndexHandler的load函数，加载索引文件。
3. 删除指定的meta_info，通过file_id确定meta_info，然后调用delete_segment_meta(file_id)删除meta_info。
   1. delete_segment_meta会把删除的节点加入可重用节点列表
4. 通过index_handler->flush()将更改保存到磁盘。

## FileOperation
- FileOperation可以直接从磁盘打开文件，并进行读取/写入等操作(但还是会经过内存，即磁盘->内核缓存->用户进程)，但是只要程序运行期间不崩溃，那么所有的read、write操作都会最终保存到磁盘中去。该类是MMapFileOperation的基类。
- 可以打开文件、关闭文件、读写文件、把文件立刻写入磁盘保存、缩小/截断文件、跳跃式的读取文件


## 架构
![架构图](img/arch.jpg)
- IndexHandle通过MMapFileOperation来操控映射到内存中的索引文件。
- MMapFileOperation继承自FileOperation，同时配合MMap_File这个对象来操作映射到内存中的数据
  - 通过MMap_File可以在构造函数中将文件映射到内存。然后在别的函数中（pRead，pWrite，flush）可知道文件是否已经映射到内存，以此判断pWrite，pRead，flush_file是该使用MMapFileOperation的函数还是基类FileOperation的函数版本。
  - 通过FileOperation可以打开文件、关闭文件、读写文件（提供内存映射版本的读写）、把文件立刻写入磁盘保存、缩小/截断文件、跳跃式的读取文件


## 其他
1. common.h中
2. meta_info中的file_id对应块文件中存储的文件的编号。block_info中的block_id对应的是该块文件的id
3. meta_info的更改是通过接口：
   1. write_segment_meta(meta_info.get_key(), meta_info)
   2. delete_segment_meta(file_id)