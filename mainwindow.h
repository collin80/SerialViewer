#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextCharFormat>
#include <QTimer>

#include <QtSerialPort/QSerialPort>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QUdpSocket>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void handleConnectButton();
    void handleClearButton();
    void handleSaveButton();
    void handleSendText();

private slots:
    void readSerialData();
    void serialError(QSerialPort::SerialPortError err);
    void readPendingDatagrams();
    void refreshSerialList();
    void portChanged();
    void disconnectPort();
    void deviceConnected();

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;
    QTcpSocket *tcpClient;
    QUdpSocket *rxBroadcastGVRET;
    QString serialBuilder;
    QHash<QString, QString> remoteDeviceIPGVRET;
    QTimer serialRefreshTimer;
    QTextCharFormat normalFormat;
    QTextCharFormat sentFormat;
    int connectedPort;
};
#endif // MAINWINDOW_H
