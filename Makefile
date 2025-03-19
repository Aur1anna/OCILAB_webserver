# 编译器选项
CC = g++
CFLAGS = -Wall -O2 -I/usr/include/mysql -I./MemoryPool -I./http -I./mysql_connect -I./ThreadPool -I./Timer -pthread -std=c++20
LDFLAGS = -lmysqlclient -lcrypto -pthread
TARGET = server

# 源文件列表（添加 MemoryPool 模块的 .cpp 文件）
SRCS = http/http_conn.cpp main.cpp mysql_connect/mysql_conn.cpp http/auth_http.cpp Timer/lst_timer.cpp \
        MemoryPool/CentralCache.cpp MemoryPool/PageCache.cpp MemoryPool/ThreadCache.cpp

# 目标文件列表（自动将 .cpp 替换为 .o）
OBJS = $(SRCS:.cpp=.o)

# 默认目标
all: $(TARGET)

# 生成可执行文件（链接所有 .o）
$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# 模式规则：编译 .cpp 为 .o（自动处理子目录）
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# 清理所有 .o 文件（保留可执行文件）
clean_objs:
	rm -f $(OBJS)

# 清理生成文件（包括子目录中的 .o）
clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean