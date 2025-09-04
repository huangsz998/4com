#include "com.h"
#include <vector>

int main() {
    GetData gd("/dev/video2");
    gd.Init();
    
    std::cout << "开始获取点云数据！" << std::endl;
    
    while (1) {
        // 使用MultiPointCloudData结构体获取点云数据
        struct MultiPointCloudData multiPcdData;
        gd.GetMultiPointCloudDataStruct(multiPcdData);
        
        std::cout << "MultiPointCloudData结构体数据解析完成，共包含 " << 200 << " 个点云数据块" << std::endl;
        
        // 处理每个点云数据块
        for (int i = 0; i < 200; i++) {
            std::cout << "点云数据块 " << i << ": slotNum=" << multiPcdData.pointCloudData[i].header.slotNum 
                      << ", frameNum=" << multiPcdData.pointCloudData[i].header.frameNum 
                      << ", channelNum=" << (int)multiPcdData.pointCloudData[i].header.channelNum << std::endl;
            
            // 可以进一步处理每个像素的数据
            // 例如：访问第i个数据块的第0个像素的回波数据
            if (multiPcdData.pointCloudData[i].pixelData[0].echo1.peakValue > 0) {
                std::cout << "  像素0: echo1距离=" << multiPcdData.pointCloudData[i].pixelData[0].echo1.dist 
                          << ", 峰值=" << multiPcdData.pointCloudData[i].pixelData[0].echo1.peakValue << std::endl;
            }
        }
        
        usleep(100000);
    }

    return 0;
}
 
