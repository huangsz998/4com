#include "com.h"

#pragma pack(push, 1)

// 设备分辨率定义
#define WIDTH 1536
#define HEIGHT 1400

#define ECHO_ELN 20 //每个回波长度20字节
#define HEAD_LEN 20 //包头长度20字节
#define TAIL_LEN 23 //包尾长度23字节
#define CRC_LEN 4   //crc长度4字节
#define ECHO_NUM 3  //每个像素回波个数

// 数据格式定义
#define FRAME_WIDTH 3072        // 单行3072字节
#define FRAME_HEIGHT 1400       // 共1400行
#define BLOCK_ROWS 7            // 每个数据块7行
#define BLOCK_COUNT 200         // 共200个数据块

#define EBD_SIZE 2952           // EBD数据长度2952字节
#define ACTIVE_AREA_SIZE 2952*6  //第2至第7行的前2952字节为点云数据存放区域



GetData::GetData(std::string dev_node) {
    g_dev_no = dev_node;
    
    // 分配内存
    ebd = new char[EBD_SIZE];//EBD数据长度2952字节
    frame_data = new char[FRAME_WIDTH * FRAME_HEIGHT];  // 3072×1400字节
    
    //初始化点云数据数组
    initializePointCloudArray();
}

GetData::~GetData() {
    // 清理点云数据数组
    cleanupPointCloudArray();
    
    delete[] ebd;
    delete[] active_area;
    delete[] frame_data;
}

// 直接初始化38400个点云数据对象
void GetData::initializePointCloudArray() {
    for (int i = 0; i < MAX_PCD_COUNT; ++i) {
        point_cloud_array[i] = new PointCloudData();
        if (point_cloud_array[i]) {
            memset(point_cloud_array[i], 0, sizeof(PointCloudData));
        }
    }
}
  // 清理38400个点云数据对象
void GetData::cleanupPointCloudArray() {
    for (int i = 0; i < MAX_PCD_COUNT; ++i) {
        if (point_cloud_array[i]) {
            delete point_cloud_array[i];
            point_cloud_array[i] = nullptr;
        }
    }
}

// 获取多个点云数据的方法
void GetData::GetMultiPointCloudData(std::vector<struct PointCloudData*> &tPcdVector) {
    // 每个数据块生成192个点云数据，共38400个
    tPcdVector.reserve(BLOCK_COUNT * 192);
    tPcdVector.clear();
    
    // 从队列中取出缓冲区
    memset(&g_buf, 0, sizeof(g_buf));
    g_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    g_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(g_fd, VIDIOC_DQBUF, &g_buf) == -1) {
        std::cerr << "无法从队列中取出缓冲区" << std::endl;
        return;
    }
    
    // 获取帧数据
    char* frame = (char*)g_buffers[g_buf.index];
    
    // 复制完整的帧数据
    memcpy(frame_data, frame, FRAME_WIDTH * FRAME_HEIGHT);
    
    // 边解析边生成点云数据
    for (int block = 0; block < BLOCK_COUNT; block++) {
        ParseSingleDataBlock(block, tPcdVector);
    }
    
    // 将缓冲区重新入队
    if (ioctl(g_fd, VIDIOC_QBUF, &g_buf) == -1) {
        std::cerr << "无法将缓冲区重新入队" << std::endl;
    }
}

