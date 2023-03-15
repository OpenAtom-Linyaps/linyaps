/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_PACKAGE_REF_REFACTOR_H_
#define LINGLONG_SRC_MODULE_PACKAGE_REF_REFACTOR_H_

#include <QString>

namespace linglong {
namespace package {
namespace refact {

namespace defaults {
extern const QString arch;
extern const QString module;
} // namespace defaults

// ${packageID}/${version}[/${arch}[/${module}]]
class Ref
{
public:
    explicit Ref(const QString &str);
    explicit Ref(const QString &packageID,
                 const QString &version,
                 const QString &arch = defaults::arch,
                 const QString &module = defaults::module);

    QString toString() const;
    bool isVaild() const;

    QString packageID;
    QString version;
    QString arch;
    QString module;

protected:
    static const char *const verifyRegexp;
};

} // namespace refact
} // namespace package
} // namespace linglong

#endif /* LINGLONG_SRC_MODULE_PACKAGE_REF_H_ */
