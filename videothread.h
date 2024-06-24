#include <QThread>
#include <QImage>
#include <QMutex>
#include <QWaitCondition>
#include"facealgo.h"      //人脸识别算法头文件
class VideoThread : public QThread
{
    Q_OBJECT

public:
    explicit VideoThread(QObject *parent = nullptr);
    ~VideoThread()override;
    //任务函数
    void run() override;
    //停止播放
    void stop();
    //开始播放
    void start_video();
    //设置使用哪个cap
    void setCap(int cap){ Cap=cap;}
    //人脸识别初始化
    void faceInit();
    //录入人脸
    void faceInput(QString image_path);
    //保存打卡图片
    void clock_in_ptr(QString image_path);
signals:
    //读图片的信号
    void frameReady(QImage frame);
    //发送学号和人脸位置
    void sendXh(QString xh,QRect);
protected:

private:
    bool m_stop;
    QMutex m_mutex;
    QWaitCondition m_condition;
    int Cap=0;
    //--------------- 人脸识别 ---------------------
    FaceAlgo  face_detector_recog;    //人脸识别对象
    cv::Mat frame;                   //全局图片
};


