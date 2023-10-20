
#include "printer.h"

void PrinterJson::printError(const int code , const QString message)
{
    QJsonObject obj;
    obj["code"] = code;
    obj["message"] = message;
    qCritical().noquote() <<  QJsonDocument(obj);
}

void PrinterJson::printResult(linglong::utils::error::Result<QList<QSharedPointer<linglong::package::AppMetaInfo>>> result)
{
    QJsonObject obj;
    if (!result.has_value()){
        obj["code"] = 0;
        obj["message"] = QString("run command successful!");
        qInfo().noquote() <<  QJsonDocument(obj);
        return;
    }
    for (const auto &it : *result)
    {
        obj["appId"] = it->appId.trimmed();
        obj["name"] = it->name.trimmed();
        obj["version"] = it->version.trimmed();
        obj["arch"] = it->arch.trimmed();
        obj["kind"] = it->kind.trimmed();
        obj["runtime"] = it->runtime.trimmed();
        obj["uabUrl"] = it->uabUrl.trimmed();
        obj["repoName"] = it->repoName.trimmed();
        obj["description"] = it->description.trimmed();
        obj["user"] = it->user.trimmed();
        obj["size"] = it->size.trimmed();
        obj["channel"] = it->channel.trimmed();
        obj["module"] = it->module.trimmed();
    }
    qInfo().noquote() <<  QJsonDocument(obj);
}

void PrinterNormal::printError(const int code , const QString message)
{
     qCritical().noquote() << "code:" << code << "message:" << message;
}

/**
 * @brief 统计字符串中中文字符的个数
 *
 * @param name 软件包名称
 * @return int 中文字符个数
 */
static int getUnicodeNum(const QString &name)
{
    int num = 0;
    int count = name.count();
    for (int i = 0; i < count; i++) {
        QChar ch = name.at(i);
        ushort decode = ch.unicode();
        if (decode >= 0x4E00 && decode <= 0x9FA5) {
            num++;
        }
    }
    return num;
}

void PrinterNormal::printResult(linglong::utils::error::Result<QList<QSharedPointer<linglong::package::AppMetaInfo>>> result)
{
    if (result->size() > 0) {
        qInfo("\033[1m\033[38;5;214m%-32s%-32s%-16s%-12s%-16s%-12s%-s\033[0m",
              qUtf8Printable("appId"),
              qUtf8Printable("name"),
              qUtf8Printable("version"),
              qUtf8Printable("arch"),
              qUtf8Printable("channel"),
              qUtf8Printable("module"),
              qUtf8Printable("description"));
        for (const auto &it : result->toVector()) {
            QString simpleDescription = it->description.trimmed();
            if (simpleDescription.length() > 56) {
                simpleDescription = it->description.trimmed().left(53) + "...";
            }
            QString appId = it->appId.trimmed();
            QString name = it->name.trimmed();
            if (name.length() > 32) {
                name = it->name.trimmed().left(29) + "...";
            }
            if (appId.length() > 32) {
                name.push_front(" ");
            }
            int count = getUnicodeNum(name);
            int length = simpleDescription.length() < 56 ? simpleDescription.length() : 56;
            qInfo().noquote() << QString("%1%2%3%4%5%6%7")
                                   .arg(appId, -32, QLatin1Char(' '))
                                   .arg(name, count - 32, QLatin1Char(' '))
                                   .arg(it->version.trimmed(), -16, QLatin1Char(' '))
                                   .arg(it->arch.trimmed(), -12, QLatin1Char(' '))
                                   .arg(it->channel.trimmed(), -16, QLatin1Char(' '))
                                   .arg(it->module.trimmed(), -12, QLatin1Char(' '))
                                   .arg(simpleDescription, -length, QLatin1Char(' '));
        }
    } else {
        qInfo().noquote() << "app not found in repo";
    }

}