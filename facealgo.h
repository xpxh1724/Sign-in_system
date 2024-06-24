#ifndef FACEALGO_H
#define FACEALGO_H
#include "iostream"
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

struct faceInfo {
    std::string name;
    cv::Mat detResult;
};


class FaceAlgo {
public:
    FaceAlgo();
    //用于初始化人脸模型，参数包括检测模型路径、识别模型路径和人脸数据库路径。
    void initFaceModels(std::string detect_model_path, std::string recog_model_path, std::string face_db_dir);
    //用于检测输入图像中的人脸，参数包括输入图像、结果列表和是否显示帧率。
    void detectFace(cv::Mat &frame, std::vector<std::shared_ptr<faceInfo>> &results, bool showFPS);
    //用于对输入图像中的人脸进行匹配，参数包括输入图像、结果列表和是否使用L2距离进行匹配。
    void matchFace(cv::Mat &frame, std::vector<std::shared_ptr<faceInfo>> &results, bool l2=false);
    //用于注册新的人脸，参数包括人脸图像和姓名。
    void registFace(cv::Mat &faceRoi, std::string name);
private:
    std::map<std::string, cv::Mat> face_models;
    cv::Ptr<cv::FaceDetectorYN> faceDetector;
    cv::Ptr<cv::FaceRecognizerSF> faceRecognizer;
};


#endif // FACEALGO_H
