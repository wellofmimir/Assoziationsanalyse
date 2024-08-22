#include <QCoreApplication>
#include <QString>
#include <QMap>
#include <QSet>

static QSet<QString> items;

static QVector<QStringList> transaktionen
{
    {"sunny", "hot", "high", "false", "no"},
    {"sunny", "hot", "high", "true", "no"},
    {"overcast", "hot", "high", "false", "yes"},
    {"rainy", "mild", "high", "false", "yes"},
    {"rainy", "cool", "normal", "false", "yes"},
    {"rainy", "cool", "high", "false", "no"},
    {"overcast", "cool", "normal", "true", "yes"},
    {"sunny", "mild", "high", "true", "no"},
    {"sunny", "cool", "normal", "false", "yes"},
    {"rainy", "mild", "high", "true", "yes"},
    {"sunny", "mild", "normal", "true", "yes"},
    {"overcast", "mild", "normal", "true", "yes"},
    {"overcast", "hot", "normal", "false", "yes"},
    {"rainy", "mild", "high", "true", "no"},
};

static QMap<QString, qreal> itemToSupport;

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

static QMap<QString, qreal> itempairNameToSupport;
static QMap<QString, QVector<Confidence> > nameToConfidences;
static QMap<QString, QVector<Lift> > nameToLifts;

int main(int argc, char *argv[])
{
    QCoreApplication a {argc, argv};

    for (const QStringList &transaktion : transaktionen)
        for (const QString &entry : transaktion)
            items << entry;

    for (const QString &item : items)
    {
        double itemCount {0.0};

        for (const QStringList &transaktion : transaktionen)
            itemCount += transaktion.count(item);

        itemToSupport.insert(item, itemCount / transaktionen.size());
    }

    for (const QString &firstItem : items)
    {
        QVector<Confidence> confidences;
        QVector<Lift> lifts;

        for (const QString &secondItem : items)
        {
            if (secondItem == firstItem)
            {
                confidences << Confidence {secondItem, 0.0};
                continue;
            }

            double itemCountFirstAndSecondItem {0.0};

            for (const QStringList &transaktion : transaktionen)
            {
                if (transaktion.contains(firstItem) && transaktion.contains(secondItem))
                    ++itemCountFirstAndSecondItem;
            }

            const qreal supportAB  {(itemCountFirstAndSecondItem / transaktionen.size())};
            const qreal supportA   {itemToSupport[firstItem]};
            const qreal supportB   {itemToSupport[secondItem]};

            const Confidence confidence {secondItem, supportAB / supportA};
            confidences << confidence;

            const Lift lift {secondItem, supportAB / (supportA * supportB)};
            lifts << lift;

            if (!itempairNameToSupport.contains(firstItem + ":" + secondItem) && !itempairNameToSupport.contains(secondItem + ":" + firstItem))
                itempairNameToSupport.insert(firstItem + ":" + secondItem, supportAB);
        }

        nameToConfidences.insert(firstItem, confidences);
        nameToLifts.insert(firstItem, lifts);
    }

    return a.exec();
}