// 解析单个数据块并生成点云数据
void GetData::ParseSingleDataBlock(int block_index, std::vector<struct PointCloudData*> &tPcdVector) {
    // 计算当前数据块在帧中的起始位置
    int block_start_row = block_index * BLOCK_ROWS;
    
    // 第1行：EBD数据（2952字节）+ 0填充（120字节）
    int ebd_offset = block_start_row * FRAME_WIDTH;
    const char* ebd_data = &frame_data[ebd_offset];
    
    // 将第2至第7行每行前2952字节拼接为一个大的active_area区域（ACTIVE_AREA_SIZE = 2952*6）
    char combined_active_area[ACTIVE_AREA_SIZE];
    int write_offset = 0;
    const int ROW_ACTIVE_BYTES = EBD_SIZE; // 每行有效active_area数据
    for (int row = 1; row < BLOCK_ROWS; row++) {
        int row_offset = (block_start_row + row) * FRAME_WIDTH;
        memcpy(&combined_active_area[write_offset], &frame_data[row_offset], ROW_ACTIVE_BYTES);
        write_offset += ROW_ACTIVE_BYTES;
    }

    // 使用一个大的active_area区域与一个ebd共同逐像素生成点云数据（192个像素）
    GeneratePointCloudFromBlockData(ebd_data, combined_active_area, block_index, tPcdVector);
}

// 从EBD和active_area数据直接生成点云数据
void GetData::GeneratePointCloudFromBlockData(const char* ebd_data, const char* active_area_data, int block_index, std::vector<struct PointCloudData*> &tPcdVector) {
    // 从拼接后的大区域中逐像素生成点云对象
    static int array_index = 0;
    for (unsigned int i = 0; i < ACTIVE_AREA_SIZE; ) {
        struct PointCloudData *newPcd = point_cloud_array[array_index % MAX_PCD_COUNT];
        array_index++;

        Header head;
        head.ee = 0xEE;
        head.ff = 0xFF;
        head.protocolVersionMajor = 0x01;
        head.protocolVersionMinor = 0x01;
        head.operationMode = 0x00;
        head.channelNum = 192;
        head.disUnit = 0;
        head.returnNum = 0x03;
        head.sceneMode = 0xAA;
        head.slotNum = (ebd_data[11] << 8) + ebd_data[9];
        head.frameNum = ebd_data[41];
        float fth = ((float)head.slotNum * (float)0.4) - float(60);
        head.hAngle = fth * 100;
        head.rangeEcho = 0x03;
        newPcd->header = head;

        CommonInfo com_info;
        // 解析
        com_info.pixel_num = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.echo_detection_threshold = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.echo_detection_threshold_1 = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.echo_detection_threshold_2 = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.bn = ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.sum_bi = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.refernce_light_maximum_signal_intensity = ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.refernce_light_maximum_position = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.far_range_maximum_signal_intensity = ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.max_bi = ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;

        for (int j = 0; j < ECHO_NUM; j++) {
            unsigned short peak_signal_intensity = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short peak_position = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short hv_width_start_position_i = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short hv_width_start_position_f = (active_area_data[i] >> 4); i += 2;
            unsigned short hv_width_end_position_i = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short hv_width_end_position_f = (active_area_data[i] >> 4); i += 2;
            unsigned short echo_start_position = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short echo_end_position = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;

            unsigned short valley_minimum_signal_strength = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short far_peak_position = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;

            unsigned short peak_min_signal = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short flag = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;

            double dc = 0.15;
            Echo echo;
            echo.width = echo_end_position - echo_start_position;
            echo.echoStart = echo_start_position;
            echo.echoEnd = echo_end_position;
            echo.peakValue = peak_signal_intensity;
            echo.area = 0;
            echo.peakPostion = peak_position;
            echo.basicRisingEdge = 0;
            echo.halfValueWidthStart = hv_width_start_position_i + (hv_width_start_position_f << 12);
            echo.halfValueWidthEnd = hv_width_end_position_i + (hv_width_end_position_f << 12);
            echo.binLen = 0;
            double dPostion = 0.0f;
            dPostion = (double)hv_width_start_position_f*(double)0.16f;
            dPostion=(double)hv_width_start_position_i+dPostion;
            double fd = (double)dPostion*dc;
            echo.dist = fd * 100;

            echo.splitAndTailPeakBreak = 0;
            echo.leadingPeakBreakAndInferferenceFlag = 0;
            echo.obiectFlagAndPeakValueMultOccur = 0;
            echo.exandValue = 0;
            echo.ghostValue = 0;
            echo.fogValue = 0;
            echo.contaminantValue = 0;

            newPcd->pixelData[com_info.pixel_num].vAngle = (float)com_info.pixel_num*0.1f*(float)100;
            if (j == 0) {
                newPcd->pixelData[com_info.pixel_num].echo1 = echo;
            }
            else if (j == 1) {
                newPcd->pixelData[com_info.pixel_num].echo2 = echo;
            }
            else {
                newPcd->pixelData[com_info.pixel_num].echo3 = echo;
            }
        }

        End end;
        memset(&end.reserved1, 0, RESERVED1_LEN);
        memset(&end.reserved2, 0, RESERVED2_LEN);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        long nanoseconds = ts.tv_nsec;
        long microseconds = nanoseconds / 1000; // 转换为微秒
        if (microseconds > 1999999) {
            end.timestamp = 1999999;
        }
        else {
            end.timestamp = (unsigned int)microseconds;
        }
        end.tail = 0x0FF0;
        newPcd->end = end;
        tPcdVector.push_back(newPcd);

        //跳过末尾CRC4字节
        i+=4;
    }
}

