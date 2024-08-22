/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "common.h"

namespace linglong::util {

std::string str_vec_join(const str_vec &vec, char sep) {
  if (vec.empty()) {
    return "";
  }

  std::string s;
  for (auto iterator = vec.begin(); iterator != std::prev(vec.end());
       ++iterator) {
    s += *iterator + sep;
  }
  s += vec.back();
  return s;
}

}  // namespace linglong::util
