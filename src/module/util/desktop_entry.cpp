/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "desktop_entry.h"

#include <QFileInfo>
#include <QSettings>
#include <QDebug>

namespace linglong {
namespace util {

class DesktopEntryPrivate
{
public:
    DesktopEntryPrivate(const QString &filePath, DesktopEntry *parent)
        : q_ptr(parent)
    {
        parse(filePath);
    }

    void parse(const QString &filePath)
    {
        enum LineType {
            GroupBegin,
            KeyValue,
            Empty,
            Comments,
        };

        auto lineType = [](const QString &line) -> LineType {
            for (auto const &c : line) {
                switch (c.toLatin1()) {
                case '#':
                    return Comments;
                case '[':
                    return GroupBegin;
                case ' ':
                    break;
                case '=':
                    return KeyValue;
                default:
                    return KeyValue;
                }
            }
            return Empty;
        };

        auto parseGroupKey = [](QString line) -> QString {
            line.remove(line.indexOf('['), 1);
            line.remove(line.indexOf(']'), 1);
            return line.trimmed();
        };

        auto parseKeyValue = [](const QString &line, QString &key, QString &value) {
            auto index = line.indexOf('=');
            key = line.left(index);
            value = line.right(line.length() - index - 1);
        };

        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCritical() << "open failed" << filePath;
            return;
        }

        QString groupKey;
        QTextStream in(&f);
        while (!in.atEnd()) {
            auto line = in.readLine();
            auto lt = lineType(line);
            QString key;
            QString value;

            switch (lt) {
            case GroupBegin:
                groupKey = parseGroupKey(line);
                break;
            case KeyValue:
                parseKeyValue(line, key, value);
                dataMaps[groupKey][key] = value;
                break;
            case Empty:
            case Comments:
                break;
            }
        }

        f.close();
    }

protected:
    QMap<QString, QMap<QString, QVariant>> dataMaps;

private:
    DesktopEntry *q_ptr = nullptr;

    Q_DECLARE_PUBLIC(DesktopEntry)
};

DesktopEntry::DesktopEntry(const QString &filePath) noexcept
    : dd_ptr(new DesktopEntryPrivate(filePath, this))
{
}

DesktopEntry::~DesktopEntry() = default;

QString DesktopEntry::rawValue(const QString &key, const QString &section, const QString &defaultValue)
{
    Q_D(DesktopEntry);
    return d->dataMaps.value(section).value(key).toString();
}

} // namespace util
} // namespace linglong
