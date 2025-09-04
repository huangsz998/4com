#pragma pack(push, 1)

#define RESERVED1_LEN 6 //包尾预留1长度
#define RESERVED2_LEN 11 //包尾预留2长度
#define SLOT_PIXEL_NUM 192 //每个slot包含像素个数
#define ECHO_HIST_LEN 120
#define FRAME_DATA_LEN 600

struct Echo {
    unsigned char width;
    unsigned short echoStart;
    unsigned short echoEnd;
    unsigned short peakValue;
    unsigned short area;
    unsigned short peakPostion;
    unsigned short basicRisingEdge;
    unsigned short halfValueWidthStart;
    unsigned short halfValueWidthEnd;
    unsigned char binLen;
    unsigned short dist;
    unsigned char splitAndTailPeakBreak;
    // unsigned char tailPeakBreak;
    unsigned char leadingPeakBreakAndInferferenceFlag;
    // unsigned char inferferenceFlag;
    unsigned char obiectFlagAndPeakValueMultOccur;
    // unsigned char peakValueMultOccur;
    unsigned char exandValue;
    unsigned char ghostValue;
    unsigned char fogValue;
    unsigned char contaminantValue;
};

// 通道数据结构体
struct PixelData {
	Echo echo1;
	Echo echo2;
	Echo echo3;
	unsigned char echoHist[ECHO_HIST_LEN];
	unsigned short vAngle;         // 需要除以100
	unsigned char hCalib;          // 需要除以100
	unsigned char vCalib;          // 需要除以100
};

struct Header {
    unsigned char ee;
    unsigned char ff;
    unsigned char protocolVersionMajor;
    unsigned char protocolVersionMinor;
    unsigned char operationMode;
    unsigned char channelNum;
    unsigned char disUnit;
    unsigned char returnNum;
    unsigned char sceneMode;
    unsigned short slotNum;
    unsigned int frameNum;
    int hAngle;
    unsigned char rangeEcho;
};

struct End {
    unsigned char reserved1[RESERVED1_LEN];
    unsigned char reserved2[RESERVED2_LEN];
    unsigned int timestamp;
    unsigned short tail;
};

// 点云数据结构体
struct PointCloudData {
	Header header;
	PixelData pixelData[SLOT_PIXEL_NUM];
	End end;
};

struct MultiPointCloudData {
  struct PointCloudData pointCloudData[200];
};

struct hzgd_EchoData {
	Echo echo1;
	Echo echo2;
	Echo echo3;
	unsigned char echoHist[ECHO_HIST_LEN];
	unsigned short vAngle;         // 需要除以100
	unsigned short hAngle;         // 需要除以100
	unsigned char hCalib;          // 需要除以100
	unsigned char vCalib;          // 需要除以100
	float fx;
	float fy;
	float fz;
	unsigned int line;
	unsigned int col;
};

#pragma pack(pop)