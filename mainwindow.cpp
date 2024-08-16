#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtSerialPort/QSerialPortInfo>
#include <QtWidgets/QFileDialog>
#include <QtNetwork/QNetworkDatagram>
#include <QtCore/QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    serial = nullptr;
    tcpClient = nullptr;
    rxBroadcastGVRET = nullptr;

    refreshSerialList();

    connect(ui->btnConnect, &QPushButton::clicked, this, &MainWindow::handleConnectButton);
    connect(ui->btnClear, &QPushButton::clicked, this, &MainWindow::handleClearButton);
    connect(ui->btnSave, &QPushButton::clicked, this, &MainWindow::handleSaveButton);
    connect(ui->lineSend, &QLineEdit::editingFinished, this, &MainWindow::handleSendText);

    serialRefreshTimer.setInterval(5000);
    serialRefreshTimer.setTimerType(Qt::CoarseTimer);
    serialRefreshTimer.start();
    connect(&serialRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshSerialList);

    rxBroadcastGVRET = new QUdpSocket(this);
    //Need to make sure it tries to share the address in case there are
    //multiple instances of SavvyCAN running.
    rxBroadcastGVRET->bind(QHostAddress::AnyIPv4, 17222, QAbstractSocket::ShareAddress);
    connect(rxBroadcastGVRET, &QUdpSocket::readyRead, this, &MainWindow::readPendingDatagrams);

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::handleClearButton()
{
    ui->txtMainView->clear();
}

void MainWindow::refreshSerialList()
{
    qDebug() << "Ping!";

    QList<QSerialPortInfo> ports;

    ports = QSerialPortInfo::availablePorts();

    ui->listPorts->clear();

    foreach(QString IP, remoteDeviceIPGVRET.keys())
    {
        QString name = remoteDeviceIPGVRET.value(IP);
        ui->listPorts->addItem(IP + "  [" + name + "]");
    }

    for (int i = 0; i < ports.count(); i++)
        ui->listPorts->addItem(ports[i].portName());
}
void MainWindow::handleConnectButton()
{
    QString portName = ui->listPorts->currentItem()->text();
    serial = new QSerialPort(QSerialPortInfo(portName));
    if(!serial) {
        qDebug() << ("can't open serial port " + portName);
        return;
    }
    qDebug() << ("Created Serial Port Object");

    /* connect reading event */
    connect(serial, SIGNAL(readyRead()), this, SLOT(readSerialData()));
    //connect(serial, SIGNAL(error(QSerialPort::SerialPortError)), this, SLOT(serialError(QSerialPort::SerialPortError)));

    int speed = 115200;

    if (ui->rbMega->isChecked()) speed = 1000000;

    /* configure */
    serial->setBaudRate(speed);
    serial->setDataBits(serial->Data8);

    serial->setFlowControl(serial->NoFlowControl);
    //serial->setFlowControl(serial->HardwareControl);
    if (!serial->open(QIODevice::ReadWrite))
    {
        qDebug() << ("Error returned during port opening: " + serial->errorString());
    }
    else
    {
        //serial->setDataTerminalReady(false); //ESP32 uses these for bootloader selection and reset so turn them off
        //serial->setRequestToSend(false);
    }

}

void MainWindow::readPendingDatagrams()
{
    qDebug() << "Got a UDP frame!";
    while (rxBroadcastGVRET->hasPendingDatagrams()) {
        QNetworkDatagram datagram = rxBroadcastGVRET->receiveDatagram();
        if (!remoteDeviceIPGVRET.contains(datagram.senderAddress().toString()))
        {
            remoteDeviceIPGVRET.insert(datagram.senderAddress().toString(), QString(datagram.data()));
            qDebug() << "Add new remote IP " << datagram.senderAddress().toString();
        }
    }
}

