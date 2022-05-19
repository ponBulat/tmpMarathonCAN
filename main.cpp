#include <QCoreApplication>

#include <marathoncan.h>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    MarathonCAN marathonCAN;

    return a.exec();
}
