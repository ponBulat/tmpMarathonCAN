#include "marathoncan.h"

#include <QDebug>
#include <QBitArray>

MarathonCAN::MarathonCAN(QObject *parent)
    : QObject{parent}
    , m_openChannelNumber{ 0 }
    , m_pCheckCanTimer{ nullptr }
{
    init();
}

void MarathonCAN::init()
{
//    qDebug() << "MarathonCAN ThreadId: " << QThread::currentThread();

    int ret = CiInit();

    if( ret )
        qWarning() << " library initialization failed ";
    else
    {
        qInfo() << "CHAI library successfully initialized";
        printInfo();
        openChannel();
    }
}

void MarathonCAN::printInfo()
{
    qInfo() << "=========== Marathon devices info ===============";

    canboard_t binfo; // структура с информацией о плате

    // CI_BRD_NUMS - максимальное количество плат
    for ( _u8 i = 0; i < CI_BRD_NUMS; i++) {
        binfo.brdnum = i; // номер платы
        auto ret = CiBoardInfo(&binfo);

        // если ошибка, то выведем их в консоль
        if (ret < 0)
        {
            switch ( ret * -1 ) {
                case ECIINVAL : // это не ошибка, это просто отсутствие плат
                    qWarning().noquote() << "The board number (" << i << ") passed as a parameter is out of the supported number of boards";
                    break;
                case ECINODEV :
                    qWarning().noquote() << "The board number ("<< i <<") passed as a parameter does not correspond to any of the detected boards";
                    break;
                case ECIMFAULT :
                    qWarning().noquote() << "System memory error (unable to copy parameters or results). Board number "<< i;
                    break;
                case ECIBUSY :
                    qWarning().noquote() << "The call cannot be made because all channels are occupied by other processes. Board number "<< i;
                    break;
                default :
                    qWarning() << "Unknow error. Errorcode: " << ret;
           }

            continue;
        }

        // если ошибок нет, то выведем информацию о плате
        // ----------------------------------------------

        qInfo().noquote() << binfo.name << '(' << binfo.manufact << "): ";

        // binfo.chip - массив номеров каналов
        for ( _u16 j = 0; j < 4; j++) {
            if (binfo.chip[j] >= 0) {
                qInfo() << '\t' << "channel " << binfo.chip[j];
            }
        }
    }

    qInfo() << "======================================" << Qt::endl;
}

void MarathonCAN::openChannel()
{

    const auto ret = CiOpen( m_openChannelNumber, static_cast<_u8>( CIO_CAN11 ) );

    if( ret ) {
        qWarning() << "error opening of " + QString::number( m_openChannelNumber ) + " channel ";
    } else {
        if( setBaud() ) {
            qInfo() << "Channel " << QString::number( m_openChannelNumber ) << " openned";
            startChannel();
        }
    }
}

bool MarathonCAN::setBaud( )
{
    // пока только 125
    auto ret = CiSetBaud( m_openChannelNumber, BCI_125K );

    if( ret ) {
        qWarning() << "CiSetBaud set baud 125K ";

//        closeChannel();
        return false;
    } else {
        qInfo().nospace() << "Channel " << QString::number( m_openChannelNumber ) << " set BCI_125K";
        return true;
    }
}

void MarathonCAN::startChannel( )
{
    auto ret = CiStart( m_openChannelNumber );

    if( ret ) {
        qWarning() << "error starting of " + QString::number( m_openChannelNumber ) + " channel ";
        stop();
    } else {
        // запустим чтение данных из сети CAN по таймеру
        m_pCheckCanTimer = new QTimer( this );
        m_pCheckCanTimer->setInterval(15);
        connect( m_pCheckCanTimer, &QTimer::timeout, [this](){ waitEvent(); } );
        m_pCheckCanTimer->start(10);

        qInfo() << "Channel " << QString::number( m_openChannelNumber ) << " started";

        scanBoards();
    }
}

