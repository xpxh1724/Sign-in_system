#ifndef SIGNIN_H
#define SIGNIN_H

#include <QWidget>
#include "videothread.h"  //视频线程头文件
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QTimer>
#include <QTextToSpeech>//说话头文件
#include <QDir>
#include <QDesktopServices>
#include <QCloseEvent>
using namespace cv;
namespace Ui {
class Widget;
}
enum cap_ZT{cap1_open,cap2_open,cap3_open,cap_close};
class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();
    //初始化
    void init();
    //测测摄像头
    void checkVideo();
protected:
    //重写父类的方法
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
signals:
    void cap_chose_Changed(cap_ZT CAP);
    void save_clock_in_ptr(QString path);
private slots:
    //---------------- Menu -----------------------
    //btn-Close 关闭
    void on_btn_Close_clicked();
    //btn-Min 最小化
    void on_btn_Min_clicked();


    //------------- Left 左侧操作栏------------------
    //btn_Clock_in签到界面按钮
    void on_btn_Clock_in_clicked();
    //btn_Check_video检查摄像头界面按钮
    void on_btn_Check_video_clicked();
    //btn-Sreach 查看数据库信息按钮
    void on_btn_Search_clicked();
    //btn_root 管理员界面
    void on_btn_root_clicked();
    //btn_record 打卡记录按钮
    void on_btn_record_clicked();

    //---------------- Voide ----------------------
    //将视频线程对象传递的每一帧图像设置到视频显示窗口
    void onFrameReady(QImage frame);

    //------------- Clock in界面① -----------------
    //btn-input 人脸录入
    void on_btn_input_clicked();

    //------------- CheckVideo界面② ---------------
    //btn-Retest 重新检测
    void on_btn_Retest_clicked();
    //btn-cap1 使用①号摄像头
    void on_btn_cap1_open_clicked();
    //btn-cap2 使用②号摄像头
    void on_btn_cap2_open_clicked();
    //btn-cap3 使用③号摄像头
    void on_btn_cap3_open_clicked();

    //--------------  Search界面③ ----------------
    //btn_Search_exec 执行sql按钮
    void on_btn_Search_exec_clicked();
    //btn_Search_all 查询所有表
    void on_btn_Search_all_clicked();
    //btn_Search_find 查找
    void on_btn_Search_find_clicked();
    //btn_Search_delete 删除
    void on_btn_Search_delete_clicked();

    //-------------- Root界面④ --------------------
    //btn_Root_Frist-首页按钮
    void on_btn_Root_Frist_clicked();
    //btn_Root_update-更改信息按钮
    void on_btn_Root_update_clicked();
    //下拉框-当前考勤类型-Changed事件
    void on_Root_clock_type_box_currentIndexChanged(const QString &clock_type);
    //btn_btn_Root_clock_type_update-首页->考勤类型确认更改按钮
    void on_btn_Root_clock_type_update_clicked();
    //btn_Root_clock_group_update-首页->考勤组考勤类型确认更改按钮
    void on_btn_Root_clock_group_update_clicked();
    //btn_Root_Update_insert-更改信息->确认新增按钮
    void on_btn_Root_Update_insert_clicked();
    //btn_Root_Update_update-更改信息->确认修改按钮
    void on_btn_Root_Update_update_clicked();
    //groupBox_update_type_box-更改信息-修改-下拉框-考勤类型-Changed事件
    void on_groupBox_update_type_box_currentIndexChanged(const QString &type);
    //btn_Root_Update_update-更改信息->确认删除按钮
    void on_btn_Root_Update_delete_clicked();
    //btn_Root_0-一键置0按钮
    void on_btn_Root_0_clicked();

    //------------ Record记录 界面⑤ ---------------
    //clock_type_box-下拉框-考勤类型-Changed事件
    void on_clock_type_box_currentIndexChanged(const QString &type);
    //clock_group_box-下拉框-考勤组-Changed事件
    void on_clock_group_box_currentIndexChanged(const QString &group);
    //dateEdit-时间选择-Changed事件
    void on_dateEdit_dateTimeChanged(const QDateTime &dateTime);
    //btn_clock_record_name_search姓名搜索按钮
    void on_btn_clock_record_name_search_clicked();
    //btn_look_record 查看打卡照片按钮
    void on_btn_look_record_clicked();
    //btn_look_record_all 查看所有记录按钮
    void on_btn_look_record_all_clicked();

private:
    Ui::Widget *ui;
    bool m_bIsPressed=false;	//鼠标按下标志位
    QPoint m_lastPt;	//记录第一次鼠标按下的局部坐标
    //--------------- 视频流 -----------------------
    VideoThread *m_videoThread;       //创建视频线程对象
    //更新实时时间
    QTimer *upTime;
    QSqlQueryModel* qmodel = new QSqlQueryModel;//Search
    QSqlQueryModel* qmodel_Record = new QSqlQueryModel;//Record
    //语音播报
    QTextToSpeech *speech;
    //打卡文件夹
    QDir dir;
};



#endif // SIGNIN_H
