#include "videothread.h"

VideoThread::VideoThread(QObject *parent) : QThread(parent), m_stop(false)
{
    faceInit();
}

VideoThread::~VideoThread()
{
    stop();
    wait();
}
//人脸识别初始化
void VideoThread::faceInit()
{
    //-----------------------人脸识别初始化--------------------------------------------
    //初始化人脸数据
    std::string face_detetor_path="../onnx/yunet.onnx";              //检测模型路径
    std::string face_recog_path="../onnx/face_recognizer_fast.onnx"; //识别模型路径
    std::string face_data_path="../faceptr";                             //人脸图片信息路径
    this->face_detector_recog.initFaceModels(face_detetor_path,face_recog_path,face_data_path);//初始化人脸模型
}
//录入人脸
void VideoThread::faceInput(QString image_path)
{
    imwrite(image_path.toStdString(),frame);
}
//保存打卡图片
void VideoThread::clock_in_ptr(QString image_path)
{
    if(!imwrite(image_path.toStdString(),frame))
    {
      std::cout<<"clock_in_ptr Error"<<std::endl;
    }
}
//任务函数
void VideoThread::run()
{
    cv::VideoCapture capture;
    capture.open(Cap);
    if (capture.isOpened())
    {
        while (!m_stop)
        {
            capture >> frame;
            if (!frame.empty())
            {
                cv::flip(frame,frame,1);//镜像翻转
                std::vector<std::shared_ptr<faceInfo>>results;
                this->face_detector_recog.detectFace(frame,results,1);//1表示显示FPS
                //显示名字
                this->face_detector_recog.matchFace(frame,results,false);
                for(auto oneface:results)
                {
                    QRect box2;
                    box2.setX(int(oneface->detResult.at<float>(0,0)));
                    box2.setY(int(oneface->detResult.at<float>(0,1))+120);
                    box2.setWidth(int(oneface->detResult.at<float>(0,2)));
                    box2.setHeight(int(oneface->detResult.at<float>(0,3)));
                    emit sendXh(QString::fromStdString(oneface->name),box2);//将学号发送过去
                    //cv::Rect box;
                    //box.x=int(oneface->detResult.at<float>(0,0));
                    //box.y=int(oneface->detResult.at<float>(0,1));
                    //box.width=int(oneface->detResult.at<float>(0,2));
                    //box.height=int(oneface->detResult.at<float>(0,3));
                    //cv::rectangle(frame,box,cv::Scalar(144,238,144),2,16,0);
                    //显示五官
                    //circle(frame,cv::Point2i(int(oneface->detResult.at<float>(0,4)),int(oneface->detResult.at<float>(0,5))),2,cv::Scalar(255,0,0),2,8,0);
                    //circle(frame,cv::Point2i(int(oneface->detResult.at<float>(0,6)),int(oneface->detResult.at<float>(0,7))),2,cv::Scalar(0,255,0),2,8,0);
                    //circle(frame,cv::Point2i(int(oneface->detResult.at<float>(0,8)),int(oneface->detResult.at<float>(0,9))),2,cv::Scalar(0,0,255),2,8,0);
                    //circle(frame,cv::Point2i(int(oneface->detResult.at<float>(0,10)),int(oneface->detResult.at<float>(0,11))),2,cv::Scalar(122,122,0),2,8,0);
                    //circle(frame,cv::Point2i(int(oneface->detResult.at<float>(0,12)),int(oneface->detResult.at<float>(0,13))),2,cv::Scalar(0,122,122),2,8,0);
                    //画名字
                    //cv::putText(frame,oneface->name,cv::Point(box.tl()),cv::FONT_HERSHEY_SIMPLEX,1,cv::Scalar(237,149,100),2,8,0);
                }
                // 将OpenCV的Mat转换为Qt的QImage
                QImage img(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_RGB888);
                img = img.rgbSwapped();
                // 发送信号将每一帧图像传递给UI界面
                emit frameReady(img);
            }
        }
    }
    capture.release();
}
//停止播放
void VideoThread::stop()
{
    QMutexLocker locker(&m_mutex);
    m_stop = true;
    m_condition.wakeAll();
}
//开始播放
void VideoThread::start_video()
{
    QMutexLocker locker(&m_mutex);
    m_stop = false;
    locker.unlock();
    // 唤醒等待的条件
    m_condition.wakeAll();
    // 如果线程当前没有运行，则启动线程
    if (!isRunning())
        QThread::start();
}


