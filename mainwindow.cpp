#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include "reflow_commands.h"
#include <QMessageBox>
#include <QtSerialPort/QSerialPort>
#include <QTimer>

quint8 MainWindow::hdlc_rx_frame[HDLC_MRU] = {0};
int MainWindow::hdlc_rx_frame_index = 0;
int MainWindow::hdlc_rx_frame_fcs   = HDLC_INITFCS;
bool MainWindow::hdlc_rx_char_esc    = false;
QString MainWindow::testbuffer = "";

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    timer = new QTimer(this);
    serial = new QSerialPort(this);
    // HDLC settings

    // Serial port settings dialog
    settings = new SettingsDialog;


    ui->actionConnect->setEnabled(true);
    ui->actionDisconnect->setEnabled(false);
    ui->actionQuit->setEnabled(true);
    ui->actionConfigure->setEnabled(true);

    initActionsConnections();

    connect(serial, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleError(QSerialPort::SerialPortError)));

    connect(serial, SIGNAL(readyRead()), this, SLOT(readData()));
    //connect(console, SIGNAL(getData(QByteArray)), this, SLOT(writeData(QByteArray)));
    connect(ui->pushButton_manualSet, SIGNAL(clicked()), this, SLOT(set_manual_mode()));
    connect(ui->pushButton_manualReset, SIGNAL(clicked()), this, SLOT(reset_manual_mode()));
    // Update/get temperature every 1 second using QTimer

    //connect(timer, SIGNAL(timeout()), this, SLOT(updateTemperatureReading()));
    //timer->start(1000);
}

MainWindow::~MainWindow()
{
    delete settings;
    delete ui;
}

void MainWindow::openSerialPort()
{
    SettingsDialog::Settings p = settings->settings();
    serial->setPortName(p.name);
    serial->setBaudRate(p.baudRate);
    serial->setDataBits(p.dataBits);
    serial->setParity(p.parity);
    serial->setStopBits(p.stopBits);
    serial->setFlowControl(p.flowControl);
    if (serial->open(QIODevice::ReadWrite)) {
            ui->actionConnect->setEnabled(false);
            ui->actionDisconnect->setEnabled(true);
            ui->actionConfigure->setEnabled(false);
            ui->statusBar->showMessage(tr("Connected to %1 : %2, %3, %4, %5, %6")
                                       .arg(p.name).arg(p.stringBaudRate).arg(p.stringDataBits)
                                       .arg(p.stringParity).arg(p.stringStopBits).arg(p.stringFlowControl));
            connect(MainWindow::timer, SIGNAL(timeout()), this, SLOT(MainWindow::updateTemperatureReading()));
            MainWindow::timer->start(1000);
    } else {
        QMessageBox::critical(this, tr("Error"), serial->errorString());

        ui->statusBar->showMessage(tr("Open error"));
    }
}

void MainWindow::closeSerialPort()
{
    disconnect(timer, SIGNAL(timeout()), this, SLOT(updateTemperatureReading()));
    sendCommand(COMMAND_STOP_ALL);
    serial->close();
    //console->setEnabled(false);
    ui->actionConnect->setEnabled(true);
    ui->actionDisconnect->setEnabled(false);
    ui->actionConfigure->setEnabled(true);
    ui->statusBar->showMessage(tr("Disconnected"));
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About Reflow terminal"),
                       tr("The <b>Reflow terminal</b> is an application "
                          "to control reflow controller that is connected to "
                          "serial port."));
}

// Serial port send bytes
void MainWindow::writeData(const QByteArray &data)
{
    serial->write(data);
}
// Serial port send byte (for hdlc)
void MainWindow::writeByte(const quint8 data)
{
    const QByteArray array_data = (const char *)data;
    this->writeData(array_data);
}

void MainWindow::readData()
{
    QByteArray data = serial->readAll();
    MainWindow::hdlc_on_rx_byte(data);
}

void MainWindow::clearLog()
{
    ui->plainTextSerialLog->clear();
}


void MainWindow::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        QMessageBox::critical(this, tr("Critical Error"), serial->errorString());
        closeSerialPort();
    }
}

void MainWindow::set_manual_mode()
{
    const QString test_data = "ABC";
    ui->statusBar->showMessage(tr("Manual start"));
    ui->plainTextSerialLog->appendPlainText(test_data);
    QByteArray character = test_data.toLocal8Bit();
    const char *c_str2 = character.data();
    MainWindow::hdlc_tx_frame(c_str2,character.size());
}

void MainWindow::reset_manual_mode()
{
    ui->statusBar->showMessage(tr("Manual reset"));
}

void MainWindow::updateTemperatureReading()
{
    sendCommand(COMMAND_GET_TEMPERATURE);
}

void MainWindow::initActionsConnections()
{
    connect(ui->actionConnect, SIGNAL(triggered()), this, SLOT(openSerialPort()));
    connect(ui->actionDisconnect, SIGNAL(triggered()), this, SLOT(closeSerialPort()));
    connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->actionConfigure, SIGNAL(triggered()), settings, SLOT(show()));
    connect(ui->actionClear, SIGNAL(triggered()), this, SLOT(clearLog()));
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(about()));
    connect(ui->actionAboutQt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
}

