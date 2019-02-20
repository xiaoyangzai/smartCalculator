#ifndef PTHREADVIDEOCAPTURE_H
#define PTHREADVIDEOCAPTURE_H

#include <QThread>
#include "widget.h"

class pthreadVideoCapture : public QThread
{
    Q_OBJECT
public:
    explicit pthreadVideoCapture(Widget *pw,QObject *parent = 0);
    void run();
signals:
    
public slots:
private:
    Widget *pw;
    
};

#endif // PTHREADVIDEOCAPTURE_H
