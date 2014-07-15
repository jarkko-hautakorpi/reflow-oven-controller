#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtCore/QtGlobal>
#include <QMainWindow>
#include <QtSerialPort/QSerialPort>

// HDLC settings
#define HDLC_MRU    64
#define HDLC_FLAG_SEQUENCE  0x7e   // Flag Sequence
#define HDLC_CONTROL_ESCAPE 0x7d   // Asynchronous Control Escape
#define HDLC_ESCAPE_BIT     0x20   // Asynchronous transparency modifier
#define HDLC_INITFCS        0xffff
#define HDLC_GOODFCS        0xf0b8
#define HDLC_FCS_POLYNOMIAL 0x8408
#define U16_HI8(data) ((quint8)(((data)>>8)&0xff))
#define U16_LO8(data) ((quint8)((data)&0xff))
#define U32_HI8(data) ((quint8)(((data)>>24)&0xff))
#define U32_MH8(data) ((quint8)(((data)>>16)&0xff))
#define U32_ML8(data) ((quint8)(((data)>>8)&0xff))
#define U32_LO8(data) ((quint8)((data)&0xff))

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
    char hdlc_rx_frame[HDLC_MRU];
    int hdlc_rx_frame_index;
    int hdlc_rx_frame_fcs;
    bool hdlc_rx_char_esc;
    explicit MainWindow(QWidget *parent = 0);
    void writeByte(const quint8 data);
    void hdlc_on_rx_frame(const quint8 *buffer, quint16 bytes_received);
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
    void hdlc_on_rx_byte(quint8 data);
    void hdlc_on_rx_frame(const quint8 *buffer, qint16 bytes_received);
    void hdlc_tx_frame(const quint8 *buffer, quint8 bytes_to_send);
    void hdlc_tx_byte(quint8 byte);

private:
    Ui::MainWindow *ui;
    SettingsDialog *settings;
    QSerialPort *serial;
};

#endif // MAINWINDOW_H
