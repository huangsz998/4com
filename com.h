#ifndef COM_H
#define COM_H

#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <filesystem>
#include <mutex>
#include <vector>
#include <atomic>
#include <thread>

#include "pointcloud_def.h"


#define MAX_PCD_COUNT 38400  // 预分配点云对象数量：200块 × 每块192个 = 38400


struct CommonInfo {
    unsigned short pixel_num;
    unsigned short echo_detection_threshold;
    unsigned short echo_detection_threshold_1;
    unsigned short echo_detection_threshold_2;
    unsigned short bn;
    unsigned short sum_bi;
    unsigned short refernce_light_maximum_signal_intensity;
    unsigned short refernce_light_maximum_position;
    unsigned short far_range_maximum_signal_intensity;
    unsigned short max_bi;
};

class GetData
{
    
public:
    GetData(std::string dev_node);
    ~GetData(); 

    void Init();
    void Uninit();
    
    // 获取多个点云数据的方法
    void GetMultiPointCloudData(std::vector<struct PointCloudData*> &tPcdVector);
    
    // 获取MultiPointCloudData结构体数据的方法
    void GetMultiPointCloudDataStruct(struct MultiPointCloudData &multiPcdData);
    
private:
    int g_fd = 0;
    void** g_buffers;
    std::string g_dev_no = "";
    enum v4l2_buf_type g_type;
    struct v4l2_requestbuffers g_req;
    struct v4l2_buffer g_buf;

    char *ebd;
    char *active_area;
    
    // 存储完整的帧数据
    char *frame_data;           
    
    // 预分配38400个点云数据空间（每像素一个对象）
    PointCloudData* point_cloud_array[MAX_PCD_COUNT]; 
    
    void ParseSingleDataBlock(int block_index, std::vector<struct PointCloudData*> &tPcdVector);
    void GeneratePointCloudFromBlockData(const char* ebd_data, const char* active_area_data, int block_index,std::vector<struct PointCloudData*> &tPcdVector);
    
    // 新增的MultiPointCloudData相关方法
    void ParseSingleDataBlockToStruct(int block_index, struct MultiPointCloudData &multiPcdData);
    void GeneratePointCloudFromBlockDataToStruct(const char* ebd_data, const char* active_area_data, int block_index, struct MultiPointCloudData &multiPcdData);
    
    // 内存分配与清理
    void initializePointCloudArray();
    void cleanupPointCloudArray();
};

#endif