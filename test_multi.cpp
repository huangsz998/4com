#include "com.h"
#include <iostream>

int main() {
    std::cout << "=== MultiPointCloudData 结构体测试程序 ===" << std::endl;
    
    // 创建GetData对象
    GetData gd("/dev/video2");
    
    // 初始化设备
    std::cout << "正在初始化设备..." << std::endl;
    gd.Init();
    
    std::cout << "开始获取点云数据！" << std::endl;
    
    // 测试获取一次数据
    struct MultiPointCloudData multiPcdData;
    gd.GetMultiPointCloudDataStruct(multiPcdData);
    
    std::cout << "MultiPointCloudData结构体数据解析完成！" << std::endl;
    std::cout << "结构体大小: " << sizeof(MultiPointCloudData) << " 字节" << std::endl;
    std::cout << "包含数据块数量: 200" << std::endl;
    
    // 显示前几个数据块的基本信息
    for (int i = 0; i < 5; i++) {
        std::cout << "数据块 " << i << ":" << std::endl;
        std::cout << "  slotNum: " << multiPcdData.pointCloudData[i].header.slotNum << std::endl;
        std::cout << "  frameNum: " << multiPcdData.pointCloudData[i].header.frameNum << std::endl;
        std::cout << "  channelNum: " << (int)multiPcdData.pointCloudData[i].header.channelNum << std::endl;
        std::cout << "  hAngle: " << multiPcdData.pointCloudData[i].header.hAngle << std::endl;
        std::cout << "  timestamp: " << multiPcdData.pointCloudData[i].end.timestamp << std::endl;
        
        // 检查第一个像素的回波数据
        if (multiPcdData.pointCloudData[i].pixelData[0].echo1.peakValue > 0) {
            std::cout << "  像素0 echo1: 距离=" << multiPcdData.pointCloudData[i].pixelData[0].echo1.dist 
                      << ", 峰值=" << multiPcdData.pointCloudData[i].pixelData[0].echo1.peakValue << std::endl;
        }
        std::cout << std::endl;
    }
    
    std::cout << "测试完成！" << std::endl;
    
    return 0;
}
