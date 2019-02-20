#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include "widget.h"

namespace Ui {
class loginDialog;
}

class loginDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit loginDialog(QWidget *parent = 0);
    ~loginDialog();
protected:
    bool eventFilter(QObject *obj, QEvent *event);
    
private slots:
    void on_pbtn_return_clicked();

    void on_pbtn_saveopt_clicked();

    void on_pbtn_clear_clicked();

    void on_pbtn_backspace_clicked();

    void on_pushButton_1_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void on_pushButton_4_clicked();

    void on_pushButton_5_clicked();

    void on_pushButton_6_clicked();

    void on_pushButton_7_clicked();

    void on_pushButton_8_clicked();

    void on_pushButton_9_clicked();

    void on_pushButton_0_clicked();

    void on_pushButton_10_clicked();


    void on_pbtn_login_clicked();

private:
    Ui::loginDialog *ui;
    QLineEdit *ptr;
    Widget *mw;
};

#endif // loginDialog_H