void MainWindow::hdlc_on_rx_byte(QByteArray q_data)
{
    quint8 data = 0;
    int qsize = q_data.size();
    for(int i=0;i<q_data.size();i++)
    {
        data = static_cast<quint8>(q_data[i]);
        // Start/End sequence
        if(data == HDLC_FLAG_SEQUENCE)
        {
            // If Escape sequence + End sequence is received then this packet must be silently discarded
            if(hdlc_rx_char_esc == true)
            {
                MainWindow::hdlc_rx_char_esc = false;
            }
            //  Minimum requirement for a valid frame is reception of good FCS
            else if(  (MainWindow::hdlc_rx_frame_index >= sizeof(hdlc_rx_frame_fcs))
                      &&(hdlc_rx_frame_fcs   == CRC16_CCITT_MAGIC_VAL    )  )
            {
                // Pass on frame with FCS field removed
                quint16 frm_index = (quint16)MainWindow::hdlc_rx_frame_index-2;
                MainWindow::hdlc_on_rx_frame((const quint8*)(MainWindow::hdlc_rx_frame), frm_index);
            }
            // Reset for next packet
            MainWindow::hdlc_rx_frame_index = 0;
            hdlc_rx_frame_fcs   = CRC16_CCITT_INIT_VAL;
            return;
        }

        // Escape sequence processing
        if(MainWindow::hdlc_rx_char_esc)
        {
            MainWindow::hdlc_rx_char_esc  = false;
            data             ^= HDLC_ESCAPE_BIT;
        }
        else if(data == HDLC_CONTROL_ESCAPE)
        {
            MainWindow::hdlc_rx_char_esc = true;
            return;
        }

        // Store received data
        MainWindow::hdlc_rx_frame[MainWindow::hdlc_rx_frame_index] = data;

        // Calculate checksum EI TOIMI EI TÄHÄN!
        hdlc_rx_frame_fcs = qChecksum((const char *) data, 1);
        //crc16_ccitt_calc_byte((quint16)hdlc_rx_frame_fcs, (quint8)data);

        // Go to next position in buffer
        MainWindow::hdlc_rx_frame_index++;

        // Check for buffer overflow
        if(MainWindow::hdlc_rx_frame_index == HDLC_MRU)
        {
            // Wrap index
            MainWindow::hdlc_rx_frame_index  = 0;

            // Invalidate FCS so that packet will be rejected
            hdlc_rx_frame_fcs  ^= 0xFFFF;
        }
    }
}

void MainWindow::hdlc_on_rx_frame(const quint8 *buffer, quint16 bytes_received)
{
    if(bytes_received)
    {
        ui->plainTextSerialLog->appendPlainText((const QString)buffer[0]);
    } else {
        for(int i=0;i<bytes_received;i++)
        {
            ui->plainTextSerialLog->appendPlainText((const QString)buffer[i]);
        }
    }
}

void MainWindow::hdlc_tx_frame(const char *buffer, quint8 bytes_to_send)
{
    quint8 data;
    quint16 fcs = CRC16_CCITT_INIT_VAL;
    int buffsize = SIZEOF_ARRAY(buffer);

    // Start marker
    MainWindow::hdlc_tx_byte((quint8)HDLC_FLAG_SEQUENCE);

    // Update checksum
    fcs = qChecksum(buffer,SIZEOF_ARRAY(buffer));

    // Send escaped data
    while(bytes_to_send)
    {
        // Get next data
        data = *(buffer++);

        // See if data should be escaped
        if((data == HDLC_CONTROL_ESCAPE) || (data == HDLC_FLAG_SEQUENCE))
        {
            MainWindow::hdlc_tx_byte(HDLC_CONTROL_ESCAPE);
            data ^= HDLC_ESCAPE_BIT;
        }

        // Send data
        MainWindow::hdlc_tx_byte(data);

        // decrement counter
        bytes_to_send--;
    }

    // Invert checksum
    fcs ^= 0xffff;

    // Low byte of inverted FCS
    data = U16_LO8(fcs);
    if((data == HDLC_CONTROL_ESCAPE) || (data == HDLC_FLAG_SEQUENCE))
    {
        MainWindow::hdlc_tx_byte(HDLC_CONTROL_ESCAPE);
        data ^= HDLC_ESCAPE_BIT;
    }
    MainWindow::hdlc_tx_byte(data);

    // High byte of inverted FCS
    data = U16_HI8(fcs);
    if((data == HDLC_CONTROL_ESCAPE) || (data == HDLC_FLAG_SEQUENCE))
    {
        MainWindow::hdlc_tx_byte(HDLC_CONTROL_ESCAPE);
        data ^= HDLC_ESCAPE_BIT;
    }
    MainWindow::hdlc_tx_byte(data);

    // End marker
    MainWindow::hdlc_tx_byte(HDLC_FLAG_SEQUENCE);
}

void MainWindow::hdlc_tx_byte(const char *byte)
{
    serial->write(static_cast<QByteArray>(byte));
}

void MainWindow::hdlc_tx_byte(int byte)
{
    MainWindow::testbuffer.append((char)byte);
    serial->write(QByteArray(1, (char)byte));
}

void MainWindow::hdlc_tx_byte(QByteArray *byte)
{
    serial->write(*byte);
}

void MainWindow::local_echo(char character)
{
    SettingsDialog::Settings p = settings->settings();
    if(p.localEchoEnabled)
    {
        ui->plainTextSerialLog->appendPlainText(QString::number(character));
    }
}

void MainWindow::sendCommand(int command) {
    QByteArray character;
    character.append((char)command);
    const char *c_str2 = character.data();
    MainWindow::hdlc_tx_frame(c_str2,1);
}
