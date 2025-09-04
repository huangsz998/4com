# # 编译器选择（C++文件需用g++）
# CC = g++
# # 编译选项：位置无关代码 + 警告 + 调试信息
# CFLAGS = -fPIC -Wall -g
# # 动态库链接选项
# LDFLAGS = -shared
# # 动态库命名规范（lib前缀 + .so后缀）
# LIBRARY = libcom.so
# # 源文件列表
# SRCS = com.cpp
# # 生成对应的目标文件列表
# OBJS = $(SRCS:.cpp=.o)

# .PHONY: all clean

# all: $(LIBRARY)

# # 动态库生成规则
# $(LIBRARY): $(OBJS)
# 	$(CC) $(LDFLAGS) -o $@ $^

# # 编译.cpp为.o（自动推导规则）
# %.o: %.cpp
# 	$(CC) $(CFLAGS) -c $< -o $@

# # 清理编译产物
# clean:
# 	rm -f $(OBJS) $(LIBRARY)





# 编译器配置
CXX      = g++
CXXFLAGS = -fPIC -Wall -g -I.  # 添加当前目录头文件搜索路径
LDFLAGS  = -L. -Wl,-rpath=.    # 运行时库搜索路径设为当前目录
LDLIBS   = -lcom -lpthread          # 链接的库名称

# 目标文件配置
LIBRARY  = libcom.so
TARGET   = main
TEST_TARGET = test_multi

# 源文件列表
LIB_SRCS = com.cpp
APP_SRCS = main.cpp
TEST_SRCS = test_multi.cpp

# 生成对应的目标文件
LIB_OBJS = $(LIB_SRCS:.cpp=.o)
APP_OBJS = $(APP_SRCS:.cpp=.o)
TEST_OBJS = $(TEST_SRCS:.cpp=.o)

.PHONY: all clean

all: $(LIBRARY) $(TARGET) $(TEST_TARGET)

# 动态库生成规则
$(LIBRARY): $(LIB_OBJS)
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

# 可执行文件生成规则
$(TARGET): $(APP_OBJS) $(LIBRARY)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

# 测试程序生成规则
$(TEST_TARGET): $(TEST_OBJS) $(LIBRARY)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

# 通用编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(LIB_OBJS) $(APP_OBJS) $(TEST_OBJS) $(LIBRARY) $(TARGET) $(TEST_TARGET)