#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtCore/QtGlobal>

#include <QMainWindow>

#include <QtSerialPort/QSerialPort>

QT_BEGIN_NAMESPACE

namespace Ui {
class MainWindow;
}

QT_END_NAMESPACE

class SettingsDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    void writeByte(quint8 &data);
    void hdlc_on_rx_frame();
    ~MainWindow();

private slots:
    void openSerialPort();
    void closeSerialPort();
    void about();
    void writeData(const QByteArray &data);
    void readData();
    void clearLog();
    void handleError(QSerialPort::SerialPortError error);


private:
    void initActionsConnections();

private:
    Ui::MainWindow *ui;
    SettingsDialog *settings;
    QSerialPort *serial;
};

#endif // MAINWINDOW_H
