#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCore/QtGlobal>
#include <QString>
#include <QtSerialPort/QSerialPort>

// HDLC settings
#define HDLC_MRU    64
#define HDLC_FLAG_SEQUENCE  0x7e   // Flag Sequence d126, 7E, "~"
#define HDLC_CONTROL_ESCAPE 0x7d   // Asynchronous Control Escape d125, 7D, "}"
#define HDLC_ESCAPE_BIT     0x20   // Asynchronous transparency modifier d32, 20, " "
#define HDLC_INITFCS        0xffff
#define HDLC_GOODFCS        0xf0b8
#define HDLC_FCS_POLYNOMIAL 0x8408
#define CRC16_CCITT_POLYNOMIAL  0x8408
/// Initial CRC value
#define CRC16_CCITT_INIT_VAL    0xffff
/// Magic CRC value
#define CRC16_CCITT_MAGIC_VAL   0xf0b8
#define U16_HI8(data) ((quint8)(((data)>>8)&0xff))
#define U16_LO8(data) ((quint8)((data)&0xff))
#define U32_HI8(data) ((quint8)(((data)>>24)&0xff))
#define U32_MH8(data) ((quint8)(((data)>>16)&0xff))
#define U32_ML8(data) ((quint8)(((data)>>8)&0xff))
#define U32_LO8(data) ((quint8)((data)&0xff))
#define U16_FROM8_HI_LO(msb, lsb)  ((uint16_t)((msb)<<8) | (lsb))
#define SIZEOF_ARRAY( a ) (sizeof( a ) / sizeof( a[ 0 ] ))

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
    static quint8 hdlc_rx_frame[HDLC_MRU];
    static int hdlc_rx_frame_index;
    static int hdlc_rx_frame_fcs;
    static bool hdlc_rx_char_esc;
    static QString testbuffer;
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
    // GUI button actions
    void set_manual_mode();
    void reset_manual_mode();
    void updateTemperatureReading();
    void receivedTemperatureReading(const quint8 *buffer, quint16 bytes_received);
    void start_default_reflow();
    void toggle_relay(bool checked, quint8 relay);
    void on_checkBox_relay1_toggled(bool checked);
    void on_checkBox_relay2_toggled(bool checked);
    void on_checkBox_relay3_toggled(bool checked);
    void on_checkBox_relay4_toggled(bool checked);

private:
    QTimer *timer;
    void initActionsConnections();
    void hdlc_on_rx_byte(QByteArray q_data);
    void hdlc_tx_frame(const char *buffer, quint8 bytes_to_send);
    void hdlc_tx_byte(const char *byte);
    void hdlc_tx_byte(int byte);
    void hdlc_tx_byte(QByteArray *byte);
    bool hdlc_rx_frame_crc_check(int frame_index);
    void local_echo(char character);
    void sendCommand(int command);
    bool hdlc_crc_check(int frame_index);
    void stopAll(void);
    void showRelayState(const quint8 *buffer, quint16 bytes_received);

private:
    Ui::MainWindow *ui;
    SettingsDialog *settings;
    QSerialPort *serial;
};

#endif // MAINWINDOW_H