void MainWindow::serialError(QSerialPort::SerialPortError err)
{
    QString errMessage;
    bool killConnection = false;
    switch (err)
    {
    case QSerialPort::NoError:
        return;
    case QSerialPort::DeviceNotFoundError:
        errMessage = "Device not found error on serial";
        killConnection = true;
        break;
    case QSerialPort::PermissionError:
        errMessage =  "Permission error on serial port";
        killConnection = true;
        break;
    case QSerialPort::OpenError:
        errMessage =  "Open error on serial port";
        killConnection = true;
        break;
#if QT_VERSION <= QT_VERSION_CHECK( 6, 0, 0 )
    case QSerialPort::ParityError:
        errMessage = "Parity error on serial port";
        break;
    case QSerialPort::FramingError:
        errMessage = "Framing error on serial port";
        break;
    case QSerialPort::BreakConditionError:
        errMessage = "Break error on serial port";
        break;
#endif
    case QSerialPort::WriteError:
        errMessage = "Write error on serial port";
        break;
    case QSerialPort::ReadError:
        errMessage = "Read error on serial port";
        break;
    case QSerialPort::ResourceError:
        errMessage = "Serial port seems to have disappeared.";
        killConnection = true;
        break;
    case QSerialPort::UnsupportedOperationError:
        errMessage = "Unsupported operation on serial port";
        killConnection = true;
        break;
    case QSerialPort::UnknownError:
        errMessage = "Beats me what happened to the serial port.";
        killConnection = true;
        break;
    case QSerialPort::TimeoutError:
        errMessage = "Timeout error on serial port";
        killConnection = true;
        break;
    case QSerialPort::NotOpenError:
        errMessage = "The serial port isn't open";
        killConnection = true;
        break;
    }
    /*
    if (serial)
    {
        serial->clearError();
        serial->flush();
        serial->close();
    }*/
    if (errMessage.length() > 1)
    {
        qDebug() << errMessage;
    }
    if (killConnection)
    {
        qDebug() << "Shooting the serial object in the head. It deserves it.";
        //disconnectDevice();
    }
}

void MainWindow::readSerialData()
{
    QByteArray data;

    if (serial) data = serial->readAll();
    if (tcpClient) data = tcpClient->readAll();

    qDebug() << ("Got data from serial. Len = " + QString::number(data.length()));

    //unfortunately, the text widget being used automatically adds line breaks at the end of each
    //append you do but the serial interface can send partial lines so it is necessary
    //to buffer the input and only send once we see a line break and then only to the last line break
    serialBuilder.append(data);
    serialBuilder = serialBuilder.remove('\r');
    int lastBreak = serialBuilder.lastIndexOf('\n');
    if (lastBreak > 0)
    {
        //send up to that last line break we found
        ui->txtMainView->appendPlainText(serialBuilder.left(lastBreak));
        //then remove all that from the buffer leaving only the potentially
        //partial line behind
        serialBuilder = serialBuilder.right(serialBuilder.length() - lastBreak - 1);
        qDebug() << "Left over:" << serialBuilder;
    }


}

void MainWindow::handleSendText()
{
    QByteArray sendtxt = ui->lineSend->text().toUtf8() + "\n";

    if (serial) serial->write(sendtxt);
    qDebug() << "Sending this to serial port: " << sendtxt;

    ui->lineSend->clear();
}

void MainWindow::handleSaveButton()
{
    QString filename;
    QFileDialog dialog(this);

    QStringList filters;
    filters.append(QString(tr("Text File (*.txt)")));

    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.setViewMode(QFileDialog::Detail);
    dialog.setAcceptMode(QFileDialog::AcceptSave);

    if (dialog.exec() == QDialog::Accepted)
    {
        filename = dialog.selectedFiles()[0];
        if (!filename.contains('.')) filename += ".txt";
        if (dialog.selectedNameFilter() == filters[0])
        {
            QFile *outFile = new QFile(filename);

            if (!outFile->open(QIODevice::WriteOnly | QIODevice::Text))
            {
                delete outFile;
                return;
            }

            //dump the whole file in UTF8 format
            outFile->write(ui->txtMainView->toPlainText().toUtf8());

            outFile->close();
            delete outFile;
        }
    }

}
