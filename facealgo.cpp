#include "facealgo.h"

FaceAlgo::FaceAlgo() {
    std::cout<<"create instance"<<std::endl;
}
//用于初始化人脸模型，参数包括检测模型路径、识别模型路径和人脸数据库路径。
void FaceAlgo::initFaceModels(std::string detect_model_path, std::string recog_model_path, std::string face_db_dir) {
    //首先，使用检测模型路径创建一个FaceDetectorYN对象(模型路径,输入图像大小,置信度阈值等参数)
    this->faceDetector = cv::FaceDetectorYN::create(detect_model_path, "", cv::Size(300, 300), 0.9f, 0.8f, 500);
    //然后，使用识别模型路径创建一个FaceRecognizerSF对象。
    this->faceRecognizer = cv::FaceRecognizerSF::create(recog_model_path, "");
    std::vector<std::string> fileNames;
    //接下来，使用cv::glob函数获取人脸数据库目录下的所有文件路径，并遍历每个文件路径。
    cv::glob(face_db_dir, fileNames);
    for(std::string file_path : fileNames)
    {
        //对于每个文件路径，使用cv::imread函数读取图像并存储在一个cv::Mat对象中。
        cv::Mat image = cv::imread(file_path);
        //然后，从文件路径中提取人脸姓名，将图像和姓名作为参数调用registFace方法进行人脸注册。
        int pos = static_cast<int>(file_path.find("\\"));
        std::string image_name = file_path.substr(pos+1, file_path.length() - pos - 5);
        this->registFace(image, image_name);
        // 最后，输出每个注册的人脸的文件名。
        std::cout<<"file name : " << image_name<< ".jpg"<<std::endl;
    }
}
//用于检测输入图像中的人脸，参数包括输入图像、结果列表和是否显示帧率。
void FaceAlgo::detectFace(cv::Mat &image, std::vector<std::shared_ptr<faceInfo>> &infoList, bool showFPS) {
    //首先，创建一个TickMeter对象用于计算帧率，并设置一个用于展示帧率的提示信息字符串。
    cv::TickMeter tm;
    std::string msg = "FPS: ";
    //然后，开始计时
    tm.start();
    // Set input size before inference--在推理前设置输入大小
    //接下来，使用输入图像的大小设置人脸检测器的输入尺寸。
    this->faceDetector->setInputSize(image.size());

    // Inference
    cv::Mat faces;
    //然后，调用人脸检测器的detect方法进行人脸检测，将检测结果保存在一个名为faces的Mat对象中。
    this->faceDetector->detect(image, faces);
    //停止计时
    tm.stop();
    // Draw results on the input image--在输入图像上绘制结果
    //然后，遍历检测结果中的每个人脸，并对每个人脸进行操作。
    for (int i = 0; i < faces.rows; i++)
    {
        // Draw bounding box--画边框
        //创建一个faceInfo对象，并将其初始化为"Unknown"姓名和从faces中复制的检测结果。
        auto fi = std::shared_ptr<faceInfo>(new faceInfo());
        fi->name = "Unknown";
        faces.row(0).copyTo(fi->detResult);
        //将faceInfo对象添加到infoList中。
        infoList.push_back(fi);
    }
    //最后，如果需要显示帧率，则在输入图像上绘制帧率信息。
    if(showFPS) {
        putText(image, msg + std::to_string(tm.getFPS()), cv::Point(480, 20), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0,0,255), 1,16,0);
    }
}
//用于对输入图像中的人脸进行匹配，参数包括输入图像、结果列表和是否使用L2距离进行匹配。
void FaceAlgo::matchFace(cv::Mat &frame, std::vector<std::shared_ptr<faceInfo>> &infoList, bool l2) {
    double cosine_similar_thresh = 0.363;
    double l2norm_similar_thresh = 1.128;
    //遍历infoList中的每个人脸信息对象，对每个人脸进行操作。
    for(auto face : infoList) {
        //创建一个Mat对象aligned_face和feature，用于存储对齐裁剪后的人脸和人脸特征。
        cv::Mat aligned_face, feature;
        //调用faceRecognizer的alignCrop方法对输入图像的人脸进行对齐裁剪，将结果保存在aligned_face中。
        faceRecognizer->alignCrop(frame, face->detResult, aligned_face);
        //调用faceRecognizer的feature方法计算aligned_face的人脸特征，将结果保存在feature中。
        faceRecognizer->feature(aligned_face, feature);
        //初始化min_dist为100.0和max_cosine为0.0，将matchedName设置为"Unknown"。
        double min_dist = 100.0;
        double max_cosine = 0.0;
        std::string matchedName = "Unknown";
        //遍历face_models中的每个人脸模型，对每个人脸模型进行操作。
        for(auto item : this->face_models) {
            //std::cout<<"face_models.item :" << item.first << std::endl;
            //std::cout<<"face_models.item :" << item.second << std::endl;
            //如果l2为真，则调用faceRecognizer的match方法，使用L2范数计算feature与item.second之间的相似度，将结果保存在L2_score中。
            if(l2) {
                double L2_score = faceRecognizer->match(feature, item.second, cv::FaceRecognizerSF::DisType::FR_NORM_L2);
                //如果L2_score小于min_dist，则更新min_dist为L2_score，matchedName为item.first。
                if(L2_score < min_dist) {
                    min_dist = L2_score;
                    matchedName = item.first;
                }
            } else {
                //如果l2为假，则调用faceRecognizer的match方法，使用余弦相似度计算feature与item.second之间的相似度，将结果保存在cos_score中。
                double cos_score = faceRecognizer->match(feature, item.second, cv::FaceRecognizerSF::DisType::FR_COSINE);
                //如果cos_score大于max_cosine，则更新max_cosine为cos_score，matchedName为item.first。
                if(cos_score > max_cosine) {
                    max_cosine = cos_score;
                    matchedName = item.first;
                }
            }
        }
        //std::cout<<"matchedName :" << matchedName << std::endl;
        //如果max_cosine大于cosine_similar_thresh，则将face的name字段清空并添加matchedName作为新的名字。
        if(max_cosine > cosine_similar_thresh) {
            face->name.clear();
            face->name.append(matchedName);
        }
        //如果l2为真且min_dist小于l2norm_similar_thresh，则将face的name字段清空并添加matchedName作为新的名字。
        if(l2 && min_dist < l2norm_similar_thresh) {
            face->name.clear();
            face->name.append(matchedName);
        }
        // std::cout<<"face.name :" << face->name << std::endl;
        // std::cout<<"max_cosine :" << max_cosine<< std::endl;
        // std::cout<<"min_dist :" << min_dist<< std::endl;
    }
}
//用于注册新的人脸，参数包括人脸图像和姓名。
void FaceAlgo::registFace(cv::Mat &frame, std::string name)
{
    //首先，函数检查输入的frame是否为空，如果为空，则输出提示信息并返回。
    if(frame.empty())
    {
        std::cout<<"ptr is empty"<<std::endl;
        return;
    }
    //接下来，函数调用this->faceDetector->setInputSize(frame.size())来设置人脸检测器的输入大小，这里使用frame.size()获取输入图像的大小。
    this->faceDetector->setInputSize(frame.size());

    // Inference
    cv::Mat faces;
    //然后，函数调用this->faceDetector->detect(frame, faces)进行人脸检测，并将检测结果存储在faces中。
    this->faceDetector->detect(frame, faces);
    cv::Mat aligned_face, feature;
    //接着，函数调用faceRecognizer->alignCrop(frame, faces.row(0), aligned_face)对检测到的人脸进行对齐和裁剪，得到一个与检测结果相对应的裁剪图像aligned_face。
    faceRecognizer->alignCrop(frame, faces.row(0), aligned_face);
    //接下来，函数调用faceRecognizer->feature(aligned_face, feature)提取裁剪图像的特征，并将特征存储在feature中。
    faceRecognizer->feature(aligned_face, feature);
    //最后，函数调用this->face_models.insert(std::pair<std::string, cv::Mat>(name, feature.clone()))将人脸名称和特征存储在类中的face_models映射中。
    this->face_models.insert(std::pair<std::string, cv::Mat>(name, feature.clone()));
}