// 获取MultiPointCloudData结构体数据的方法
void GetData::GetMultiPointCloudDataStruct(struct MultiPointCloudData &multiPcdData) {
    // 从队列中取出缓冲区
    memset(&g_buf, 0, sizeof(g_buf));
    g_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    g_buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(g_fd, VIDIOC_DQBUF, &g_buf) == -1) {
        std::cerr << "无法从队列中取出缓冲区" << std::endl;
        return;
    }
    
    // 获取帧数据
    char* frame = (char*)g_buffers[g_buf.index];
    
    // 复制完整的帧数据
    memcpy(frame_data, frame, FRAME_WIDTH * FRAME_HEIGHT);
    
    // 边解析边生成点云数据到MultiPointCloudData结构体中
    for (int block = 0; block < BLOCK_COUNT; block++) {
        ParseSingleDataBlockToStruct(block, multiPcdData);
    }
    
    // 将缓冲区重新入队
    if (ioctl(g_fd, VIDIOC_QBUF, &g_buf) == -1) {
        std::cerr << "无法将缓冲区重新入队" << std::endl;
    }
}

// 解析单个数据块并生成点云数据到MultiPointCloudData结构体中
void GetData::ParseSingleDataBlockToStruct(int block_index, struct MultiPointCloudData &multiPcdData) {
    // 计算当前数据块在帧中的起始位置
    int block_start_row = block_index * BLOCK_ROWS;
    
    // 第1行：EBD数据（2952字节）+ 0填充（120字节）
    int ebd_offset = block_start_row * FRAME_WIDTH;
    const char* ebd_data = &frame_data[ebd_offset];
    
    // 将第2至第7行每行前2952字节拼接为一个大的active_area区域（ACTIVE_AREA_SIZE = 2952*6）
    char combined_active_area[ACTIVE_AREA_SIZE];
    int write_offset = 0;
    const int ROW_ACTIVE_BYTES = EBD_SIZE; // 每行有效active_area数据
    for (int row = 1; row < BLOCK_ROWS; row++) {
        int row_offset = (block_start_row + row) * FRAME_WIDTH;
        memcpy(&combined_active_area[write_offset], &frame_data[row_offset], ROW_ACTIVE_BYTES);
        write_offset += ROW_ACTIVE_BYTES;
    }

    // 使用一个大的active_area区域与一个ebd共同逐像素生成点云数据（192个像素）
    GeneratePointCloudFromBlockDataToStruct(ebd_data, combined_active_area, block_index, multiPcdData);
}

