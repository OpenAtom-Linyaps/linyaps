#pragma once

#include <iosfwd>      // for ostream, basic_ostream
#include <string>      // for operator<<, string
#include <type_traits> // for enable_if, is_same

#include <QDebug>  // for QDebug
#include <QString> // for QString

template <typename T>
typename std::enable_if<!std::is_same<T, std::string>::value,
                        std::ostream &>::type
operator<<(std::ostream &os, const T &x)
{
        QString buf;
        {
                QDebug debug(&buf);
                debug << x;
        }
        os << buf.toStdString();
        return os;
}
