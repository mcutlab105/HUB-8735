//  匯入函式庫
#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "RTSP.h"
#include "NNObjectDetection.h"
#include "VideoStreamOverlay.h"
#include "ObjectClassList.h"
#include <AmebaServo.h>

//  定義巨集
#define CHANNEL   0
#define CHANNELNN 3
#define NNWIDTH  576
#define NNHEIGHT 320

//設置視訊參數
VideoSetting config(VIDEO_FHD, 30, VIDEO_H264, 0);
VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);

//  創建物件
NNObjectDetection ObjDet;
RTSP rtsp;
AmebaServo myservo;
IPAddress ip;

//  處理視訊流的輸入和輸出
StreamIO videoStreamer(1, 1);
//  處理神經網絡模型的輸入和輸出視訊流
StreamIO videoStreamerNN(1, 1);

//  定義網路名稱和密碼
char ssid[] = "iPhone 12";
char pass[] = "ZAQWSXCDE";

//  代表網路的連接狀態
int status = WL_IDLE_STATUS;

//  定義名為rtsp_portnum的整數變數
int rtsp_portnum;

//  定義名為pos的整數變數,並賦予其值為0
int pos = 0;

void setup()
{
    //  定義鮑率
    Serial.begin(115200);

    //  定義伺服馬達的腳位
    myservo.attach(9);

    //  連結到Wi-Fi網路,直到成功為止
    while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);

        // wait 2 seconds for connection:
        delay(2000);
    }
    ip = WiFi.localIP();

    //  配置相機的視頻通道和RTSP相關參數
    config.setBitrate(2 * 1024 * 1024);
    Camera.configVideoChannel(CHANNEL, config);
    Camera.configVideoChannel(CHANNELNN, configNN);
    Camera.videoInit();
    rtsp.configVideo(config);
    rtsp.begin();
    rtsp_portnum = rtsp.getPort();

    //  定義物件檢測的視訊設定(包含解析度、幀率、模型等)
    ObjDet.configVideo(configNN);
    ObjDet.modelSelect(OBJECT_DETECTION, DEFAULT_YOLOV4TINY, NA_MODEL, NA_MODEL);
    //  開始執行物件檢測
    ObjDet.begin();

    //  設置視訊串流的輸入和輸出,並開始視訊串流
    videoStreamer.registerInput(Camera.getStream(CHANNEL));
    videoStreamer.registerOutput(rtsp);
    if (videoStreamer.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }

    //  啟動CHANNEL通道,獲得該通道中捕捉的視訊串流
    Camera.channelBegin(CHANNEL);

    //  配置並啟動另一個視訊串流,專門用於物件檢測
    videoStreamerNN.registerInput(Camera.getStream(CHANNELNN));
    videoStreamerNN.setStackSize();
    videoStreamerNN.setTaskPriority();
    videoStreamerNN.registerOutput(ObjDet);
    if (videoStreamerNN.begin() != 0) {
        Serial.println("StreamIO link start failed");
    }

    //  啟動CHANNELNN通道,獲得該通道中捕捉的視訊串流
    Camera.channelBegin(CHANNELNN);

    //  配置和啟動OSD功能
    OSD.configVideo(CHANNEL, config);
    OSD.begin();
}

//  主要的程式循環
void loop()
{
    //  從物件檢測objDet中獲取檢測結果,並將其儲存在名為results中
    std::vector<ObjectDetectionResult> results = ObjDet.getResult();

    //  獲取視訊的高度和寬度
    uint16_t im_h = config.height();
    uint16_t im_w = config.width();

    //  將RTSP流的URL打印到串列埠
    Serial.print("Network URL for RTSP Streaming: ");
    Serial.print("rtsp://");
    Serial.print(ip);
    Serial.print(":");
    Serial.println(rtsp_portnum);
    Serial.println(" ");

    //  使用printf函數將偵測到的物件總數輸出到串列埠
    printf("Total number of objects detected = %d\r\n", ObjDet.getResultCount());
    //  在指定的視訊通到創建一個bitmap
    OSD.createBitmap(CHANNEL);

    //  判斷是否偵測到一個物件,如果視返回大於0的數字
    if (ObjDet.getResultCount() > 0) {
        //  返回偵測到的物件總數
        for (int i = 0; i < ObjDet.getResultCount(); i++) {
            //  獲取第i個偵測到的物件類型
            int obj_type = results[i].type();
            //  檢查是否應忽略物件
            if (itemList[obj_type].filter) {

                ObjectDetectionResult item = results[i];
                
                int xmin = (int)(item.xMin() * im_w);
                int xmax = (int)(item.xMax() * im_w);
                int ymin = (int)(item.yMin() * im_h);
                int ymax = (int)(item.yMax() * im_h);

                printf("Item %d %s:\t%d %d %d %d\n\r", i, itemList[obj_type].objectName, xmin, xmax, ymin, ymax);
                OSD.drawRect(CHANNEL, xmin, ymin, xmax, ymax, 3, OSD_COLOR_WHITE);

                //  宣告text_str的字元陣列,長度為20
                char text_str[20];
                //  使用snprintf函數將文字格式化輸出到text_str中。它會將itemList[obj_type].objectName的字串和item.score()的整數轉換為字串後放入text_str中
                snprintf(text_str, sizeof(text_str), "%s %d", itemList[obj_type].objectName, item.score());
                //  呼叫OSD.darwText函數。並在顯示位置xmin, ymin - OSD.getTextHeight(CHANNEL),顯示text_str的內容,文字顏色為指定為青色
                OSD.drawText(CHANNEL, xmin, ymin - OSD.getTextHeight(CHANNEL), text_str, OSD_COLOR_CYAN);
                //  檢查物體名稱是否與字串"bottle"相同
                if(strcmp(itemList[obj_type].objectName, "bottle") == 0)
                {
                  //  控制伺服馬達的角度,將角度從0遞增到90度
                  for (pos = 0; pos <= 90; pos += 1) 
                  {
                    //  將角度值傳送給伺服馬達,以控制其位置
                    myservo.write(pos);
                    //  延遲0.0015秒
                    delay(15);
                  }
                  //  控制伺服馬達的角度,將角度從90遞減到0度
                  for (pos = 90; pos >= 0; pos -= 1) 
                  {
                    //  將角度值傳送給伺服馬達,以控制其位置
                    myservo.write(pos);
                    //  延遲0.0015秒
                    delay(15);
                  }
                }
            }
        }
    }
    //  更新顯示
    OSD.update(CHANNEL);

    //  延遲0.1秒
    delay(100);
}