void MarathonCAN::scanBoards()
{
    canmsg_t can_msg_tx_LaserT;
    canmsg_t can_msg_tx_TableT;

    // Обнуляет кадры
    msg_zero( &can_msg_tx_LaserT );
    msg_zero( &can_msg_tx_TableT );
    // После вызова can_msg_tx_ представляет собой кадр стандартного формата (SFF - standart frame format,
    // идентификатор - 11 бит), длина поля данных - ноль, данные и все остальные поля равны нулю

    /* generate tx CAN message */
    can_msg_tx_LaserT.id = ( Bulat::Board::LaserT << 6 | Bulat::MSG::STATUSR  );
    can_msg_tx_TableT.id = ( Bulat::Board::TableT << 6 | Bulat::MSG::STATUSR  );


    const int messageCount{ 2 };

    // создадим массив для серии CAN-кадров
    canmsg_t mbuf[ messageCount ];
    mbuf[ 0 ] = can_msg_tx_LaserT;
    mbuf[ 1 ] = can_msg_tx_TableT;

    // переменная для возможных ошибок
    int chaierr{0};

    // отправим серию кадров в сеть через очередь на отправку
    auto writedFrames = CiTransmitSeries( m_openChannelNumber, mbuf, messageCount, &chaierr );
    // writedFrames -  количество успешно записанных в очередь кадров,
    // может быть меньше чем запрошенное количество cnt

    // chaierr == 0 это успешное выполнение
    if( chaierr ) {
        qWarning() << "Error write series of " + QString::number( m_openChannelNumber ) + " channel";
    } else if( writedFrames < messageCount ) {
        // если количество записанных кадров меньше чем количество кадров в массиве на запись, то беда
        qWarning().noquote() << "Write in " + QString::number( m_openChannelNumber ) + " channel "
                           + QString::number( writedFrames ) + " of  " + QString::number( messageCount );
    }
    else
        qInfo() << "write " << QString::number( m_openChannelNumber ) << " channel success.  canBusFrame Series ";
}

void MarathonCAN::waitEvent()
{
    // сразу установим флаги интересующих нас событий
    // при получении данных с использованием canWaitEvent
    canwait_t canwait;
    canwait.chan = m_openChannelNumber;
    canwait.wflags = CI_WAIT_RC | CI_WAIT_ER /*| CI_WAIT_TR*/;

    auto ret = CiWaitEvent( &canwait, 1, 10 );

    if( ret < 0 ) {
        switch ( ret * -1 ) {
        case ECIINVAL  : {
            qWarning() << "CiWaitEvent ECIINVAL";  break;
        }
        case ECIMFAULT : qWarning() << "CiWaitEvent ECIMFAULT"; break;
        case ECIGEN    :  qWarning() << "CiWaitEvent ECIGEN";    break;
        }

        qWarning() << "Error WaitEvent of " + QString::number( m_openChannelNumber ) + " channel ";
    }
    else if ( ret > 0 ) {
        if( canwait.rflags & CI_WAIT_RC )
        {
            readFrames();
        }

        if( canwait.rflags & CI_WAIT_ER )
        {
            qWarning() << "error";
        }

        if( canwait.rflags & CI_WAIT_TR )
        {
            qWarning() << "retWaitEvent CI_WAIT_TR!";

            // выведем значение порога очереди на отправку
            {
                _u16 thres = 0;
                const auto res = CiTrQueThreshold( m_openChannelNumber, CI_CMD_GET, &thres );

                if( res ) {
                    qWarning() << "Error CiTrQueThreshold( res: " << res;
                } else {
                    qInfo() << "Transmit queue threshold value: " << thres;
                }
            }

//                 выведем текущее состояние процесса отправки кадров канала ввода-вывода.
            {
                _u16 trqcnt = 0;
                const auto res = CiTrStat( m_openChannelNumber, &trqcnt );

                if( res ) {
                    switch ( res ) {
                    case CI_TR_INCOMPLETE : qInfo() << "the controller transmits the frame to the network"; break;
                    case CI_TR_COMPLETE_OK : qInfo() << "the last transfer to the network was successful"; break;
                    case CI_TR_COMPLETE_ABORT : qInfo() << "the last network transfer was dropped";
                    }

                } else {
                    qWarning() << "Error CiTrStat res: " << res;
                }

                qDebug() << "the number of frames in the queue for sending: " << trqcnt;
            }
        }
    }
//    else
//        qDebug() << "timeout";

}

void MarathonCAN::readFrames()
{
//    qDebug() << "";

    // Возвращает количество кадров находящихся в приемной очереди драйвера
    _u16 rcqcnt = 0;
    const auto answer = CiRcQueGetCnt( m_openChannelNumber, &rcqcnt );

    if( answer ) {
        qWarning() << " MarathonCAN::oneFrameRead CiRcQueGetCnt answer: " << answer;
    } else {
        canmsg_t frames[ rcqcnt ];
        auto ret = CiRead( m_openChannelNumber, frames, rcqcnt );

        if( ret >= 0 )
        {
            for( const auto frame : frames  )
                parseMessage( frame );
        }
        else
            qWarning() << "CiRead errorcode: " << ret;
    }
}

