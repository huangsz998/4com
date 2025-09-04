# 点云数据采集与处理系统

## 项目概述

本项目是一个基于V4L2的点云数据采集与处理系统，能够从激光雷达设备获取原始数据并解析为结构化的点云数据。系统支持两种数据输出格式：传统的vector容器和新的MultiPointCloudData结构体。

## 系统架构

### 核心组件

1. **GetData类** - 主要的数据采集和处理类
2. **PointCloudData结构体** - 单个点云数据块的结构定义
3. **MultiPointCloudData结构体** - 包含200个点云数据块的复合结构体
4. **V4L2接口** - 用于与视频设备通信

### 数据结构

#### PointCloudData结构体
```cpp
struct PointCloudData {
    Header header;                    // 数据包头信息
    PixelData pixelData[192];        // 192个像素的数据
    End end;                         // 数据包尾信息
};
```

#### MultiPointCloudData结构体
```cpp
struct MultiPointCloudData {
    struct PointCloudData pointCloudData[200];  // 200个点云数据块
};
```

## 功能特性

### 1. 数据采集
- 支持V4L2视频设备接口
- 自动缓冲区管理
- 实时数据流处理

### 2. 数据解析
- 支持EBD和active_area数据解析
- 每个数据块包含192个像素点
- 每个像素支持3个回波数据
- 自动计算距离、角度等参数

### 3. 输出格式
- **传统方式**: 使用vector<PointCloudData*>返回动态数组
- **新方式**: 使用MultiPointCloudData结构体返回固定大小数组

## 使用方法

### 基本使用流程

```cpp
#include "com.h"

int main() {
    // 1. 创建GetData对象
    GetData gd("/dev/video2");
    
    // 2. 初始化设备
    gd.Init();
    
    // 3. 获取点云数据
    struct MultiPointCloudData multiPcdData;
    gd.GetMultiPointCloudDataStruct(multiPcdData);
    
    // 4. 处理数据
    for (int i = 0; i < 200; i++) {
        // 访问第i个数据块
        PointCloudData& block = multiPcdData.pointCloudData[i];
        
        // 处理每个像素
        for (int j = 0; j < 192; j++) {
            PixelData& pixel = block.pixelData[j];
            
            // 访问回波数据
            if (pixel.echo1.peakValue > 0) {
                // 处理第一个回波
                float distance = pixel.echo1.dist / 100.0f;  // 距离(米)
                float angle = pixel.vAngle / 100.0f;         // 角度(度)
            }
        }
    }
    
    // 5. 清理资源
    gd.Uninit();
    
    return 0;
}
```

### API接口说明

#### GetData类主要方法

1. **构造函数**
   ```cpp
   GetData(std::string dev_node);
   ```
   - 参数: `dev_node` - 设备节点路径，如"/dev/video2"

2. **初始化方法**
   ```cpp
   void Init();
   ```
   - 功能: 初始化V4L2设备，设置视频格式，分配缓冲区

3. **获取MultiPointCloudData数据**
   ```cpp
   void GetMultiPointCloudDataStruct(struct MultiPointCloudData &multiPcdData);
   ```
   - 参数: `multiPcdData` - 输出参数，用于存储解析后的点云数据
   - 功能: 从设备获取一帧数据并解析为MultiPointCloudData结构体

4. **获取传统vector数据**
   ```cpp
   void GetMultiPointCloudData(std::vector<struct PointCloudData*> &tPcdVector);
   ```
   - 参数: `tPcdVector` - 输出参数，用于存储解析后的点云数据指针
   - 功能: 从设备获取一帧数据并解析为vector容器

5. **清理方法**
   ```cpp
   void Uninit();
   ```
   - 功能: 停止数据流，释放资源

## 编译和运行

### 编译步骤

```bash
# 编译动态库和所有可执行文件
make

# 或者分别编译
make libcom.so    # 编译动态库
make main         # 编译主程序
make test_multi   # 编译测试程序
```

### 运行程序

```bash
# 运行主程序（连续获取数据）
./main

# 运行测试程序（获取一次数据并显示信息）
./test_multi
```

### 清理编译文件

```bash
make clean
```

## 数据格式说明

### 设备参数
- 设备分辨率: 1536 × 1400
- 帧数据大小: 3072 × 1400 字节
- 数据块数量: 200个
- 每块行数: 7行
- 每块像素数: 192个

### 数据解析流程
1. 从V4L2设备获取原始帧数据
2. 将帧数据按块分割（每块7行）
3. 解析每块的EBD数据和active_area数据
4. 生成192个像素的点云数据
5. 组装为完整的MultiPointCloudData结构体

## 注意事项

1. **设备权限**: 确保程序有访问视频设备的权限
2. **内存管理**: MultiPointCloudData结构体在栈上分配，无需手动释放
3. **数据同步**: 每次调用GetMultiPointCloudDataStruct都会获取最新的帧数据
4. **错误处理**: 程序包含基本的错误检查，建议在生产环境中增加更完善的错误处理

## 性能优化建议

1. **内存预分配**: 系统已预分配38400个点云对象，避免频繁内存分配
2. **数据复用**: 使用MultiPointCloudData结构体可以减少内存拷贝
3. **批量处理**: 一次获取200个数据块，提高处理效率

## 故障排除

### 常见问题

1. **设备打开失败**
   - 检查设备路径是否正确
   - 确认设备权限
   - 验证设备是否被其他程序占用

2. **数据解析错误**
   - 检查设备数据格式是否匹配
   - 验证EBD和active_area数据完整性

3. **编译错误**
   - 确认所有头文件路径正确
   - 检查V4L2开发库是否安装

## 更新日志

### v2.0 (当前版本)
- 新增MultiPointCloudData结构体支持
- 优化内存使用，减少动态分配
- 改进数据访问接口
- 完善文档和使用说明

### v1.0
- 基础V4L2数据采集功能
- 点云数据解析
- vector容器数据输出
