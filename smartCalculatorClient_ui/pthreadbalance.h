#ifndef PTHREADBALANCE_H
#define PTHREADBALANCE_H

#include <QThread>
#include "common.h"
#include "widget.h"

class PthreadBalance : public QThread
{
    Q_OBJECT
public:
    explicit PthreadBalance(int balance_fd,QObject *parent = 0);
    void run();
signals:
    void signal_msg(QString s);
    void signal_update_weight(float weight);
public slots:
private:
    int balance_fd;
    
};

#endif // PTHREADBALANCE_H
