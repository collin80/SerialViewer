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
    connect(ui->btnDisconnect, &QPushButton::clicked, this, &MainWindow::disconnectPort);
    connect(ui->listPorts, &QListWidget::currentItemChanged, this, &MainWindow::portChanged);
    connect(ui->lineCustomDevice, &QLineEdit::textChanged, this, &MainWindow::portChanged);

    serialRefreshTimer.setInterval(2000);
    serialRefreshTimer.setTimerType(Qt::CoarseTimer);
    serialRefreshTimer.start();
    connect(&serialRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshSerialList);

    rxBroadcastGVRET = new QUdpSocket(this);
    //Need to make sure it tries to share the address in case there are
    //multiple instances of SavvyCAN running.
    rxBroadcastGVRET->bind(QHostAddress::AnyIPv4, 17222, QAbstractSocket::ShareAddress);
    connect(rxBroadcastGVRET, &QUdpSocket::readyRead, this, &MainWindow::readPendingDatagrams);

    //only need this in the special case of network devices that present serial interfaces
    ui->groupPort->setVisible(false);

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::handleClearButton()
{
    ui->txtMainView->clear();
}

void MainWindow::portChanged()
{
    QString currText = ui->listPorts->currentItem()->text();
    if (currText.contains("[") || ui->lineCustomDevice->text().length() > 3) //it's a network interface
    {
        ui->groupPort->setVisible(true);
        ui->groupSpeed->setVisible(false);
    }
    else //it's a direct serial interface
    {
        ui->groupPort->setVisible(false);
        ui->groupSpeed->setVisible(true);
    }
}

void MainWindow::disconnectPort()
{
    ui->lblStatus->setText("Not Connected");

    if (serial != nullptr)
    {
        if (serial->isOpen())
        {
            //serial->clear();
            serial->close();

        }
        serial->disconnect(); //disconnect all signals
        delete serial;
        serial = nullptr;
    }

    if (tcpClient != nullptr)
    {
        if (tcpClient->isOpen())
        {
            tcpClient->close();
        }
        tcpClient->disconnect();
        delete tcpClient;
        tcpClient = nullptr;
    }
}

/*
 * Grabs any new serial ports that show up and adds them. Currently does not
 * remove ports from the list if they disappear from the computer. Might be a good
 * idea to do that eventually. Would have to track which were seen and which were not.
 */
void MainWindow::refreshSerialList()
{
    //qDebug() << "Ping!";

    QList<QSerialPortInfo> ports;

    ports = QSerialPortInfo::availablePorts();

    foreach(QString IP, remoteDeviceIPGVRET.keys())
    {
        QString name = remoteDeviceIPGVRET.value(IP);
        //only add it if it isn't already in there.
        if (ui->listPorts->findItems(IP, Qt::MatchContains).count() == 0)
            ui->listPorts->addItem(IP + "  [" + name + "]");
    }

    for (int i = 0; i < ports.count(); i++)
    {
        if (ui->listPorts->findItems(ports[i].portName(), Qt::MatchContains).count() == 0)
            ui->listPorts->addItem(ports[i].portName());
    }
}
void MainWindow::handleConnectButton()
{
    QString portName = "";
    if (ui->listPorts->currentRow() > -1) portName = ui->listPorts->currentItem()->text();

    //we're only connecting to a single connection so disconnect anything open before continuing
    if(serial)
        disconnectPort();
    if(tcpClient)
        disconnectPort();

    //Now figure out whether it's a serial port or a network port
    if (portName.contains("[") || ui->lineCustomDevice->text().length() > 3) //network port
    {
        int port = 23;
        if (ui->rbTelnet->isChecked()) port = 23;
        if (ui->rbAlternate->isChecked()) port = 2323;
        if (ui->rbCustom->isChecked())
        {
            port = ui->lineCustomPort->text().toInt();
            if (port == 0) port = 23;
        }
        QString ipAddr;
        if (ui->lineCustomDevice->text().length() > 3) ipAddr = ui->lineCustomDevice->text();
        else ipAddr = portName.split(" ")[0];
        if (ipAddr.length() < 3) return;
        qDebug() << ("TCP Connection to a remote device");
        tcpClient = new QTcpSocket();
        tcpClient->connectToHost(ipAddr, port);
        connect(tcpClient, SIGNAL(readyRead()), this, SLOT(readSerialData()));
        connect(tcpClient, SIGNAL(connected()), this, SLOT(deviceConnected()));
        qDebug() << ("Created TCP Socket");
    }
    else //serial port
    {
        if (portName.length() < 2) return;
        serial = new QSerialPort(QSerialPortInfo(portName));
        if(!serial) {
            qDebug() << ("can't open serial port " + portName);
            return;
        }
        qDebug() << ("Created Serial Port Object");

        /* connect reading event */
        connect(serial, SIGNAL(readyRead()), this, SLOT(readSerialData()));
        connect(serial, SIGNAL(error(QSerialPort::SerialPortError)), this, SLOT(serialError(QSerialPort::SerialPortError)));

        int speed = 115200;

        if (ui->rbMega->isChecked()) speed = 1000000;
        if (ui->rbCustomSpeed->isChecked()) speed = ui->lineCustomSpeed->text().toInt();
        if (speed < 300) speed = 115200; //a sane value if something stupidly low was set

        /* configure */
        serial->setBaudRate(speed);
        serial->setDataBits(serial->Data8);

        serial->setFlowControl(serial->NoFlowControl);
        //serial->setFlowControl(serial->HardwareControl);
        if (!serial->open(QIODevice::ReadWrite))
        {
            qDebug() << ("Error returned during port opening: " + serial->errorString());
            return;
        }
        else
        {
            //serial->setDataTerminalReady(false); //ESP32 uses these for bootloader selection and reset so turn them off
            //serial->setRequestToSend(false);
        }
        ui->lblStatus->setText("Connected to " + portName);
    }
}

void MainWindow::deviceConnected()
{
    if (ui->lineCustomDevice->text().length() > 3) ui->lblStatus->setText("Connected to " + ui->lineCustomDevice->text());
    else ui->lblStatus->setText("Connected to " + ui->listPorts->currentItem()->text());
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

    if (serial)
    {
        serial->clearError();
        serial->flush();
        serial->close();
    }
    if (errMessage.length() > 1)
    {
        qDebug() << errMessage;
    }
    if (killConnection)
    {
        qDebug() << "Killing the serial object. It is no longer needed";
        disconnectPort();
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
    if (serial == nullptr && tcpClient == nullptr)
    {
        qDebug() << "Attempt to write to serial port when it has not been initialized!";
        return;
    }

    if (serial && !serial->isOpen())
    {
        qDebug() << ("Attempt to write to serial port when it is not open!");
        return;
    }

    if (tcpClient && !tcpClient->isOpen())
    {
        qDebug() << ("Attempt to write to TCP/IP port when it is not open!");
        return;
    }

    QByteArray sendtxt = ui->lineSend->text().toUtf8() + "\n";

    if (serial) serial->write(sendtxt);
    if (tcpClient) tcpClient->write(sendtxt);
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