// 从EBD和active_area数据直接生成点云数据到MultiPointCloudData结构体中
void GetData::GeneratePointCloudFromBlockDataToStruct(const char* ebd_data, const char* active_area_data, int block_index, struct MultiPointCloudData &multiPcdData) {
    // 使用block_index作为数组索引，每个block对应一个PointCloudData
    struct PointCloudData *newPcd = &multiPcdData.pointCloudData[block_index];
    
    // 初始化PointCloudData的header（只需要设置一次）
    Header head;
    head.ee = 0xEE;
    head.ff = 0xFF;
    head.protocolVersionMajor = 0x01;
    head.protocolVersionMinor = 0x01;
    head.operationMode = 0x00;
    head.channelNum = 192;
    head.disUnit = 0;
    head.returnNum = 0x03;
    head.sceneMode = 0xAA;
    head.slotNum = (ebd_data[11] << 8) + ebd_data[9];
    head.frameNum = ebd_data[41];
    float fth = ((float)head.slotNum * (float)0.4) - float(60);
    head.hAngle = fth * 100;
    head.rangeEcho = 0x03;
    newPcd->header = head;
    
    // 从拼接后的大区域中逐像素生成点云对象到MultiPointCloudData结构体中
    for (unsigned int i = 0; i < ACTIVE_AREA_SIZE; ) {

        CommonInfo com_info;
        // 解析
        com_info.pixel_num = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.echo_detection_threshold = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.echo_detection_threshold_1 = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.echo_detection_threshold_2 = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.bn = ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.sum_bi = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.refernce_light_maximum_signal_intensity = ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.refernce_light_maximum_position = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.far_range_maximum_signal_intensity = ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
        com_info.max_bi = ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;

        for (int j = 0; j < ECHO_NUM; j++) {
            unsigned short peak_signal_intensity = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short peak_position = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short hv_width_start_position_i = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short hv_width_start_position_f = (active_area_data[i] >> 4); i += 2;
            unsigned short hv_width_end_position_i = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short hv_width_end_position_f = (active_area_data[i] >> 4); i += 2;
            unsigned short echo_start_position = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short echo_end_position = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;

            unsigned short valley_minimum_signal_strength = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short far_peak_position = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;

            unsigned short peak_min_signal = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;
            unsigned short flag = ((active_area_data[i + 1] >> 4) * 256) + ((active_area_data[i + 1] & 0x0F) * 16) + (active_area_data[i] >> 4); i += 2;

            double dc = 0.15;
            Echo echo;
            echo.width = echo_end_position - echo_start_position;
            echo.echoStart = echo_start_position;
            echo.echoEnd = echo_end_position;
            echo.peakValue = peak_signal_intensity;
            echo.area = 0;
            echo.peakPostion = peak_position;
            echo.basicRisingEdge = 0;
            echo.halfValueWidthStart = hv_width_start_position_i + (hv_width_start_position_f << 12);
            echo.halfValueWidthEnd = hv_width_end_position_i + (hv_width_end_position_f << 12);
            echo.binLen = 0;
            double dPostion = 0.0f;
            dPostion = (double)hv_width_start_position_f*(double)0.16f;
            dPostion=(double)hv_width_start_position_i+dPostion;
            double fd = (double)dPostion*dc;
            echo.dist = fd * 100;

            echo.splitAndTailPeakBreak = 0;
            echo.leadingPeakBreakAndInferferenceFlag = 0;
            echo.obiectFlagAndPeakValueMultOccur = 0;
            echo.exandValue = 0;
            echo.ghostValue = 0;
            echo.fogValue = 0;
            echo.contaminantValue = 0;

            newPcd->pixelData[com_info.pixel_num].vAngle = (float)com_info.pixel_num*0.1f*(float)100;
            if (j == 0) {
                newPcd->pixelData[com_info.pixel_num].echo1 = echo;
            }
            else if (j == 1) {
                newPcd->pixelData[com_info.pixel_num].echo2 = echo;
            }
            else {
                newPcd->pixelData[com_info.pixel_num].echo3 = echo;
            }
        }

        //跳过末尾CRC4字节
        i+=4;
    }
    
    // 设置PointCloudData的end（只需要设置一次）
    End end;
    memset(&end.reserved1, 0, RESERVED1_LEN);
    memset(&end.reserved2, 0, RESERVED2_LEN);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long nanoseconds = ts.tv_nsec;
    long microseconds = nanoseconds / 1000; // 转换为微秒
    if (microseconds > 1999999) {
        end.timestamp = 1999999;
    }
    else {
        end.timestamp = (unsigned int)microseconds;
    }
    end.tail = 0x0FF0;
    newPcd->end = end;
}

