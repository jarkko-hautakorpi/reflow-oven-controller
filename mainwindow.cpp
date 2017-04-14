#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include "reflow_commands.h"
#include <QMessageBox>
#include <QtSerialPort/QSerialPort>
#include <QTimer>
#include <QTime>
#include <QThread>

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
    ui->groupBox_Auto->setDisabled(true);
    ui->groupBox_manual->setDisabled(true);
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
    // GUI Button signals to slots
    connect(serial, SIGNAL(readyRead()), this, SLOT(readData()));
    connect(ui->pushButton_manualSet, SIGNAL(clicked()), this, SLOT(set_manual_mode()));
    connect(ui->pushButton_manualReset, SIGNAL(clicked()), this, SLOT(reset_manual_mode()));
    connect(ui->pushButton_StartReflow, SIGNAL(clicked()), this, SLOT(start_default_reflow()));
    connect(ui->pushButton_StartReflow, SIGNAL(clicked()), this, SLOT(start_default_reflow()));
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
            //connect(MainWindow::timer, SIGNAL(timeout()), this, SLOT(MainWindow::updateTemperatureReading()));
            MainWindow::timer->start(1000);
            ui->groupBox_Auto->setDisabled(false);
            ui->groupBox_manual->setDisabled(false);
            serial->setDataTerminalReady(p.DTR_Signal);
            serial->setRequestToSend(p.RTS_Signal);
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
    QString test_data;
    test_data += (char)COMMAND_RELAY_ON;
    test_data += (char) 0b00000001;
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

