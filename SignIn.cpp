#include "SignIn.h"
#include "ui_SignIn.h"
#include <QCameraInfo>    //检查本地摄像头头文件
#include <QDateTime>
#include <QInputDialog>
#include <QMessageBox>
#include <QUrl>

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);
    init();
}

Widget::~Widget()
{
    delete ui;
}
//鼠标按下事件
void Widget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_bIsPressed = true;
        m_lastPt = event->globalPos() - this->pos();
        event->accept();
    }
}
//鼠标释放事件
void Widget::mouseReleaseEvent(QMouseEvent *event)
{
    m_bIsPressed = false;	//鼠标按下标志位还原置为false
    Q_UNUSED(event);
}
//鼠标移动事件
void Widget::mouseMoveEvent(QMouseEvent *event)
{
    //if (m_bIsPressed && (event->buttons() && Qt::LeftButton))
    if (m_bIsPressed && (event->buttons()))
    {
        move(event->globalPos() - m_lastPt);
        event->accept();
    }
}
//初始化
void Widget::init()
{
    //去除窗口标题栏和边框
    this->setWindowFlags(Qt::FramelessWindowHint);
    setWindowTitle("人脸识别考勤系统");
    setGeometry(500,150,this->geometry().width(),this->geometry().height());
    //设置透明2-窗体标题栏不透明,背景透明
    this->setAttribute(Qt::WA_TranslucentBackground, true);

    //设置默认界面
    ui->stackedWidget->setCurrentIndex(0);

    //------------------------数据库初始化---------------------------------------------
    {
        //连接数据库
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName("../sign_in.db"); //如果本目录下没有该文件,则会在本目录下生成,否则连接该文件
        if (!db.open()) {
            QMessageBox::warning(this, QObject::tr("Database Error"),db.lastError().text());
        }
        else {
            qDebug()<<"QSQLITE open sign_in.db success!"<<endl;
        }
    }
    //-----------------------语音播报初始化--------------------------------------------
    {
        speech=new QTextToSpeech;
        speech->setLocale(QLocale::Chinese);//设置语音合成的语言为中文。
        speech->setPitch(1.0);//设置语音的音调 1.0代表默认音调
        speech->setRate(0.0);//设置语音的语速，0.0代表默认语速
        speech->setVolume(1.0);//设置语音的音量，1.0代表最大音量。
    }
    //-----------------------UI显示的初始化--------------------------------------------
    {
        ui->looplab->hide();
        ui->looplab_text->hide();
        ui->over_show_xh->clear();
        ui->over_show_name->clear();
        ui->over_show_kqz->clear();
        ui->over_show_2_qd->clear();
        ui->over_show_2_qd_logo->clear();
        ui->over_show_2_qt->clear();
        ui->over_show_2_qt_logo->clear();
        //更新-考勤类型UI
        QSqlQuery sql;
        if(sql.exec(tr("select *from clock_in where clock_type_name='%1';").arg(ui->clock_in_type_name_lbl->text())))
        {
            sql.next();
            ui->clock_in_time_start_lbl->setText(sql.value(1).toString());
            ui->clock_in_time_end_lbl->setText(sql.value(2).toString());
            ui->clock_in_type_auto_lbl->setText(sql.value(3).toString()+"签退");
        }
        sql.clear();
    }
    //------------------------实时时间更新+自动下班打卡----------------------------------
    {
        upTime=new QTimer;
        connect(upTime,&QTimer::timeout,this,[=]{
            // 获取当前本地时间
            QDateTime currentDateTime = QDateTime::currentDateTime();
            //qDebug()<<"now:"<<currentDateTime<<endl;
            ui->time_lbl->setText(currentDateTime.toString("yyyy/MM/dd HH:mm:ss ddd"));
            if(ui->clock_in_type_auto_lbl->text()=="自动签退")
            {
                //到达下班时间--自动下班打卡
                if(currentDateTime.toString("hh:mm:ss")==ui->clock_in_time_end_lbl->text())
                {
                    QSqlQuery sql;
                    if(sql.exec(QString("update stu set is_clock_out=1 where clock_type_name='%1';").arg(ui->clock_in_type_name_lbl->text())))
                    {

                        qDebug()<<"自动下班打卡成功"<<endl;
                        on_btn_Root_0_clicked();
                        if(speech->state()==QTextToSpeech::Ready)
                        {
                            speech->say("下班啦 已自动签退!");//开始合成文本
                        }
                    }
                }
            }else if (ui->clock_in_type_auto_lbl->text()=="人脸签退")
            {
                //到达下班时间+5min后自动结算今天考勤
                if(currentDateTime.addSecs(300).toString("hh:mm:ss")==ui->clock_in_time_end_lbl->text())
                {
                    QSqlQuery sql;
                    if(sql.exec(QString("update stu set is_clock_out=1 where clock_type_name='%1';").arg(ui->clock_in_type_name_lbl->text())))
                    {
                        on_btn_Root_0_clicked();
                        qDebug()<<"结束今天考勤-成功/!"<<endl;
                        if(speech->state()==QTextToSpeech::Ready)
                        {
                            speech->say("已自动结算今日考勤!");//开始合成文本
                        }
                    }
                }
            }
        });
        upTime->start(1000);
    }
    //---------------------- 检测本地所有摄像头 ----------------------------------------
    checkVideo();
    //-----------------------视频显示初始化--------------------------------------------
    {
        m_videoThread = new VideoThread(this);//初始化视频线程对象
        ui->video_Show->setScaledContents(true);//图像自适应大小
        connect(m_videoThread, &VideoThread::frameReady, this, &Widget::onFrameReady);//连接图像传输信号槽
        //摄像头变化信号槽
        connect(this,&Widget::cap_chose_Changed,this,[=](cap_ZT CAP){
            switch (CAP) {
            case cap1_open:
                qDebug()<<"cap1_open"<<endl;
                m_videoThread->setCap(0);
                m_videoThread->start_video();
                break;
            case cap2_open:
                qDebug()<<"cap2_open"<<endl;
                m_videoThread->setCap(1);
                m_videoThread->start_video();
                break;
            case cap3_open:
                m_videoThread->setCap(2);
                m_videoThread->start_video();
                break;
            default: m_videoThread->stop();break;
            }
        });
        //打开摄像头①号
        emit cap_chose_Changed(cap1_open);
    }
    //---------------------人脸检测-打卡检测-------------------------------------------
    //检测人脸是否识别到人脸
    connect(m_videoThread,&VideoThread::sendXh,this,[=](QString xh,QRect wz){
        //①如果识别成功且录入了人脸信息
        if(xh!="Unknown"&&xh!=nullptr)
        {
            //更新显示-人脸光圈位置
            ui->looplab->show();
            ui->looplab->setGeometry(wz);
            ui->looplab_text->show();
            ui->looplab_text->setGeometry(wz.x()-10,wz.y()+wz.height()-30,240,60);
            //更新显示-识别信息
            QSqlQuery query;
            query.exec(QString("select * from stu where xh='%1'").arg(xh));
            query.next();
            ui->over_show_xh->setText(query.value(0).toString());
            ui->over_show_name->setText(query.value(1).toString());
            ui->over_show_kqz->setText(query.value(2).toString());
            //判断是否签到-图标显示
            if(query.value(3).toInt()==1)
            {
                ui->over_show_2_qd->setText("已签到");
                ui->over_show_2_qd_logo->setPixmap(QPixmap(":/ptr/success.png").scaled(36,36, Qt::KeepAspectRatio));

            }else {
                ui->over_show_2_qd->setText("未签到");
                ui->over_show_2_qd_logo->setPixmap(QPixmap(":/ptr/fail.png").scaled(36,36, Qt::KeepAspectRatio));
            }
            //判断是否签退-图标显示
            if(query.value(4).toInt()==1)
            {
                ui->over_show_2_qt->setText("已签退");
                ui->over_show_2_qt_logo->setPixmap(QPixmap(":/ptr/success.png").scaled(36,36, Qt::KeepAspectRatio));
            }
            else {
                ui->over_show_2_qt->setText("未签退");
                ui->over_show_2_qt_logo->setPixmap(QPixmap(":/ptr/fail.png").scaled(36,36, Qt::KeepAspectRatio));
            }

            //②检测这个人的打卡状态-如果没有打卡
            if(query.value(3).toInt()!=1)
            {
                //③考勤类型名称为空时
                if(query.value(5).toString()==nullptr)
                {
                    ui->looplab_text->setText("你的考勤类型为空!");
                    return;
                }else if (ui->clock_in_type_name_lbl->text()!=query.value(5).toString())
                {
                    ui->looplab_text->setText("考勤类型不匹配!");
                    return;
                }
                //④查询该考勤类型今天是否需要考勤
                QString week=QDateTime::currentDateTime().toString("ddd");//周几
                int week_num=0;
                if(week=="周一") {week_num=1;}
                if(week=="周二") {week_num=2;}
                if(week=="周三") {week_num=3;}
                if(week=="周四") {week_num=4;}
                if(week=="周五") {week_num=5;}
                if(week=="周六") {week_num=6;}
                if(week=="周日") {week_num=7;}
                QString str= QString("select clock_in.* from stu,clock_in where stu.xh='%1' and stu.clock_type_name=clock_in.clock_type_name").arg(xh);
                if(query.exec(str))
                {
                    ui->looplab_text->setText("还没有打卡");
                    query.next();
                    //④-ok-今天需要考勤
                    if(query.value(3+week_num).toInt()==1)
                    {
                        //考勤起始时间
                        QTime clock_in_time=query.value(1).toTime();
                        //考勤结束时间
                        QTime clock_out_time=query.value(2).toTime();
                        //检测此刻的时间
                        QTime now_time = QTime::currentTime();
                        //未到打卡时间--现在的时间小于考勤开始时间
                        if(now_time<clock_in_time.addSecs(-1800))
                        {
                            ui->looplab_text->setText("未到考勤时间");
                        }
                        //打卡成功--现在的时间大于等于考勤开始时间且小于考勤起始时间
                        else if (now_time>=clock_in_time.addSecs(-1800)&&now_time<=clock_in_time)
                        {
                            ui->looplab_text->setText("打卡成功!");
                            if(query.exec(QString("update stu set is_clock_in=1  where xh='%1';").arg(xh)))
                            {
                                qDebug()<<"打卡成功！"<<endl;
                                if(query.exec(QString("insert into record values('%1','%2','%3','%4','%5');").arg(ui->clock_in_type_name_lbl->text()).arg(ui->over_show_name->text()).arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")).arg("").arg(ui->over_show_kqz->text())))
                                {
                                    qDebug()<<"打卡信息插入成功！"<<endl;
                                    if(speech->state()==QTextToSpeech::Ready)
                                    {
                                        speech->say("上班啦 打卡成功!");//开始合成文本
                                    }
                                    //----------------打卡照片保存------------------------
                                    // 获取当前本地时间
                                    QDateTime currentDateTime = QDateTime::currentDateTime();
                                    //拼接文件夹路径
                                    QString mkdir="../clock_in/"+currentDateTime.toString("yyyy-MM-dd");
                                    //拼接照片路径
                                    QString ptr_path=mkdir+"/"+ui->over_show_xh->text()+".jpg";
                                    //创建文件夹
                                    if (!dir.exists(mkdir)) {
                                        dir.mkpath(mkdir);
                                        qDebug()<<"创建文件夹："<<mkdir<<"成功！"<<endl;
                                        qDebug()<<"打卡照片名称为："<<ptr_path<<endl;
                                        emit save_clock_in_ptr(ptr_path);
                                    }else {
                                        qDebug()<<"文件夹："<<mkdir<<"已存在！"<<endl;
                                        qDebug()<<"打卡照片名称为："<<ptr_path<<endl;
                                        emit save_clock_in_ptr(ptr_path);
                                    }
                                }
                            }
                        }
                        //迟到打卡--现在的时间大于考勤起始时间小于等于考勤起始时间+10分钟(600秒)
                        else if (now_time>clock_in_time&&now_time<=clock_in_time.addSecs(600))
                        {
                            ui->looplab_text->setText("迟到打卡!");
                            if(query.exec(QString("update stu set is_clock_in=1  where xh='%1'").arg(xh)))
                            {
                                qDebug()<<"迟到打卡！"<<endl;
                                if(query.exec(QString("insert into record values('%1','%2','%3','%4','%5');").arg(ui->clock_in_type_name_lbl->text()).arg(ui->over_show_name->text()).arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")).arg("").arg(ui->over_show_kqz->text())))
                                {
                                    qDebug()<<"打卡信息插入成功！"<<endl;
                                    if(speech->state()==QTextToSpeech::Ready)
                                    {
                                        speech->say("上班啦 迟到打卡!");//开始合成文本
                                    }
                                    //----------------打卡照片保存------------------------
                                    // 获取当前本地时间
                                    QDateTime currentDateTime = QDateTime::currentDateTime();
                                    //拼接文件夹路径
                                    QString mkdir="../clock_in/"+currentDateTime.toString("yyyy-MM-dd");
                                    //拼接照片路径
                                    QString ptr_path=mkdir+"/"+ui->over_show_xh->text()+".jpg";
                                    //创建文件夹
                                    if (!dir.exists(mkdir)) {
                                        dir.mkpath(mkdir);
                                        qDebug()<<"创建文件夹："<<mkdir<<"成功！"<<endl;
                                        qDebug()<<"打卡照片名称为："<<ptr_path<<endl;
                                        emit save_clock_in_ptr(ptr_path);
                                    }else {
                                        qDebug()<<"文件夹："<<mkdir<<"已存在！"<<endl;
                                        qDebug()<<"打卡照片名称为："<<ptr_path<<endl;
                                        emit save_clock_in_ptr(ptr_path);
                                    }
                                }
                            }
                        }
                        //迟到打卡--现在的时间大于考勤起始时间+10分钟(600秒)且小于等于考勤结束时间
                        else if(now_time>clock_in_time.addSecs(600)&&now_time<=clock_out_time)
                        {
                            ui->looplab_text->setText("异常打卡!");
                            if(query.exec(QString("update stu set is_clock_in=1  where xh='%1'").arg(xh)))
                            {
                                qDebug()<<"异常打卡！"<<endl;
                                if(query.exec(QString("insert into record values('%1','%2','%3','%4','%5');").arg(ui->clock_in_type_name_lbl->text()).arg(ui->over_show_name->text()).arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")).arg("").arg(ui->over_show_kqz->text())))
                                {
                                    qDebug()<<"打卡信息插入成功！"<<endl;
                                    if(speech->state()==QTextToSpeech::Ready)
                                    {
                                        speech->say("上班啦 异常打卡!");//开始合成文本
                                    }
                                    //----------------打卡照片保存------------------------
                                    // 获取当前本地时间
                                    QDateTime currentDateTime = QDateTime::currentDateTime();
                                    //拼接文件夹路径
                                    QString mkdir="../clock_in/"+currentDateTime.toString("yyyy-MM-dd");
                                    //拼接照片路径
                                    QString ptr_path=mkdir+"/"+ui->over_show_xh->text()+".jpg";
                                    //创建文件夹
                                    if (!dir.exists(mkdir)) {
                                        dir.mkpath(mkdir);
                                        qDebug()<<"创建文件夹："<<mkdir<<"成功！"<<endl;
                                        qDebug()<<"打卡照片名称为："<<ptr_path<<endl;
                                        emit save_clock_in_ptr(ptr_path);
                                    }else {
                                        qDebug()<<"文件夹："<<mkdir<<"已存在！"<<endl;
                                        qDebug()<<"打卡照片名称为："<<ptr_path<<endl;
                                        emit save_clock_in_ptr(ptr_path);
                                    }
                                }
                            }
                        }
                        else
                        {
                            ui->looplab_text->setText("考勤已结束,无法打卡");
                        }
                    }
                    //④-no-今天不需要考勤
                    else
                    {
                        ui->looplab_text->setText("今天不需要考勤");
                    }
                }
                else
                {
                    qDebug()<<"str-sql错误了:"<<str<<endl;
                }
            }
            //已经打卡
            else
            {
                int is_clock_out=query.value(4).toInt();
                //1.已签到未签退
                if(is_clock_out!=1)
                {
                    ui->looplab_text->setText("已打卡！");
                    //判断是否到签退时间
                    QTime clock_out_time=QTime::fromString(ui->clock_in_time_end_lbl->text());
                    QTime now_time=QTime::currentTime();
                    //当时间到达签退时间后-允许签退
                    if(now_time>=clock_out_time)
                    {
                        if(query.exec(QString("update stu set is_clock_out=1  where xh='%1';").arg(xh)))
                        {
                            qDebug()<<"签退成功！"<<endl;
                            if(query.exec(QString("update record set clock_out_time='%1' where name='%2' and kqz='%3' and DATE(clock_in_time)='%4';").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")).arg(ui->over_show_name->text()).arg(ui->over_show_kqz->text()).arg(QDateTime::currentDateTime().toString("yyyy-MM-dd"))))
                            {
                                qDebug()<<"签退信息修改成功！"<<endl;
                                if(speech->state()==QTextToSpeech::Ready)
                                {
                                    speech->say("下班啦 辛苦了");//开始合成文本
                                }
                                //----------------签退照片保存------------------------
                                // 获取当前本地时间
                                QDateTime currentDateTime = QDateTime::currentDateTime();
                                //拼接文件夹路径
                                QString mkdir="../clock_in/"+currentDateTime.toString("yyyy-MM-dd");
                                //拼接照片路径
                                QString ptr_path=mkdir+"/"+ui->over_show_xh->text()+"-2.jpg";
                                //创建文件夹
                                if (!dir.exists(mkdir)) {
                                    dir.mkpath(mkdir);
                                    qDebug()<<"创建文件夹："<<mkdir<<"成功！"<<endl;
                                    qDebug()<<"签退照片名称为："<<ptr_path<<endl;
                                    emit save_clock_in_ptr(ptr_path);
                                }else {
                                    qDebug()<<"文件夹："<<mkdir<<"已存在！"<<endl;
                                    qDebug()<<"签退照片名称为："<<ptr_path<<endl;
                                    emit save_clock_in_ptr(ptr_path);
                                }
                            }
                            else {
                                qDebug()<<"签退信息修改失败！\n"<<query.lastError().text()<<endl;
                            }
                        }
                    }
                }
                //2.已签到已签退
                else if(is_clock_out==1)
                {
                    ui->looplab_text->setText("已签退！");
                }

            }
        }
        //如果未识别成功或未录入人脸信息
        else
        {
            ui->looplab->hide();
            ui->looplab_text->hide();
            ui->over_show_xh->clear();
            ui->over_show_name->clear();
            ui->over_show_kqz->clear();
            ui->over_show_2_qd->clear();
            ui->over_show_2_qd_logo->clear();
            ui->over_show_2_qt->clear();
            ui->over_show_2_qt_logo->clear();
        }
    });
    //保存打卡照片
    connect(this,&Widget::save_clock_in_ptr,m_videoThread,&VideoThread::clock_in_ptr);
}
//检测摄像头
void Widget::checkVideo()
{
    ui->Check_video_cap_lbl->setText("未检测到摄像头");
    ui->Check_video_cap_lbl_2->setText("未检测到摄像头");
    ui->Check_video_cap_lbl_3->setText("未检测到摄像头");
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();   // 获取可用摄像头列表
    int count=1;
    for (auto cap:cameras)
    {
        if(count==1)
        {
            ui->Check_video_cap_lbl->setText(cap.description());
            count++;
        }else if (count==2) {
            ui->Check_video_cap_lbl_2->setText(cap.description());
            count++;
        }
        else if (count==3) {
            ui->Check_video_cap_lbl_3->setText(cap.description());
        }
    }
}

//---------------- Menu -----------------------
//btn-Close 关闭
void Widget::on_btn_Close_clicked()
{
    this->close();
}
//btn-Min 最小化
void Widget::on_btn_Min_clicked()
{
    this->showMinimized();
}

//------------- Left 左侧操作栏------------------
//btn_Clock_in签到界面按钮
void Widget::on_btn_Clock_in_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}
//btn_Check_video检查摄像头界面按钮
void Widget::on_btn_Check_video_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}
//btn-Sreach 查看数据库信息按钮
void Widget::on_btn_Search_clicked()
{
    qmodel->setQuery(QString("select * from stu;"));
    qmodel->setHeaderData(0, Qt::Horizontal, tr("学号"));
    qmodel->setHeaderData(1, Qt::Horizontal, tr("姓名"));
    qmodel->setHeaderData(2, Qt::Horizontal, tr("考勤组"));
    qmodel->setHeaderData(3, Qt::Horizontal, tr("是否签到"));
    qmodel->setHeaderData(4, Qt::Horizontal, tr("是否签退"));
    qmodel->setHeaderData(5, Qt::Horizontal, tr("考勤类型"));
    ui->tableView->horizontalHeader()->setFont(QFont("宋体",12,QFont::Black));
    ui->tableView->verticalHeader()->setVisible(false);   //隐藏列表头
    ui->tableView->setFont(QFont("楷体",12));//设置字体
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);//设置大小与行内容匹配且鼠标不可拖拽
    ui->tableView->setAlternatingRowColors(true);//隔行变色
    ui->tableView->setModel(qmodel);//将模型加载
    ui->stackedWidget->setCurrentIndex(2);
}
//btn_root 管理员界面按钮
void Widget::on_btn_root_clicked()
{
    QSqlQuery sql;
    if(sql.exec(QString("select * from clock_in;")))
    {
        while (sql.next())
        {
            //如果没有则插入
            if(ui->Root_clock_type_box->findText(sql.value(0).toString())==-1)
            {
                ui->Root_clock_type_box->addItem(sql.value(0).toString());
                ui->Clock_group_1_box->addItem(sql.value(0).toString());
                ui->Clock_group_2_box->addItem(sql.value(0).toString());
                ui->Clock_group_3_box->addItem(sql.value(0).toString());
            }
        }
        ui->stackedWidget->setCurrentIndex(3);
        ui->stackedWidget_root->setCurrentIndex(0);
        if(sql.exec(QString("select *from clock_in where clock_type_name='%1'").arg(ui->Root_clock_type_box->currentText())))
        {
            sql.next();
            ui->Root_clock_in_time_lbl->setText(sql.value(1).toString());
            ui->Root_clock_out_time_lbl->setText(sql.value(2).toString());
        }
        if(sql.exec(QString("select clock_type_name from stu where kqz='大一考勤组';")))
        {
            sql.next();
            ui->Clock_group_1_box->setCurrentText(sql.value(0).toString());
        }
        else {
            qDebug()<<"大一考勤组状态查询失败"<<endl;
        }
        if(sql.exec(QString("select clock_type_name from stu where kqz='大二考勤组';")))
        {
            sql.next();
            ui->Clock_group_2_box->setCurrentText(sql.value(0).toString());
        }
        else {
            qDebug()<<"大二考勤组状态查询失败"<<endl;
        }
        if(sql.exec(QString("select clock_type_name from stu where kqz='大三考勤组';")))
        {
            sql.next();
            ui->Clock_group_3_box->setCurrentText(sql.value(0).toString());
        }
        else {
            qDebug()<<"大三考勤组状态查询失败"<<endl;
        }

    }
    else
    {
        QMessageBox::information(this,"管理员界面提示","管理员界面下拉框更新失败！");
    }

}
//btn_record 打卡记录按钮
void Widget::on_btn_record_clicked()
{
    ui->dateEdit->setDate(QDate::currentDate());//设置为今天
    qmodel_Record->setQuery(QString("select * from record where DATE(clock_in_time)='%1';").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd")));
    qmodel_Record->setHeaderData(0, Qt::Horizontal, tr("考勤类型"));
    qmodel_Record->setHeaderData(1, Qt::Horizontal, tr("姓名"));
    qmodel_Record->setHeaderData(2, Qt::Horizontal, tr("签到时间"));
    qmodel_Record->setHeaderData(3, Qt::Horizontal, tr("签退时间"));
    qmodel_Record->setHeaderData(4, Qt::Horizontal, tr("考勤组"));
    ui->tableView_Record->horizontalHeader()->setFont(QFont("宋体",12,QFont::Black));
    ui->tableView_Record->verticalHeader()->setVisible(false);   //隐藏列表头
    ui->tableView_Record->setFont(QFont("楷体",12));//设置字体
    ui->tableView_Record->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);//设置大小与行内容匹配且鼠标不可拖拽
    ui->tableView_Record->setAlternatingRowColors(true);//隔行变色
    ui->tableView_Record->setModel(qmodel_Record);//将模型加载
    QSqlQuery sql;
    if(sql.exec(QString("select clock_type_name,kqz from stu;")))
    {
        while (sql.next())
        {
            //如果没有则插入
            if(ui->clock_type_box->findText(sql.value(0).toString())==-1)
            {
                ui->clock_type_box->addItem(sql.value(0).toString());
            }
            //如果没有则插入
            if(ui->clock_group_box->findText(sql.value(1).toString())==-1)
            {
                ui->clock_group_box->addItem(sql.value(1).toString());
            }
        }
    }
    ui->stackedWidget->setCurrentIndex(4);
}

//---------------- Voide ----------------------
//将视频线程对象传递的每一帧图像设置到视频显示窗口
void Widget::onFrameReady(QImage frame)
{
    // 在UI界面上显示视频的每一帧图像
    ui->video_Show->setPixmap(QPixmap::fromImage(frame));
}

//-------------- Clock in界面① ----------------
//btn-input 人脸录入按钮
void Widget::on_btn_input_clicked()
{
    QString xh=QInputDialog::getText(this,"人脸录入","学号:",QLineEdit::Normal,"210160XXXXX");
    if(xh==nullptr||xh=="210160XXXXX")
    {
        QMessageBox::warning(this,"人脸录入","请输入正确的学号!");
        return;
    }
    //学号正确进入下一步
    QString name=QInputDialog::getText(this,"人脸录入","姓名:",QLineEdit::Normal,"庞XX");
    if(name==nullptr||name=="庞XX")
    {
        QMessageBox::warning(this,"人脸录入","请输入正确的姓名!");
        return;
    }
    //姓名正确进入下一步
    QStringList groups;
    groups << tr("大一考勤组") << tr("大二考勤组")<<tr("大三考勤组");
    QString kqz = QInputDialog::getItem(this, "人脸录入", tr("请选择一个考勤组"), groups, 0, false);
    if (kqz.isEmpty()) {
        return;
    }
    qDebug()<<"获取的学号："<<xh<<" 姓名："<<name<<" 考勤组："<<kqz<<endl;
    //所有信息正确，插入数据库
    QSqlQuery sql;
    if(!sql.exec(QString("insert into stu(xh,name,kqz,clock_type_name) values('%1','%2','%3','日常考勤');").arg(xh).arg(name).arg(kqz)))
    {
        QMessageBox::information(this,"人脸录入","录入信息📕插入数据库失败😟!");
        return;
    }else {
        QMessageBox::information(this,"人脸录入","录入信息📕插入数据库成功🙌😃!\n\n准备拍照📷");
        //插入信息成功，将照片保存到人脸库
        qDebug()<<"input_xh:"<<xh;
        QString image_path=QString("../faceptr/%1.jpg").arg(xh);
        qDebug()<<"image_path:"<<image_path;
        m_videoThread->faceInput(image_path);
        m_videoThread->stop();
        QThread::sleep(1);
        m_videoThread->faceInit();
        m_videoThread->start_video();
    }
}

//------------- CheckVideo界面② ---------------
//btn-Retest 重新检测
void Widget::on_btn_Retest_clicked()
{
    checkVideo();
}
//btn-cap1 使用①号摄像头
void Widget::on_btn_cap1_open_clicked()
{
    if(ui->Check_video_cap_lbl->text()!="未检测到摄像头")
    {
        m_videoThread->stop();
        QThread::sleep(1);
        emit cap_chose_Changed(cap1_open);//开启摄像头①号
        QThread::sleep(1);
        ui->stackedWidget->setCurrentIndex(0);
    }
}
//btn-cap2 使用②号摄像头
void Widget::on_btn_cap2_open_clicked()
{
    if(ui->Check_video_cap_lbl_2->text()!="未检测到摄像头")
    {
        m_videoThread->stop();
        QThread::sleep(1);
        emit cap_chose_Changed(cap2_open);//开启摄像头②号
        QThread::sleep(1);
        ui->stackedWidget->setCurrentIndex(0);
    }
}
//btn-cap3 使用③号摄像头
void Widget::on_btn_cap3_open_clicked()
{
    if(ui->Check_video_cap_lbl_3->text()!="未检测到摄像头")
    {
        m_videoThread->stop();
        QThread::sleep(1);
        emit cap_chose_Changed(cap3_open);//开启摄像头③号
        QThread::sleep(1);
        ui->stackedWidget->setCurrentIndex(0);
    }
}

//--------------  Search界面③ ----------------
//btn_Sreach_exec 执行sql按钮
void Widget::on_btn_Search_exec_clicked()
{
    QSqlQuery sql;
    if(sql.exec(ui->Sreach_sql_line->text()))
    {
        if(ui->Sreach_sql_line->text().contains("select"))
        {
            qmodel->setQuery(ui->Sreach_sql_line->text());
        }else {
            qmodel->setQuery("select *from stu;");
        }
    }
    else
    {
        QMessageBox::warning(this,"执行sql",tr("执行SQL：\n%1\n失败！\n%2").arg(ui->Sreach_sql_line->text()).arg(sql.lastError().text()));
        return;
    }
}
//btn_Sreach_all 查询所有表
void Widget::on_btn_Search_all_clicked()
{
    QSqlQuery sql;
    if(sql.exec("select name FROM sqlite_master WHERE type='table';"))
    {
        qmodel->setQuery("select name FROM sqlite_master WHERE type='table';");
    }
}
//btn_Search_find 查找按钮
void Widget::on_btn_Search_find_clicked()
{
    QString type=ui->Sreach_find_type_box->currentText();
    QString msg=ui->Sreach_find_line->text();
    QString str;
    if(type=="按学号查找")
    {
        str=QString("select *from stu where xh='%1'").arg(msg);
    }
    else if (type=="按姓名查找")
    {
        str=QString("select *from stu where name='%1'").arg(msg);
    }
    else if(type=="按考勤组查找"){
        str=QString("select *from stu where kqz='%1'").arg(msg);
    }
    QSqlQuery sql;
    if(!sql.exec(str))
    {
        QMessageBox::information(this,"查询提示",tr("查询错误如下:\n%1").arg(sql.lastError().text()));
    }else
    {
        qDebug()<<str<<endl;
        qmodel->setQuery(str);
    }
}
//btn_Search_delete 删除按钮
void Widget::on_btn_Search_delete_clicked()
{
    QString type=ui->Sreach_find_type_box->currentText();
    QString msg=ui->Sreach_find_line->text();
    QString str;
    if(type=="按学号查找")
    {
        str=QString("delete from stu where xh='%1'").arg(msg);
    }
    else if (type=="按姓名查找")
    {
        str=QString("delete from stu where name='%1'").arg(msg);
    }
    else if(type=="按考勤组查找"){
        str=QString("delete from stu where kqz='%1'").arg(msg);
    }
    QSqlQuery sql;
    if(!sql.exec(str))
    {
        QMessageBox::information(this,"删除提示",tr("查询错误如下:\n%1").arg(sql.lastError().text()));
    }else
    {
        qDebug()<<str<<endl;
        qmodel->setQuery("select *from stu;");
    }
}

//-------------- Root界面④ --------------------
//btn_Root_Frist-首页按钮
void Widget::on_btn_Root_Frist_clicked()
{
    on_btn_root_clicked();
}
//btn_Root_update-更改信息按钮
void Widget::on_btn_Root_update_clicked()
{
    QSqlQuery sql;
    if(sql.exec(QString("select clock_type_name from clock_in;")))
    {
        //将所有考勤类型加载到comboBox
        while (sql.next()) {
            if(ui->groupBox_update_type_box->findText(sql.value(0).toString())==-1)
            {
                ui->groupBox_update_type_box->addItem(sql.value(0).toString());
                ui->groupBox_delete_type_box->addItem(sql.value(0).toString());
            }
        }
        ui->stackedWidget_root->setCurrentIndex(1);
    }
    else {
        QMessageBox::information(this,"更改信息按钮-提示","加载更改信息界面信息错误！");
    }

}
//下拉框-当前考勤类型-Changed事件
void Widget::on_Root_clock_type_box_currentIndexChanged(const QString &clock_type)
{
    QSqlQuery sql;
    if(sql.exec(QString("select clock_in_time,clock_out_time from clock_in where clock_type_name='%1';").arg(clock_type)))
    {
        sql.next();
        ui->Root_clock_in_time_lbl->setText(sql.value(0).toString());
        ui->Root_clock_out_time_lbl->setText(sql.value(1).toString());
    }
}
//btn_btn_Root_clock_type_update-首页->考勤类型确认更改按钮
void Widget::on_btn_Root_clock_type_update_clicked()
{
    ui->clock_in_type_name_lbl->setText(ui->Root_clock_type_box->currentText());
    ui->clock_in_time_start_lbl->setText(ui->Root_clock_in_time_lbl->text());
    ui->clock_in_time_end_lbl->setText(ui->Root_clock_out_time_lbl->text());
    ui->stackedWidget->setCurrentIndex(0);
}
//btn_Root_clock_group_update-首页->考勤组考勤类型确认更改按钮
void Widget::on_btn_Root_clock_group_update_clicked()
{
    QSqlQuery sql;
    if(sql.exec(QString("update stu set clock_type_name='%1' where kqz='%2'").arg(ui->Clock_group_1_box->currentText()).arg(ui->Clock_group_1_lbl->text())))
    {
        qDebug()<<ui->Clock_group_1_lbl->text()<<":修改成功！"<<endl;
    }
    if(sql.exec(QString("update stu set clock_type_name='%1' where kqz='%2'").arg(ui->Clock_group_2_box->currentText()).arg(ui->Clock_group_2_lbl->text())))
    {
        qDebug()<<ui->Clock_group_2_lbl->text()<<":修改成功！"<<endl;
    }
    if(sql.exec(QString("update stu set clock_type_name='%1' where kqz='%2'").arg(ui->Clock_group_3_box->currentText()).arg(ui->Clock_group_3_lbl->text())))
    {
        qDebug()<<ui->Clock_group_3_lbl->text()<<":修改成功！"<<endl;
    }
}
//btn_Root_Update_insert-更改信息->确认新增按钮
void Widget::on_btn_Root_Update_insert_clicked()
{
    if(ui->groupBox_insert_type_line->text()==nullptr||ui->groupBox_insert_start_line->text()==nullptr
            ||ui->groupBox_insert_end_line->text()==nullptr)
    {
        QMessageBox::information(this,"更改信息-新增考勤","请将信息填写完整！");
        return;
    }
    QSqlQuery sql;
    if(sql.exec(QString("insert into clock_in values('%1','%2','%3','%4',%5,%6,%7,%8,%9,%10,%11);")
                .arg(ui->groupBox_insert_type_line->text()).arg(ui->groupBox_insert_start_line->text())
                .arg(ui->groupBox_insert_end_line->text()).arg(ui->groupBox_insert_qt_type_box->currentText())
                .arg(ui->groupBox_insert_day1->isChecked()).arg(ui->groupBox_insert_day2->isChecked())
                .arg(ui->groupBox_insert_day3->isChecked()).arg(ui->groupBox_insert_day4->isChecked())
                .arg(ui->groupBox_insert_day5->isChecked()).arg(ui->groupBox_insert_day6->isChecked())
                .arg(ui->groupBox_insert_day7->isChecked())))
    {
        on_btn_Root_update_clicked();
        QMessageBox::information(this,"更改信息-新增考勤",tr("新增考勤类型%1成功！").arg(ui->groupBox_insert_type_line->text()));
        ui->groupBox_insert_type_line->clear();
        ui->groupBox_insert_start_line->clear();
        ui->groupBox_insert_end_line->clear();
        ui->groupBox_insert_day1->setChecked(0);
        ui->groupBox_insert_day2->setChecked(0);
        ui->groupBox_insert_day3->setChecked(0);
        ui->groupBox_insert_day4->setChecked(0);
        ui->groupBox_insert_day5->setChecked(0);
        ui->groupBox_insert_day6->setChecked(0);
        ui->groupBox_insert_day7->setChecked(0);
    }
    else {
        QMessageBox::warning(this,"更改信息-新增考勤",tr("新增考勤类型%1失败！").arg(ui->groupBox_insert_type_line->text()));
    }
}
//btn_Root_Update_update-更改信息->确认修改按钮
void Widget::on_btn_Root_Update_update_clicked()
{
    if(ui->groupBox_update_start_line->text()==nullptr||ui->groupBox_update_end_line->text()==nullptr)
    {
        QMessageBox::information(this,"更改信息-修改考勤","请将信息填写完整！");
        return;
    }
    QSqlQuery sql;
    if(sql.exec(QString("update clock_in set clock_in_time='%1',clock_out_time='%2',clock_out_type='%3',one=%4,two=%5,three=%6,four=%7,five=%8,six=%9,seven=%10 where clock_type_name='%11';")
                .arg(ui->groupBox_update_start_line->text()).arg(ui->groupBox_update_end_line->text())
                .arg(ui->groupBox_update_qt_type_box->currentText()).arg(ui->groupBox_update_day1->isChecked())
                .arg(ui->groupBox_update_day2->isChecked()).arg(ui->groupBox_update_day3->isChecked())
                .arg(ui->groupBox_update_day4->isChecked()).arg(ui->groupBox_update_day5->isChecked())
                .arg(ui->groupBox_update_day6->isChecked()).arg(ui->groupBox_update_day7->isChecked())
                .arg(ui->groupBox_update_type_box->currentText())))

    {
        if(ui->clock_in_type_name_lbl->text()==ui->groupBox_update_type_box->currentText())
        {
            ui->clock_in_time_start_lbl->setText(ui->groupBox_update_start_line->text());
            ui->clock_in_time_end_lbl->setText(ui->groupBox_update_end_line->text());
        }
        QMessageBox::information(this,"更改信息-修改考勤",tr("修改考勤%1成功！").arg(ui->groupBox_update_type_box->currentText()));
        QSqlQuery sql;
        if(sql.exec(tr("select *from clock_in where clock_type_name='%1';").arg(ui->clock_in_type_name_lbl->text())))
        {
            sql.next();
            ui->clock_in_time_start_lbl->setText(sql.value(1).toString());
            ui->clock_in_time_end_lbl->setText(sql.value(2).toString());
            ui->clock_in_type_auto_lbl->setText(sql.value(3).toString()+"签退");
        }
        sql.clear();
    }
    else {
        QMessageBox::warning(this,"更改信息-修改考勤",tr("修改考勤%1失败！").arg(ui->groupBox_update_type_box->currentText()));
    }

}
//groupBox_update_type_box-更改信息-修改-下拉框-考勤类型-Changed事件
void Widget::on_groupBox_update_type_box_currentIndexChanged(const QString &type)
{
    QSqlQuery sql;
    if(sql.exec(QString("select *from clock_in where clock_type_name='%1'").arg(type)))
    {
        sql.next();
        ui->groupBox_update_start_line->setText(sql.value(1).toString());
        ui->groupBox_update_end_line->setText(sql.value(2).toString());
        ui->groupBox_update_qt_type_box->setCurrentText(sql.value(3).toString());
        ui->groupBox_update_day1->setChecked(sql.value(4).toInt());
        ui->groupBox_update_day2->setChecked(sql.value(5).toInt());
        ui->groupBox_update_day3->setChecked(sql.value(6).toInt());
        ui->groupBox_update_day4->setChecked(sql.value(7).toInt());
        ui->groupBox_update_day5->setChecked(sql.value(8).toInt());
        ui->groupBox_update_day6->setChecked(sql.value(9).toInt());
        ui->groupBox_update_day7->setChecked(sql.value(10).toInt());
    }
}
//btn_Root_Update_update-更改信息->确认删除按钮
void Widget::on_btn_Root_Update_delete_clicked()
{
    QSqlQuery sql;
    if(sql.exec(QString("delete from clock_in where clock_type_name='%1'").arg(ui->groupBox_delete_type_box->currentText())))
    {
        //更新所有下拉框
        ui->groupBox_update_type_box->removeItem(ui->groupBox_delete_type_box->currentIndex());
        ui->Root_clock_type_box->removeItem(ui->groupBox_delete_type_box->currentIndex());
        ui->Clock_group_1_box->removeItem(ui->groupBox_delete_type_box->currentIndex());
        ui->Clock_group_2_box->removeItem(ui->groupBox_delete_type_box->currentIndex());
        ui->Clock_group_3_box->removeItem(ui->groupBox_delete_type_box->currentIndex());
        ui->groupBox_delete_type_box->removeItem(ui->groupBox_delete_type_box->currentIndex());
        QMessageBox::information(this,"更改信息-删除考勤",tr("考勤类型:%1删除成功！").arg(ui->groupBox_delete_type_box->currentText()));
    }
    else {
        QMessageBox::warning(this,"更改信息-删除考勤",tr("考勤类型:%1删除失败！").arg(ui->groupBox_delete_type_box->currentText()));
    }
}
//btn_Root_0-一键置0按钮
void Widget::on_btn_Root_0_clicked()
{
    QSqlQuery sql;
    if(sql.exec(QString("update stu set is_clock_in=0,is_clock_out=0;")))
    {
        qDebug()<<"一键置为0成功！"<<endl;
        // 创建文件对象
        QFile file("../log_clock_in.txt");

        // 打开文件，以写入模式打开
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qDebug() << "无法打开文件";
        }
        // 创建文本流对象
        QTextStream out(&file);
        // 写入文本内容
        out <<ui->time_lbl->text()<<"结算打卡成功\n";
        // 关闭文件
        file.close();
        qDebug() << "文件保存成功";
        return;
    }
    else
    {
        // 创建文件对象
        QFile file("../log_clock_in.txt");

        // 打开文件，以写入模式打开
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qDebug() << "无法打开文件";
        }
        // 创建文本流对象
        QTextStream out(&file);
        // 写入文本内容
        out <<ui->time_lbl->text()<<"结算打卡失败!!!!!!\n";
        // 关闭文件
        file.close();
        qDebug() << "文件保存成功";
    }

}

//------------ Record记录 界面⑤ ---------------
//clock_type_box-下拉框-考勤类型-Changed事件
void Widget::on_clock_type_box_currentIndexChanged(const QString &type)
{
    QString str;
    if(type=="全部"&&ui->clock_group_box->currentText()!="全部")
    {
        str= QString("select *from record where and kqz='%1' and DATE(clock_in_time)='%2';").arg(ui->clock_group_box->currentText()).arg(ui->dateEdit->dateTime().toString("yyyy-MM-dd"));
    }
    else if (type!="全部"&&ui->clock_group_box->currentText()=="全部") {
        str= QString("select *from record where clock_type_name='%1'  and DATE(clock_in_time)='%2';").arg(type).arg(ui->dateEdit->dateTime().toString("yyyy-MM-dd"));
    }
    else if (type=="全部"&&ui->clock_group_box->currentText()=="全部") {
        str= QString("select *from record where DATE(clock_in_time)='%1';").arg(ui->dateEdit->dateTime().toString("yyyy-MM-dd"));
    }
    else {
        str= QString("select *from record where clock_type_name='%1' and kqz='%2' and DATE(clock_in_time)='%3';").arg(type).arg(ui->clock_group_box->currentText()).arg(ui->dateEdit->dateTime().toString("yyyy-MM-dd"));
    }
    QSqlQuery sql;
    if(sql.exec(str))
    {
        qmodel_Record->setQuery(str);
    }
}
//clock_group_box-下拉框-考勤组-Changed事件
void Widget::on_clock_group_box_currentIndexChanged(const QString &group)
{
    QString str;
    if(ui->clock_type_box->currentText()=="全部"&&group=="全部")
    {
        str= QString("select *from record where DATE(clock_in_time)='%1';").arg(ui->dateEdit->dateTime().toString("yyyy-MM-dd"));
    }
    else if (ui->clock_type_box->currentText()!="全部"&&group=="全部") {
        str= QString("select *from record where clock_type_name='%1' and DATE(clock_in_time)='%2';").arg(ui->clock_type_box->currentText()).arg(ui->dateEdit->dateTime().toString("yyyy-MM-dd"));
    }
    else if (ui->clock_type_box->currentText()=="全部"&&group!="全部") {
        str= QString("select *from record where kqz='%1' and DATE(clock_in_time)='%2';").arg(group).arg(ui->dateEdit->dateTime().toString("yyyy-MM-dd"));
    }
    else {
        str= QString("select *from record where clock_type_name='%1' and kqz='%2' and DATE(clock_in_time)='%3';").arg(ui->clock_type_box->currentText()).arg(group).arg(ui->dateEdit->dateTime().toString("yyyy-MM-dd"));
    }
    QSqlQuery sql;
    if(sql.exec(str))
    {
        qmodel_Record->setQuery(str);
    }
}
//dateEdit-时间选择-Changed事件
void Widget::on_dateEdit_dateTimeChanged(const QDateTime &dateTime)
{
    QString str;
    if(ui->clock_type_box->currentText()=="全部"&&ui->clock_group_box->currentText()!="全部")
    {
        str= QString("select *from record where  kqz='%1' and DATE(clock_in_time)='%2';").arg(ui->clock_group_box->currentText()).arg(dateTime.toString("yyyy-MM-dd"));
    }
    else if (ui->clock_type_box->currentText()!="全部"&&ui->clock_group_box->currentText()=="全部") {
        str= QString("select *from record where clock_type_name='%1' and DATE(clock_in_time)='%2';").arg(ui->clock_type_box->currentText()).arg(dateTime.toString("yyyy-MM-dd"));
    }
    else if (ui->clock_type_box->currentText()=="全部"&&ui->clock_group_box->currentText()=="全部") {
        str= QString("select *from record where DATE(clock_in_time)='%1';").arg(dateTime.toString("yyyy-MM-dd"));
    }
    else {
        str= QString("select *from record where clock_type_name='%1' and kqz='%2' and DATE(clock_in_time)='%3';").arg(ui->clock_type_box->currentText()).arg(ui->clock_group_box->currentText()).arg(dateTime.toString("yyyy-MM-dd"));
    }
    QSqlQuery sql;
    if(sql.exec(str))
    {
        qmodel_Record->setQuery(str);
    }
}
//btn_clock_record_name_search姓名搜索按钮
void Widget::on_btn_clock_record_name_search_clicked()
{
    QString type=ui->clock_type_box->currentText();
    QString group=ui->clock_group_box->currentText();
    QString time=ui->dateEdit->dateTime().toString("yyyy-MM-dd");
    QString name=ui->clock_record_name_line->text();
    QString str;
    if(name==nullptr)
    {
        QMessageBox::information(this,"考勤记录-提示","搜索的人姓名不可以为空！");
        return;
    }
    else
    {
        if(type=="全部"&&group!="全部")
        {
            str= QString("select *from record where kqz='%1' and DATE(clock_in_time)='%2' and name='%3';").arg(group).arg(time).arg(name);
        }
        else if (type!="全部"&&group=="全部") {
            str= QString("select *from record where clock_type_name='%1'  and DATE(clock_in_time)='%2' and name='%3';").arg(type).arg(time).arg(name);
        }
        else if (type=="全部"&&group=="全部") {
            str= QString("select *from record where DATE(clock_in_time)='%1' and name='%2';").arg(time).arg(name);
        }
        else {
            str= QString("select *from record where clock_type_name='%1' and kqz='%2' and DATE(clock_in_time)='%3' and name='%4';").arg(type).arg(group).arg(time).arg(name);
        }
    }
    QSqlQuery sql;
    if(sql.exec(str))
    {
        qmodel_Record->setQuery(str);
    }
    else {
        qDebug()<<"error:"<<str<<endl;
    }

}
//btn_look_record 查看打卡照片按钮
void Widget::on_btn_look_record_clicked()
{
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString path="../clock_in/"+currentDateTime.toString("yyyy-MM-dd");
    QUrl url = QUrl::fromLocalFile(path);
    QDesktopServices::openUrl(url);
}
//btn_look_record_all 查看所有记录按钮
void Widget::on_btn_look_record_all_clicked()
{
    qmodel_Record->setQuery(QString("select * from record;"));
}
