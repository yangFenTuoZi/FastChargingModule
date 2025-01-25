# Fast Charging Module

## 介绍

一个快充模块，允许用户自定义电流数据及需要修改的文件

本模块只有在检测到充电开始后才会写入电流数据，可以设置延迟及等待时间，给电池留出喘气时间，防止断充

（作者本人用别的快充模块+第三方电池经常断充）

## 下载

1. [Github Releases](https://github.com/yangFenTuoZi/FastChargingModule/releases)
2. 直接找作者要最新稳定、测试版

## 构建

构建需要使用Android Studio，也可以尝试移除对agp的依赖，我没能力，就不弄了

作者使用的Android Studio版本及环境：

```
Android Studio Ladybug Feature Drop | 2024.2.2
Build #AI-242.23726.103.2422.12816248, built on December 18, 2024
Runtime version: 21.0.5+13-b509.30 amd64 (JCEF 122.1.9)
VM: OpenJDK 64-Bit Server VM by JetBrains s.r.o.
Toolkit: sun.awt.windows.WToolkit
Windows 11.0
GC: G1 Young Generation, G1 Concurrent GC, G1 Old Generation
Memory: 2048M
Cores: 20
Registry:
ide.experimental.ui=true
i18n.locale=
Non-Bundled Plugins:
com.jetbrains.plugins.ini4idea (242.23726.110)
com.intellij.marketplace (242.24335)
com.intellij.zh (242.152)
Statistic (4.3.3)
com.shuzijun.markdown-editor (2.0.5)
de.achimonline.github_markdown_emojis (1.4.0)
com.developerphil.adbidea (1.6.19)
```

要构建直接运行fastCharging/build.gradle中的zipAll函数即可，输出文件夹fastCharging/release

## 版权

本项目基于[Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)协议开源，二改需注明原作者及原项目地址