void GetData::Init()
{
    g_fd = open(g_dev_no.c_str(), O_RDWR);
    if (g_fd == -1) {
        std::cerr << "无法打开摄像头设备" << g_dev_no << std::endl;
        return;
    }

    // 查询设备能力
    struct v4l2_capability cap;
    if (ioctl(g_fd, VIDIOC_QUERYCAP, &cap) == -1) {
        std::cerr << "无法查询设备能力" << std::endl;
        close(g_fd);
        return;
    }

    // 检查设备是否支持视频捕获
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        std::cerr << "设备不支持视频捕获" << std::endl;
        close(g_fd);
        return;
    }

    // 设置视频格式
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    // fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB12;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    if (ioctl(g_fd, VIDIOC_S_FMT, &fmt) == -1) {
        std::cerr << "无法设置视频格式" << std::endl;
        close(g_fd);
        return;
    }

    // 请求缓冲区
    memset(&g_req, 0, sizeof(g_req));
    g_req.count = 4;
    g_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    g_req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(g_fd, VIDIOC_REQBUFS, &g_req) == -1) {
        std::cerr << "无法请求缓冲区" << std::endl;
        close(g_fd);
        return;
    }

    // 映射缓冲区
    g_buffers = new void*[g_req.count];
    for (__u32 i = 0; i < g_req.count; ++i) {
        memset(&g_buf, 0, sizeof(g_buf));
        g_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        g_buf.memory = V4L2_MEMORY_MMAP;
        g_buf.index = i;
        if (ioctl(g_fd, VIDIOC_QUERYBUF, &g_buf) == -1) {
            std::cerr << "无法查询缓冲区" << std::endl;
            close(g_fd);
            return;
        }
        g_buffers[i] = mmap(NULL, g_buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, g_buf.m.offset);
        if (g_buffers[i] == MAP_FAILED) {
            std::cerr << "无法映射缓冲区" << std::endl;
            close(g_fd);
            return;
        }
    }

    // 入队缓冲区
    for (__u32 i = 0; i < g_req.count; ++i) {
        memset(&g_buf, 0, sizeof(g_buf));
        g_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        g_buf.memory = V4L2_MEMORY_MMAP;
        g_buf.index = i;
        if (ioctl(g_fd, VIDIOC_QBUF, &g_buf) == -1) {
            std::cerr << "无法将缓冲区入队" << std::endl;
            close(g_fd);
            return;
        }
    }

    // 开始捕获数据
    g_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(g_fd, VIDIOC_STREAMON, &g_type) == -1) {
        std::cerr << "无法开始视频流" << std::endl;
        close(g_fd);
        return;
    }
}

void GetData::Uninit()
{
    // 停止捕获数据
    if (ioctl(g_fd, VIDIOC_STREAMOFF, &g_type) == -1) {
        std::cerr << "无法停止视频流" << std::endl;
        close(g_fd);
        return;
    }

    // 解除映射并关闭设备
    for (__u32 i = 0; i < g_req.count; ++i) {
        if (munmap(g_buffers[i], g_buf.length) == -1) {
            std::cerr << "无法解除映射" << std::endl;
            close(g_fd);
            return;
        }
    }
    delete[] g_buffers;

    close(g_fd);
}
