#ifndef PTHREADVIDEOCLASSIFY_H
#define PTHREADVIDEOCLASSIFY_H

#include <QThread>
#include "widget.h"

class pthreadVideoClassify : public QThread
{
    Q_OBJECT
public:
    explicit pthreadVideoClassify(Widget* pw,QObject *parent = 0);
    void run();
signals:
    
public slots:


private:
    Widget *pw;
    
};

#endif // PTHREADVIDEOCLASSIFY_H