void MarathonCAN::parseMessage( const canmsg_t &frame )
{
    qInfo() << "\nNew message:";
    const auto boardName = frame.id >> 6;
    switch ( boardName ) {
        case Bulat::Board::LaserT :
            parseMessageLaserT( frame );
            break;
        case Bulat::Board::TableT :
            parseMessageTableT( frame );
            break;
        default:
            break;
    }
}

void MarathonCAN::parseMessageLaserT( const canmsg_t &frame )
{
    qInfo() << "Board: LaserT";

    // маска 0x3f это 000 0011 1111
    // в предположении, что id 11-bit
    const auto idWithoutName = frame.id & 0x3f;

    switch ( idWithoutName ) {
    case Bulat::MSG::COMMANDW : {
        // ничего не делаем
        break;
    }
    case Bulat::MSG::STATUSW : {

        // ожидаем 4 байт
        const QBitArray status{ QBitArray::fromBits( reinterpret_cast<const char*>( frame.data ), frame.len * 8 ) };

        qDebug() << "on: " << status.testBit( 0 );
        qDebug() << "studio: " << status.testBit( 1 );

        break;
    }
    case Bulat::MSG::STATUSR : {
        // ничего не делаем
        break;
    }
    }
}

void MarathonCAN::parseMessageTableT( const canmsg_t &frame )
{
    qInfo() << "Board: TableT";

    // маска 0x3f это 000 0011 1111
    // в предположении, что id 11-bit
    const auto idWithoutName = frame.id & 0x3f;

    switch ( idWithoutName ) {
        case Bulat::MSG::COMMANDW : {
            // ничего не делаем
            break;
        }
        case Bulat::MSG::STATUSW : {

            // проверим поле статус и студия в первом байте
            const QBitArray status{ QBitArray::fromBits( reinterpret_cast<const char*>( frame.data ), 8 ) };

            qDebug() << "on: " << status.testBit( 0 );
            qDebug() << "studio: " << status.testBit( 1 );

            break;
        }
        case Bulat::MSG::STATUSR : {
            // ничего не делаем
            break;
        }
    }
}

void MarathonCAN::stop()
{
    // остановим таймер высылки данных в CAN
    m_pCheckCanTimer->stop();

    // cбрасываем текущий запрос на передачу и очищаем очередь на отправку
    trCancel();

    // принудительно очищает (стирает) содержимое приемной очереди канала
    rcQueCancel();

    auto ret = CiStop( m_openChannelNumber );

    if( ret )
        qWarning() << "Error stopping of " + QString::number( m_openChannelNumber ) + " channel ";
    else
    {
        qInfo() << "Channel " << m_openChannelNumber << " stopped";


        auto ret = CiClose( m_openChannelNumber );

        if( ret )
            qWarning() << "Error closing of " + QString::number( m_openChannelNumber ) + " channel ";
        else
            qInfo() << "Channel " << m_openChannelNumber << " closed";
    }
}

void MarathonCAN::trCancel()
{
    _u16 trqcnt = 0;
    const auto ret = CiTrCancel( m_openChannelNumber, &trqcnt );

    if( ret > 0 ) {
        QString answer;

        switch (ret) {
        case CI_TRCANCEL_NOTRANSMISSION : answer = "no current transfer request";
            break;
        case CI_TRCANCEL_ABORTED : answer = "the current transfer request has been dropped"
                                            "(no frame sent to the network)";
            break;
        case CI_TRCANCEL_TRANSMITTED : answer = "the current transfer request has been dropped"
                                                "(no frame sent to the network)";
            break;
        }
        qInfo() << "Transmit queue cleared. CiTrCancel answer: " << answer << ". Number of frames erased: " << trqcnt;
    }
    // если меньше нуля, то ошибка
    else if( ret < 0 )
        qWarning() << "CiTrCancel of " + QString::number( m_openChannelNumber ) + " channel ";
}

void MarathonCAN::rcQueCancel()
{
    _u16 rcqcnt = 0;
    const auto ret = CiRcQueCancel( m_openChannelNumber, &rcqcnt );

    if( ret )
        qWarning() << "Error CiRcQueCancel of " + QString::number( m_openChannelNumber ) + " channel ";
    else
        qInfo() << "Reception queue cleared. Number of frames erased: " << rcqcnt;
}


