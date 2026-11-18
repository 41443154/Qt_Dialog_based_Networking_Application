#include "tcpfilesender.h"

#include <QDataStream>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtGlobal>

// 建立視窗
TcpFileSender::TcpFileSender(QWidget *parent)
    : QDialog(parent),
    clientProgressBar(new QProgressBar(this)),
    clientStatusLabel(new QLabel(tr("客戶端就緒唄"), this)),
    startButton(new QPushButton(tr("開始"), this)),
    openButton(new QPushButton(tr("開檔"), this)),
    quitButton(new QPushButton(tr("退出"), this)),
    tcpClient(this)
{
    setWindowTitle(tr("檔案傳送"));
    resize(420, 150);

    clientProgressBar->setRange(0, 100);
    clientProgressBar->setValue(0);
    startButton->setEnabled(false);

    // 建立按鈕排列
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(startButton);
    buttonLayout->addWidget(openButton);
    buttonLayout->addWidget(quitButton);

    // 建立整體畫面
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(clientProgressBar);
    mainLayout->addWidget(clientStatusLabel);
    mainLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    // 連接按鈕事件
    connect(openButton, &QPushButton::clicked,
            this, &TcpFileSender::openFile);

    connect(startButton, &QPushButton::clicked,
            this, &TcpFileSender::start);

    connect(quitButton, &QPushButton::clicked,
            this, &QWidget::close);


    connect(&tcpClient, &QTcpSocket::connected,
            this, &TcpFileSender::startTransfer);


    connect(&tcpClient, &QTcpSocket::bytesWritten,
            this, &TcpFileSender::updateClientProgress);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(&tcpClient, &QAbstractSocket::errorOccurred,
            this, &TcpFileSender::onSocketError);
#else
    connect(&tcpClient,
            SIGNAL(error(QAbstractSocket::SocketError)),
            this,
            SLOT(onSocketError(QAbstractSocket::SocketError)));
#endif
}

TcpFileSender::~TcpFileSender() = default;


// 選擇要傳送的檔案
void TcpFileSender::openFile()
{
    QString selectedFile =
        QFileDialog::getOpenFileName(this, tr("選擇檔案"));

    if (selectedFile.isEmpty())
        return;

    fileName = selectedFile;

    clientStatusLabel->setText(
        tr("已選擇：%1").arg(QFileInfo(fileName).fileName())
        );

    clientProgressBar->setValue(0);
    startButton->setEnabled(true);
}


// 開始建立 TCP 連線
void TcpFileSender::start()
{
    if (fileName.isEmpty())
        return;

    startButton->setEnabled(false);
    openButton->setEnabled(false);

    totalBytes = 0;
    bytesWritten = 0;
    bytesToWrite = 0;
    transferInProgress = true;

    clientProgressBar->setValue(0);
    clientStatusLabel->setText(tr("連接中..."));

    tcpClient.abort();

    tcpClient.connectToHost(
        QHostAddress::LocalHost, 16998
        );
}


// TCP 連線成功後開始傳輸
void TcpFileSender::startTransfer()
{
    localFile.setFileName(fileName);

    if (!localFile.open(QIODevice::ReadOnly))
    {
        failTransfer(
            tr("無法讀取檔案：%1").arg(localFile.errorString())
            );
        return;
    }

    // 取出檔名
    QString currentFile = QFileInfo(fileName).fileName();

    // 製作傳輸標頭
    outBlock.clear();

    QDataStream sendOut(&outBlock, QIODevice::WriteOnly);
    sendOut.setVersion(QDataStream::Qt_4_6);

    // 預留兩個 qint64 欄位
    sendOut << qint64(0) << qint64(0) << currentFile;

    // 檔案總大小包含標頭
    totalBytes = localFile.size() + outBlock.size();

    // 回到開頭，填入標頭資訊
    sendOut.device()->seek(0);

    sendOut << totalBytes
            << qint64(outBlock.size() - 2 * sizeof(qint64));

    bytesToWrite = localFile.size();

    // 先傳送標頭
    qint64 sent = tcpClient.write(outBlock);

    if (sent != outBlock.size())
    {
        failTransfer(tr("傳送檔案標頭失敗"));
        return;
    }

    outBlock.clear();

    clientStatusLabel->setText(tr("已連接，正在傳送..."));
}


// 分批傳送檔案並更新進度條
void TcpFileSender::updateClientProgress(qint64 numBytes)
{
    if (!transferInProgress)
        return;

    bytesWritten += numBytes;

    // 每次最多讀取 4KB
    if (bytesToWrite > 0 &&
        tcpClient.bytesToWrite() <= loadSize * 4)
    {
        qint64 readSize = qMin(bytesToWrite, loadSize);

        outBlock = localFile.read(readSize);

        if (outBlock.isEmpty())
        {
            failTransfer(tr("讀取檔案內容失敗"));
            return;
        }

        qint64 sent = tcpClient.write(outBlock);

        if (sent != outBlock.size())
        {
            failTransfer(tr("傳送檔案內容失敗"));
            return;
        }

        bytesToWrite -= sent;
        outBlock.clear();
    }

    // 更新進度條百分比
    int progress = 0;

    if (totalBytes > 0)
    {
        progress = static_cast<int>(
            100.0 * bytesWritten / totalBytes
            );
    }

    clientProgressBar->setValue(qMin(progress, 100));

    clientStatusLabel->setText(
        tr("已傳送 %1 Bytes").arg(bytesWritten)
        );

    // 所有資料已寫入 TCP socket
    if (bytesToWrite == 0 && bytesWritten >= totalBytes)
    {
        transferInProgress = false;
        localFile.close();

        clientProgressBar->setValue(100);
        clientStatusLabel->setText(
            tr("傳送完成：%1 Bytes").arg(bytesWritten)
            );

        startButton->setEnabled(true);
        openButton->setEnabled(true);

        tcpClient.disconnectFromHost();
    }
}


// 網路連線錯誤
void TcpFileSender::onSocketError(
    QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);

    if (transferInProgress)
    {
        failTransfer(
            tr("TCP 連線失敗：%1").arg(tcpClient.errorString())
            );
    }
}


// 統一處理傳輸失敗
void TcpFileSender::failTransfer(const QString &message)
{
    if (!transferInProgress)
        return;

    transferInProgress = false;

    localFile.close();
    tcpClient.abort();

    startButton->setEnabled(!fileName.isEmpty());
    openButton->setEnabled(true);

    clientStatusLabel->setText(tr("傳輸失敗"));

    QMessageBox::warning(
        this, tr("檔案傳送"), message
        );
}