void MainWindow::receivedTemperatureReading(const quint8 *buffer, quint16 bytes_received)
{
    quint16 temperature = 0;
    temperature |= buffer[1]; // msb
    temperature = temperature << 8;
    temperature |= buffer[2]; // lsb
    QString temp;
    temp.setNum(temperature);
    ui->lcdNumber->display(temp);
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
            //  CRC validation
            else if(  (MainWindow::hdlc_rx_frame_index >= 2) // one data byte + 2 CRC bytes
                      &&(MainWindow::hdlc_crc_check(MainWindow::hdlc_rx_frame_index)) )
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
        MainWindow::local_echo(data);

        // Calculate checksum EI TOIMI EI TÄHÄN!
        //hdlc_rx_frame_fcs = qChecksum((const char *) data, 1);
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

/**
 * @brief Received command execution and handling
 *
 * @param buffer Received data
 * @param bytes_received Number of received data bytes
 */
void MainWindow::hdlc_on_rx_frame(const quint8 *buffer, quint16 bytes_received)
{
    if(bytes_received)
    {
        ui->plainTextSerialLog->appendPlainText((const QString)buffer[0]);
    }
    int response = (int) buffer[0];
    switch (response) {
    case COMMAND_GET_TEMPERATURE: MainWindow::receivedTemperatureReading(buffer, bytes_received);
        break;
    case COMMAND_RELAY_ON: MainWindow::showRelayState(buffer, bytes_received);
        break;
    case COMMAND_RELAY_OFF:  MainWindow::showRelayState(buffer, bytes_received);
        break;
    case COMMAND_STOP_ALL: MainWindow::stopAll();
        break;
    default:
        break;
    }

}

void MainWindow::hdlc_tx_frame(const char *buffer, quint8 bytes_to_send)
{
    quint8 data;
    quint16 crc = CRC16_CCITT_INIT_VAL;

    // Start marker
    MainWindow::hdlc_tx_byte((quint8)HDLC_FLAG_SEQUENCE);

    // Update checksum
    crc = qChecksum(buffer,bytes_to_send);

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
    //crc ^= 0xffff;

    // Low byte of inverted FCS
    data = U16_LO8(crc);
    if((data == HDLC_CONTROL_ESCAPE) || (data == HDLC_FLAG_SEQUENCE))
    {
        MainWindow::hdlc_tx_byte(HDLC_CONTROL_ESCAPE);
        data ^= HDLC_ESCAPE_BIT;
    }
    MainWindow::hdlc_tx_byte(data);

    // High byte of inverted FCS
    data = U16_HI8(crc);
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
    //hdlc_tx_frame(reinterpret_cast<const char *>(&command), sizeof command);
    //Tai:
    //hdlc_tx_frame( (const char*) &command , sizeof command);
    QByteArray character;
    character.append((char)command);
    const char *c_str2 = character.data();
    MainWindow::hdlc_tx_frame(c_str2,1);
}

bool MainWindow::hdlc_crc_check(int frame_index) {
    quint16 crc_received = 0;
    crc_received = MainWindow::hdlc_rx_frame[frame_index-1];
    crc_received = crc_received << 8;
    crc_received |= MainWindow::hdlc_rx_frame[frame_index-2];
    quint16 qtcrc = qChecksum( (const char *)MainWindow::hdlc_rx_frame, (frame_index-2));
    if( crc_received == qtcrc) {
        return true;
    } else {
        return false;
    }
}

void MainWindow::start_default_reflow() {
    // Change the GUI stuff
    ui->progressBar->setMaximum(0);
    ui->progressBar->setMinimum(0);
    ui->progressBar->setValue(-1);
    ui->groupBox_manual->setDisabled(true);

    quint16 soak_temp=0, soak_time=0, reflow_temp=0, reflow_time=0;
    QString data_frame;
    QTime time = ui->timeEdit_soak->time();
    // Soak
    soak_time = (60 * time.minute()) + (time.second());
    soak_temp = ui->lineEdit_soakTemp->text().toInt();
    // Reflow
    time = ui->timeEdit_reflow->time();
    reflow_time = (60 * time.minute()) + (time.second());
    reflow_temp = ui->lineEdit_reflowTemp->text().toInt();
    // Frame structure = "command, soaktimeH, soaktimeL, soaktempH, soaktempL, reflowtimeH, reflowtimeL, reflowtempH, reflowtempL"
    data_frame += (char)COMMAND_START_DEFAULT_REFLOW;
    data_frame += (char) U16_HI8(soak_time);    data_frame += (char) U16_LO8(soak_time);
    data_frame += (char) U16_HI8(soak_temp);    data_frame += (char) U16_LO8(soak_temp);
    data_frame += (char) U16_HI8(reflow_time);    data_frame += (char) U16_LO8(reflow_time);
    data_frame += (char) U16_HI8(reflow_temp);    data_frame += (char) U16_LO8(reflow_temp);
    ui->statusBar->showMessage(tr("Starting Reflow"));
    QByteArray character = data_frame.toLocal8Bit();
    const char *c_str2 = character.data();
    MainWindow::hdlc_tx_frame(c_str2,character.size());
}

// Change relay state by sending a command "command, bitmask"
void MainWindow::toggle_relay(bool checked, quint8 relay) {
    QString data_frame;
    if(checked) {
        data_frame += (char)COMMAND_RELAY_ON;
        ui->statusBar->showMessage(tr("Relay ON"));
    } else {
        data_frame += (char)COMMAND_RELAY_OFF;
        ui->statusBar->showMessage(tr("Relay OFF"));
    }
    data_frame += (char)relay;
    QByteArray character = data_frame.toLocal8Bit();
    const char *c_str2 = character.data();
    MainWindow::hdlc_tx_frame(c_str2,character.size());
}

// Manual relay control. Checkboxes set the state of relay by bitmask
// RELAY 1
void MainWindow::on_checkBox_relay1_toggled(bool checked)
{
    MainWindow::toggle_relay(checked, 0x01);
}
// RELAY 2
void MainWindow::on_checkBox_relay2_toggled(bool checked)
{
    MainWindow::toggle_relay(checked, 0x02);
}
// RELAY 3
void MainWindow::on_checkBox_relay3_toggled(bool checked)
{
    MainWindow::toggle_relay(checked, 0x04);
}
// RELAY 4
void MainWindow::on_checkBox_relay4_toggled(bool checked)
{
    MainWindow::toggle_relay(checked, 0x08);
}
void MainWindow::stopAll(void) {
    ui->progressBar->setMaximum(100);
    ui->progressBar->setValue(100);
    ui->checkBox_relay1->setChecked(false);
    ui->checkBox_relay2->setChecked(false);
    ui->checkBox_relay3->setChecked(false);
    ui->checkBox_relay4->setChecked(false);
    // Allow manual/auto use now
    ui->groupBox_Auto->setDisabled(false);
    ui->groupBox_manual->setDisabled(false);
}
void MainWindow::showRelayState(const quint8 *buffer, quint16 bytes_received) {
    if(bytes_received > 1) {
        int command = buffer[0];
        int bitmask = buffer[1];
        bool status;
        if(command == (int)COMMAND_RELAY_ON) {
            status = true;
        } else {
            status = false;
        }
        if ((bitmask & 0x01) == 0x01)	{ ui->checkBox_relay1->setChecked(status); }
        if ((bitmask & 0x02) == 0x02)	{ ui->checkBox_relay2->setChecked(status); }
        if ((bitmask & 0x04) == 0x04)	{ ui->checkBox_relay3->setChecked(status); }
        if ((bitmask & 0x08) == 0x08)	{ ui->checkBox_relay4->setChecked(status); }
    }
}

void MainWindow::on_pushButton_sendAT_Baudrate_clicked()
{
    QString atcommand;
    atcommand += "AT+B";
    atcommand += ui->lineEdit_ATBAUDRATE->text();
    QByteArray characters = atcommand.toLocal8Bit();
    const char *c_str2 = characters.data();
    //serial->write("AT+B4800");
    serial->write(c_str2);
    // Change the serial port baudrate now
    SettingsDialog::Settings p = settings->settings();
    p.baudRate = ui->lineEdit_ATBAUDRATE->text().toInt();
}
