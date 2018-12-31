/****************************************************************************
**
** Copyright (C) 2019 Ömer Göktaş
** Contact: omergoktas.com
**
** This file is part of the FastDownloaderTest project.
**
** The FastDownloader is free software: you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public License
** version 3 as published by the Free Software Foundation.
**
** The FastDownloader is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public
** License along with the FastDownloader. If not, see
** <https://www.gnu.org/licenses/>.
**
****************************************************************************/

#include <QCoreApplication>
#include <QBuffer>
#include <QFile>
#include <QDebug>
#include <QTimer>
#include <QElapsedTimer>
#include <fastdownloader.h>

void download()
{
    const QUrl downloadAddress("https://bit.ly/2EXm5LF");
    const QByteArray md5("b152270d004887ff5b474a19990aa110");
    const QString pathToSave("/Users/omergoktas/Desktop/Dragon and Toast.mp3"); //!!! Change this!

    auto buffer = new QBuffer;
    auto file = new QFile(pathToSave);
    auto downloader = new FastDownloader(downloadAddress, 3);
    auto timer = new QElapsedTimer;

    /*!
        It is 5 by default, and 6 (MAX_SIMULTANEOUS_CONNECTIONS) max. Check out fastdownloader.h
        file for more information.
     */
    downloader->setNumberOfSimultaneousConnections(FastDownloader::MAX_SIMULTANEOUS_CONNECTIONS);

    /*!
        This feature lets you set a chunk size limit, hence, the downloader will slice download
        chunks into pieces you choose. Each chunk finishes whenever they reach out the limit you select.
        Then another, a new connnection pops up and continue downloading form the end of the file.
        This allows users to hack and bypass some speed limits on some servers.
     */
    // downloader->setChunkSizeLimit(3145728); //! 3 Megabytes

    if (!downloader->start()) {
        qWarning("WARNING: Cannot start downloading for some reason");
        return;
    }

    qDebug() << "## Starting to download, address:" << downloadAddress;

    QObject::connect(downloader, &FastDownloader::redirected, [=] (const QUrl& redirectedUrl) {
        qDebug() << "## Redirected to " << redirectedUrl;
    });

    QObject::connect(downloader, &FastDownloader::resolved, [=] (const QUrl& resolvedUrl) {
        qDebug() << "## Destination resolved.";
        qDebug() << "   --> Resolved address:" << resolvedUrl;
        qDebug() << "   --> Content length:" << downloader->contentLength() << "bytes";
        qDebug() << "   --> Is simultaneous download possible:" << downloader->isSimultaneousDownloadPossible();
        qDebug() << "   --> Number of simultaneous connections:"
                 << (downloader->isSimultaneousDownloadPossible() ? downloader->numberOfSimultaneousConnections() : 1);
        buffer->open(QIODevice::WriteOnly);
        timer->start();
    });

    QObject::connect(downloader, &FastDownloader::readyRead, [=] (int id) {
        buffer->seek(downloader->head(id) + downloader->pos(id));
        buffer->write(downloader->readAll(id));
    });

    QObject::connect(downloader, QOverload<qint64,qint64>::of(&FastDownloader::downloadProgress),
                     [=] (qint64 bytesReceived, qint64 bytesTotal) {
        static int i = 0; ++i;
        if (i % 5 == 0) //! Just slow down the output flow a bit
            qDebug() << "## Download Progress: " << bytesReceived << "of" << bytesTotal << "bytes";
    });

    QObject::connect(downloader, QOverload<>::of(&FastDownloader::finished), [=] {
        buffer->close();
        if (downloader->bytesReceived() > 0) {
            file->open(QIODevice::WriteOnly);
            file->write(buffer->data());
            file->close();
        }
        const QByteArray& downloadedMd5 = QCryptographicHash::hash(buffer->data(), QCryptographicHash::Md5).toHex();
        qDebug() << "## All done!";
        qDebug() << "   --> Error occurred:" << downloader->isError();
        qDebug() << "   --> Do md5 hashes match:" << (md5 == downloadedMd5);
        qDebug() << "   --> Elapsed time:" << qRound(timer->elapsed() / 1000.) << "seconds";
        qDebug() << "   --> Data saved anway:" << (downloader->bytesReceived() > 0) << ","
                 << downloader->bytesReceived() << "bytes";
        qDebug() << "   --> Average download speed:"
                 << (downloader->bytesReceived() / 1024. / 1024.) / (timer->elapsed() / 1000.) << "Mb/sec";
        QCoreApplication::quit();
    });

    QObject::connect(downloader, QOverload<int>::of(&FastDownloader::finished), [=] (int id) {
        qDebug() << "## A chunk finished downloading, chunk id:" << id;
    });

    QObject::connect(downloader, QOverload<int, QNetworkReply::NetworkError>::of(&FastDownloader::error),
                     [=] (int id, QNetworkReply::NetworkError code) {
        qDebug() << "## An error occurred on chunk id:" << id << ", error code:" << code;
        qDebug() << "## Quitting in 2 seconds...";
        QTimer::singleShot(2000, QCoreApplication::instance(), &QCoreApplication::quit);
    });

    QObject::connect(downloader, &FastDownloader::sslErrors, [=] (int id, const QList<QSslError>& errors) {
        downloader->ignoreSslErrors(id); //! Do not call if you don't want to ignore and let connection fail
        qDebug() << "## Ssl errors ignored on chunk id:" << id << ", errors:" << errors;
    });
}

int main(int argc, char* argv[])
{
    QCoreApplication a(argc, argv);
    download();
    return a.exec();
}
