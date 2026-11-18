#ifndef TCPFILESENDER_H
#define TCPFILESENDER_H

#include <QDialog>
#include <QTcpSocket>
#include <QFile>
#include <QString>

class QProgressBar;
class QLabel;
class QPushButton;

class TcpFileSender : public QDialog
{
    Q_OBJECT

public:
    explicit TcpFileSender(QWidget *parent = nullptr);
    ~TcpFileSender() override;

private slots:
    void openFile();
    void start();
    void startTransfer();
    void updateClientProgress(qint64 numBytes);
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void failTransfer(const QString &message);

    QProgressBar *clientProgressBar;
    QLabel *clientStatusLabel;
    QPushButton *startButton;
    QPushButton *openButton;
    QPushButton *quitButton;

    QTcpSocket tcpClient;
    QFile localFile;
    QString fileName;
    QByteArray outBlock;

    qint64 totalBytes = 0;
    qint64 bytesWritten = 0;
    qint64 bytesToWrite = 0;

    static constexpr qint64 loadSize = 4096;
    bool transferInProgress = false;
};

#endif // TCPFILESENDER_H
