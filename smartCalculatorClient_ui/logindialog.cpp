#include <QRegExp>
#include <QPushButton>
#include <QRegExpValidator>
#include "common.h"
#include "jpeg_util.h"
#include "video_util.h"
#include "memory_pool.h"
#include "logindialog.h"
#include "ui_logindialog.h"
#include "widget.h"

loginDialog::loginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::loginDialog)
{
    ui->setupUi(this);
    QRegExp rx("^((2[0-4]\\d|25[0-5]|[01]?\\d\\d?)\\.){3}(2[0-4]\\d|25[0-5]|[01]?\\d\\d?)$");

    QRegExpValidator *m_IPValidator = new QRegExpValidator(rx, this);

    ui->ple_IP->setValidator(m_IPValidator);
    ptr = ui->ple_IP;
    ui->ple_IP->installEventFilter(this);
    ui->ple_Port->installEventFilter(this);
    ui->ple_deviceid->installEventFilter(this);
    ui->ple_IP->setText(QString("202.117.43.212"));
    ui->ple_Port->setText(QString("8888"));
    ui->ple_deviceid->setText(QString("0001"));

}

loginDialog::~loginDialog()
{
    delete ui;
}

void loginDialog::on_pbtn_return_clicked()
{
    this->ui->ple_deviceid->clear();
    this->ui->ple_IP->clear();
    ui->ple_Port->clear();
}

void loginDialog::on_pbtn_saveopt_clicked()
{
    this->close();
}

void loginDialog::on_pbtn_clear_clicked()
{
    ptr->clear();
}

void loginDialog::on_pbtn_backspace_clicked()
{
    ptr->backspace();
}

void loginDialog::on_pushButton_1_clicked()
{
    ptr->setText(ptr->text() + "1");
}

void loginDialog::on_pushButton_2_clicked()
{
    ptr->setText(ptr->text() + "2");
}

void loginDialog::on_pushButton_3_clicked()
{
    ptr->setText(ptr->text() + "3");
}

void loginDialog::on_pushButton_4_clicked()
{
    ptr->setText(ptr->text() + "4");
}

void loginDialog::on_pushButton_5_clicked()
{
    ptr->setText(ptr->text() + "5");
}

void loginDialog::on_pushButton_6_clicked()
{
    ptr->setText(ptr->text() + "6");
}

void loginDialog::on_pushButton_7_clicked()
{
    ptr->setText(ptr->text() + "7");
}

void loginDialog::on_pushButton_8_clicked()
{
    ptr->setText(ptr->text() + "8");
}

void loginDialog::on_pushButton_9_clicked()
{
    ptr->setText(ptr->text() + "9");
}

void loginDialog::on_pushButton_0_clicked()
{
    ptr->setText(ptr->text() + "0");
}

void loginDialog::on_pushButton_10_clicked()
{
    ptr->setText(ptr->text() + ".");
}
bool loginDialog::eventFilter(QObject *obj, QEvent *event)
{
    if(obj == ui->ple_IP && event->type() == QEvent::MouseButtonPress)
    {
        ptr = ui->ple_IP;

        return true;
    }
    else if(obj == ui->ple_Port && event->type() == QEvent::MouseButtonPress)
    {
        ptr = ui->ple_Port;
        return true;
    }
    else if(obj == ui->ple_deviceid && event->type() == QEvent::MouseButtonPress)
    {
        ptr = ui->ple_deviceid;
        return true;
    }
    else
        return QDialog::eventFilter(obj,event);
}

void loginDialog::on_pbtn_login_clicked()
{
    if(ui->ple_deviceid->text().isEmpty() || ui->ple_IP->text().isEmpty() || ui->ple_Port->text().isEmpty())
        return;
    int deviceid = ui->ple_deviceid->text().toInt();
    short port = ui->ple_Port->text().toShort();
    const char *server_ip = ui->ple_IP->text().toAscii().data();
    if(::inet_addr(server_ip) == INADDR_NONE)
    {
        ui->pla_statu->setText(QString("Ip Address invaild"));
        return;
    }
    ui->pla_statu->setText(QString("Starting connect to the server[%1]...").arg(ui->ple_IP->text()));
	//打开电子秤设备
    int balance_fd = ::open("/dev/hx711",O_RDONLY);
    if(balance_fd < 0)
	{
        ui->pla_statu->setText(QString("open balance device failed: %1").arg(::strerror(errno)));
        return;
	}
    mw = new Widget(ui->ple_IP->text().toAscii().data(),port,balance_fd,deviceid);
    mw->show();
    this->close();
}
