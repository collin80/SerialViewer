#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <qt5/QtSerialPort/QSerialPort>

#include <qt5/QtNetwork/QTcpSocket>
#include <qt5/QtNetwork/QUdpSocket>

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

private:
    Ui::MainWindow *ui;
    QSerialPort *serial;
    QTcpSocket *tcpClient;
    QUdpSocket *udpClient;
};
#endif // MAINWINDOW_H
