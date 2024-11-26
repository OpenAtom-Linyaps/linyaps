<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Search Apps From Remote

Use `ll-cli search` to search app meta info from remote repository.

View the help information for the `ll-cli search` command:

```bash
ll-cli search --help
```

Here is the output:

```text
Search the applications/runtimes containing the specified text from the remote repository
Usage: ll-cli search [OPTIONS] KEYWORDS

Example:
# find remotely app by name                                                                                                                                                                     ll-cli search org.deepin.demo
# find remotely runtime by name
ll-cli search org.deepin.base --type=runtime
# find all off app of remote
ll-cli search .
# find all off runtime of remote
ll-cli search . --type=runtime

Positionals:
  KEYWORDS TEXT REQUIRED      Specify the Keywords

Options:
  -h,--help                   Print this help message and exit                                                                                                                                    --help-all                  Expand all help
  --type TYPE [app]           Filter result with specify type. One of "runtime", "app" or "all"
  --dev                       include develop application in result

If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```

Use `ll-cli search` to search app meta info from remote repository and local cache:

```bash
ll-cli search calculator
```

This command returns the info of all apps whose `appid` (appid is the app's unique identifier) contains the keyword "calculator", including the complete `appid`, application name, version, CPU architecture and descriptions.

Here is the output:

```text
id                               name                             version         arch        channel         module      description
com.github.matrix-calculator.linyaps com.github.matrix-calculator     3.3.0.0         x86_64      main            runtime     convert from 3.3 build uos https://chinauos.com \Pyth...
io.github.Calculator             Calculator                       0.0.1.0         x86_64      main            runtime     C++ GUI Calculator app.
io.github.Calculator             Calculator                       0.0.1.1         x86_64      main            runtime     C++ GUI Calculator app.
io.github.cupcalculator          cupcalculator                    1.1.2.0         x86_64      main            runtime     A program which calculates scorings from orienteering...
io.github.cyber-calculator       cyber-calculator                 1.0.0.0         x86_64      main            runtime     CyberOS Calculator.
io.github.galaxy-calculator      galaxy-calculator                0.0.2.0         x86_64      main            runtime     Galaxy Calculator: Given the size of the Galaxy you c...
io.github.Huawei_modem_calculator_v2 Huawei_modem_calculator_v2       1.0.0.0         x86_64      main            runtime     Is it Huawei modem unlock code calculator from forth3...
io.github.QT_GraphingCalculator  QT_GraphingCalculator            0.0.1.0         x86_64      main            runtime     Graphing calculator using QT and QTcustomplot
io.github.Qt-calculator          Qt-calculator                    1.0.0.0         x86_64      main            runtime     A simple GUI calculator🧮 built using C++.
io.github.SolarCalculator        SolarCalculator                  1.0.0.0         x86_64      main            runtime     Small app to calculate sunrise and sunset based on da...
io.github.StringCalculator       StringCalculator                 0.0.1.0         x86_64      main            runtime     Full-featured calculator written in C++ with Qt frame...
```

If you need to look up Base and Runtime, you can use the following command:

```bash
ll-cli search . --type=runtime
```
