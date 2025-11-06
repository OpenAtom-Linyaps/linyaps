% ll-cli-search 1

## NAME

ll\-cli\-search - 从远程仓库中搜索包含指定关键词的应用程序/运行时

## SYNOPSIS

**ll-cli search** [*options*] _keywords_

## DESCRIPTION

`ll-cli search` 命令可以查询如意玲珑远程仓库中的应用信息。该命令用于从远程仓库中搜索包含指定关键词的应用程序、运行时或基础环境。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**--type** _TYPE_ [*all*]
: 按指定类型过滤结果。可选类型："runtime"、"base"、"app"或 "all"。

**--repo** _REPO_
: 指定仓库

**--dev**
: 结果包括应用调试包

**--show-all-version**
: 显示应用，base或runtime的所有版本

## POSITIONAL ARGUMENTS

**KEYWORDS** _TEXT_ _REQUIRED_
: 指定搜索关键词

## EXAMPLES

通过 `ll-cli search` 命令可以从远程 repo 中查找应用程序信息:

```bash
ll-cli search calculator
```

该命令将返回包含 calculator 关键词的所有应用程序信息，包含完整的 `appid`、应用程序名称、版本、平台及应用描述信息。

`ll-cli search calculator` 输出如下：

```text
ID                                         名称                             版本            渠道            模块        仓库      描述
com.bixense.passwordcalculator             com.bixense.passwordcalculator   1.1.0.0         main            binary      nightly   flatpak runtime environment on linglong
com.expidusos.calculator                   com.expidusos.calculator         0.1.1.0         main            binary      nightly   flatpak runtime environment on linglong
dev.edfloreshz.calculator                  dev.edfloreshz.calculator        0.1.1.0         main            binary      nightly   flatpak runtime environment on linglong
io.github.deaen.awesomecalculator          io.github.deaen.awesomecalculator 2.2.0.0         main            binary      nightly   flatpak runtime environment on linglong
org.deepin.calculator                      deepin-calculator                6.5.26.1        main            binary      nightly   Calculator for UOS
org.gnome.calculator                       org.gnome.calculator             48.1.0.0        main            binary      nightly   flatpak runtime environment on linglong
uno.platform.uno-calculator                uno.platform.uno-calculator      1.2.9.0         main            binary      nightly   flatpak runtime environment on linglong
com.github.matrix-calculator.linyaps       com.github.matrix-calculator     3.3.0.0         main            runtime     stable    convert from 3.3 build uos https://chinauos.com \Pyth...
com.github.thatonecalculator.discordrpcmaker discordrpcmaker                  2.1.1.0         main            binary      stable    使用按钮制作和管理自定义 Discord 丰富存在的最佳方法！ 。
com.poki.true-love-calculator              True Love Calculator             34.4.1.20250423 main            binary      stable    True Love Calculator is an online mini-game provided ...
com.poki.your-love-calculator              Your Love Calculator             34.4.1.20250423 main            binary      stable    Your Love Calculator is an online mini-game provided ...
dev.edfloreshz.calculator                  dev.edfloreshz.calculator        0.1.1.0         main            binary      stable    flatpak runtime environment on linglong
io.github.Calculator                       Calculator                       0.0.1.1         main            runtime     stable    C++ GUI Calculator app.
io.github.Huawei_modem_calculator_v2       Huawei_modem_calculator_v2       1.0.0.0         main            runtime     stable    Is it Huawei modem unlock code calculator from forth3...
io.github.QT_GraphingCalculator            QT_GraphingCalculator            0.0.1.0         main            runtime     stable    Graphing calculator using QT and QTcustomplot
io.github.Qt-calculator                    Qt-calculator                    1.0.0.0         main            runtime     stable    A simple GUI calculator🧮 built using C++.
io.github.SolarCalculator                  SolarCalculator                  1.0.0.0         main            runtime     stable    Small app to calculate sunrise and sunset based on da...
io.github.StringCalculator                 StringCalculator                 0.0.1.0         main            runtime     stable    Full-featured calculator written in C++ with Qt frame...
io.github.cupcalculator                    cupcalculator                    1.1.2.0         main            runtime     stable    A program which calculates scorings from orienteering...
io.github.cyber-calculator                 cyber-calculator                 1.0.0.0         main            runtime     stable    CyberOS Calculator.
io.github.euduardop.faultcalculator-app    io.github.euduardop.faultcalc... 31.7.24.0       main            binary      stable    appimage convert
io.github.galaxy-calculator                galaxy-calculator                0.0.2.0         main            runtime     stable    Galaxy Calculator: Given the size of the Galaxy you c...
io.github.johannesboehler2.BmiCalculator   io.github.johannesboehler2.Bm... 1.3.1.0         main            binary      stable    Calculate your body mass index from weight, height an...
org.deepin.calculator                      deepin-calculator                6.5.26.1        main            binary      stable    Calculator for UOS
uno.platform.uno-calculator                uno.platform.uno-calculator      1.2.9.0         main            binary      stable    flatpak runtime environment on linglong
```

如果需要查找远程所有的 base 可以使用以下命令：

```bash
ll-cli search . --type=base
```

如果需要查找远程所有的 runtime 可以使用以下命令：

```bash
ll-cli search . --type=runtime
```

按名称搜索远程 base:

```bash
ll-cli search org.deepin.base --type=base
```

按名称搜索远程 runtime:

```bash
ll-cli search org.deepin.runtime.dtk --type=runtime
```

搜索远程所有软件包:

```bash
ll-cli search .
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-list(1)](./list.md)**, **[ll-cli-install(1)](./install.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
