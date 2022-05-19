#ifndef MARATHONCAN_H
#define MARATHONCAN_H

#include <QObject>

#include "chai.h"

#include <QTimer>

namespace Bulat {

    enum Board {
        TableT = 0x02, // стол
        Power  = 0x04, // плата подсветки, после сдвига = 00100
        LaserT = 0x0f, // ТВЕРДОТЕЛЬНЫЙ ЛАЗЕР
    };

    enum MSG {
        COMMANDW = 0x00, // формирование комманды для любой платы
        STATUSW  = 0x02, // ожидаем из сети CAN
        STATUSR  = 0x03  // записываем в CAN чтобы узнать статус платы
    };
}

class MarathonCAN : public QObject
{
    Q_OBJECT

    qint8 m_openChannelNumber;

    QTimer *m_pCheckCanTimer;

public:
    explicit MarathonCAN(QObject *parent = nullptr);

signals:

private:
    void init() ;

    void printInfo();

    void openChannel();
    bool setBaud();
    void startChannel();

    void scanBoards();

    void waitEvent();
    void readFrames();

    void parseMessage( const canmsg_t &frame );
    void parseMessageLaserT( const canmsg_t &frame );
    void parseMessageTableT( const canmsg_t &frame );

    void stop();

    // cбрасывает текущий запрос на передачу и очищает очередь на отправку
    void trCancel();

    // принудительно очищает (стирает) содержимое приемной очереди канала
    void rcQueCancel();
};

#endif // MARATHONCAN_H
