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

#include <QCryptographicHash>

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

const QByteArray rapidApiKey {"89f06b7e3da6ddb749650f874be57bbacc4e5f3d7e0e155f6abd50501dc2485fb83fe401b2e33888a38008f5138ef01150471a458f10a6bbf38dd498e7026839"};

int main(int argc, char *argv[])
{
    QCoreApplication app {argc, argv};

    const quint16 PORT {80};

    const QScopedPointer<QHttpServer> httpServer {new QHttpServer {&app}};

    httpServer->route("/ping", QHttpServerRequest::Method::Get,
    [](const QHttpServerRequest &request) -> QFuture<QHttpServerResponse>
    {
        qDebug() << "Ping verarbeitet";

        const bool requestIsFromRapidAPI = [](const QHttpServerRequest &request) -> bool
        {
            for (const QPair<QByteArray, QByteArray> &header : request.headers())
                if (header.first == "X-RapidAPI-Proxy-Secret" && QCryptographicHash::hash(header.second, QCryptographicHash::Sha512).toHex() == rapidApiKey)
                    return true;

            return false;

        }(request);

        if (!requestIsFromRapidAPI)
            return QtConcurrent::run([]()
            {
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "HTTP-Requests allowed only via RapidAPI-Gateway."}
                    }
                };
            });

        return QtConcurrent::run([]()
        {
            return QHttpServerResponse
            {
                QJsonObject
                {
                    {"Message", "pong"}
                }
            };
        });
    });

    httpServer->route("/shoppingcartanalysis", QHttpServerRequest::Method::Get    |
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
        const bool requestIsFromRapidAPI = [](const QHttpServerRequest &request) -> bool
        {
            for (const QPair<QByteArray, QByteArray> &header : request.headers())
            {
                if (header.first == "X-RapidAPI-Proxy-Secret" && QCryptographicHash::hash(header.second, QCryptographicHash::Sha512) == rapidApiKey)
                    return true;
            }

            return false;

        }(request);

        if (!requestIsFromRapidAPI)
            return QtConcurrent::run([]()
            {
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "HTTP-Requests allowed only via RapidAPI-Gateway."}
                    }
                };
            });

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

    httpServer->route("/shoppingcartanalysis", QHttpServerRequest::Method::Post,
    [](const QHttpServerRequest &request) -> QFuture<QHttpServerResponse>
    {
        qDebug() << "Anfrage von IP: " << request.remoteAddress().toString();

        const bool requestIsFromRapidAPI = [](const QHttpServerRequest &request) -> bool
        {
            for (const QPair<QByteArray, QByteArray> &header : request.headers())
            {
                if (header.first == "X-RapidAPI-Proxy-Secret" && QCryptographicHash::hash(header.second, QCryptographicHash::Sha512) == rapidApiKey)
                    return true;
            }

            return false;

        }(request);

        if (!requestIsFromRapidAPI)
            return QtConcurrent::run([]()
            {
                return QHttpServerResponse
                {
                    QJsonObject
                    {
                        {"Message", "HTTP-Requests allowed only via RapidAPI-Gateway."}
                    }
                };
            });

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

        const QJsonObject supports = [](const QMap<QString, qreal> &nameToSupport) -> QJsonObject
        {
            QJsonObject supports;

            for (const QString &name : nameToSupport.keys())
                supports.insert(name, nameToSupport.value(name));

            return supports;

        }(itemToSupport);

        const QJsonObject supportsBetweenItems = [](const QMap<QString, qreal> &itempairNameToSupport, const QSet<QString> &items) -> QJsonObject
        {
            QJsonObject supportsBetweenItems;

            for (const QString &itempairName : itempairNameToSupport.keys())
            {
                const QString firstItemName {itempairName.split(":").first()};

                QJsonArray jsonArray;

                for (const QString &item : items)
                {
                    if (itempairNameToSupport.keys().contains(firstItemName + ":" + item))
                        jsonArray.append(QJsonObject{{item, itempairNameToSupport.value(firstItemName + ":" + item)}});
                }

                supportsBetweenItems.insert(firstItemName, jsonArray);
            }

            return supportsBetweenItems;

        }(itempairNameToSupport, items);

        return QtConcurrent::run([=]()
        {
            return QHttpServerResponse
            {
                QJsonObject
                {
                    {"Confidences",         confidences},
                    {"Lifts",               lifts},
                    {"Supports",            supports},
                    {"Supportrelations",    supportsBetweenItems}
                }
            };
        });
    });

    if (httpServer->listen(QHostAddress::Any, static_cast<quint16>(PORT)) == 0)
        return -1;

    return app.exec();
}
