#include <QCoreApplication>
#include <QPair>
#include <QString>
#include <QMap>
#include <QSet>

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QScopedPointer>
#include <QSettings>
#include <QUuid>
#include <QDebug>
#include <QDir>

#include <QtConcurrent/QtConcurrent>
#include <QFutureInterface>
#include <QFuture>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QtHttpServer>
#include <QHostAddress>

struct Confidence
{
    QString name;
    double confidence;
};

struct Lift
{
    QString name;
    double lift;
};

int main(int argc, char *argv[])
{
    QCoreApplication app {argc, argv};

    const quint16 PORT {50000};

    const QScopedPointer<QHttpServer> httpServer {new QHttpServer {&app}};

    httpServer->route("/associationanalysis", QHttpServerRequest::Method::Get     |
                                              QHttpServerRequest::Method::Put     |
                                              QHttpServerRequest::Method::Head    |
                                              QHttpServerRequest::Method::Trace   |
                                              QHttpServerRequest::Method::Patch   |
                                              QHttpServerRequest::Method::Delete  |
                                              QHttpServerRequest::Method::Options |
                                              QHttpServerRequest::Method::Connect |
                                              QHttpServerRequest::Method::Unknown,
    [](const QHttpServerRequest &request) -> QFuture<QHttpServerResponse>
    {
        Q_UNUSED(request)

        return QtConcurrent::run([]()
        {
            return QHttpServerResponse
            {
                QJsonObject
                {
                    {"Message", "The used HTTP-Method is not implemented."}
                }
            };
        });
    });

    httpServer->route("/associationanalysis", QHttpServerRequest::Method::Post,
    [](const QHttpServerRequest &request) -> QFuture<QHttpServerResponse>
    {
        if (request.body().isEmpty())
            return QtConcurrent::run([]()
            {
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "HTTP-Request body is empty."}
                    }
                };
            });

        const QJsonDocument jsonDocument {QJsonDocument::fromJson(request.body())};

        if (jsonDocument.isNull())
            return QtConcurrent::run([]()
            {
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. Please send a valid JSON-Object."}
                    }
                };
            });

        const QJsonObject jsonObject {jsonDocument.object()};

        if (jsonObject.isEmpty())
            return QtConcurrent::run([]()
            {
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "Invalid data sent. Please send a valid JSON-Object."}
                    }
                };
            });

        const QVector<QPair<QString, QStringList> > transaktionen = [](const QJsonObject &jsonObject) -> QVector<QPair<QString, QStringList> >
        {
            QVector<QPair<QString, QStringList> > transaktionen;

            for (const QString &jsonKey : jsonObject.keys())
                transaktionen << QPair<QString, QStringList>{jsonKey, jsonObject.value(jsonKey).toVariant().toStringList()};

            return transaktionen;

        }(jsonObject);

        const QSet<QString> items = [](const QVector<QPair<QString, QStringList> > &transaktionen) -> QSet<QString>
        {
            QSet<QString> items;

            for (const QPair<QString, QStringList> &transaktion : transaktionen)
                for (const QString &transaktionsEntry : transaktion.second)
                    items << transaktionsEntry;

            return items;

        }(transaktionen);

        const QMap<QString, qreal> itemToSupport = [](const QSet<QString> &items, const QVector<QPair<QString, QStringList> > &transaktionen) -> QMap<QString, qreal>
        {
            QMap<QString, qreal> itemToSupport;

            for (const QString &item : std::as_const(items))
            {
                double itemCount {0.0};

                for (const QPair<QString, QStringList> &transaktion : transaktionen)
                    itemCount += transaktion.second.count(item);

                itemToSupport.insert(item, itemCount / transaktionen.size());
            }

            return itemToSupport;

        }(items, transaktionen);

        QMap<QString, qreal> itempairNameToSupport;
        QMap<QString, QVector<Confidence> > nameToConfidences;
        QMap<QString, QVector<Lift> > nameToLifts;

        for (const QString &firstItem : items)
        {
            QVector<Confidence> confidences;
            QVector<Lift> lifts;

            for (const QString &secondItem : items)
            {
                if (secondItem == firstItem)
                {
                    continue;
                    confidences << Confidence {secondItem, 0.0};
                }

                double itemCountFirstAndSecondItem {0.0};

                for (const QPair<QString, QStringList> &transaktion : transaktionen)
                {
                    if (transaktion.second.contains(firstItem) && transaktion.second.contains(secondItem))
                        ++itemCountFirstAndSecondItem;
                }

                const qreal supportAB  {(itemCountFirstAndSecondItem / transaktionen.size())};
                const qreal supportA   {itemToSupport[firstItem]};
                const qreal supportB   {itemToSupport[secondItem]};

                const Confidence confidence {secondItem, supportAB / supportA};
                confidences << confidence;

                const Lift lift {secondItem, supportAB / (supportA * supportB)};
                lifts << lift;

                //if (!itempairNameToSupport.contains(firstItem + ":" + secondItem) && !itempairNameToSupport.contains(secondItem + ":" + firstItem))
                    itempairNameToSupport.insert(firstItem + ":" + secondItem, supportAB);
            }

            nameToConfidences.insert(firstItem, confidences);
            nameToLifts.insert(firstItem, lifts);
        }

        const QJsonObject lifts = [](const QMap<QString, QVector<Lift> > &nameToLifts) -> QJsonObject
        {
            QJsonObject lifts;

            for (const QString &name : nameToLifts.keys())
            {
                QJsonArray jsonArray;

                for (const Lift &lift : nameToLifts.value(name))
                    jsonArray.append(QJsonObject{{lift.name, lift.lift}});

                lifts.insert(name, jsonArray);
            }

            return lifts;

        }(nameToLifts);

        const QJsonObject confidences = [](const QMap<QString, QVector<Confidence> > &nameToConfidences) -> QJsonObject
        {
            QJsonObject confidences;

            for (const QString &name : nameToConfidences.keys())
            {
                QJsonArray jsonArray;

                for (const Confidence &confidence : nameToConfidences.value(name))
                    jsonArray.append(QJsonObject{{confidence.name, confidence.confidence}});

                confidences.insert(name, jsonArray);
            }

            return confidences;

        }(nameToConfidences);

        const QJsonObject supports = [](const QMap<QString, qreal> &itempairNameToSupport, const QSet<QString> &items) -> QJsonObject
        {
            QJsonObject supports;

            for (const QString &itempairName : itempairNameToSupport.keys())
            {
                const QString firstItemName {itempairName.split(":").first()};

                QJsonArray jsonArray;

                for (const QString &item : items)
                {
                    if (itempairNameToSupport.keys().contains(firstItemName + ":" + item))
                        jsonArray.append(QJsonObject{{item, itempairNameToSupport.value(firstItemName + ":" + item)}});
                }

                supports.insert(firstItemName, jsonArray);
            }

            return supports;

        }(itempairNameToSupport, items);

        return QtConcurrent::run([=]()
        {
            return QHttpServerResponse
            {
                QJsonObject
                {
                    {"Confidences", confidences},
                    {"Lifts",       lifts},
                    {"Supports",    supports}
                }
            };
        });
    });

    if (httpServer->listen(QHostAddress::LocalHost, static_cast<quint16>(PORT)) == 0)
        return -1;

    return app.exec();
}
