# logserver
logserver包含3个部分：syslogd、log接口及logwatch。其中log与syslogd之间是非阻塞UDP通讯。logwatch与syslogd之间是阻塞TCP通讯。logwatch从syslogd实时获取满足指定log信息，并在logwatch的标准输出显示。   
# syslogd
syslogd是在标准syslogd项目上增加了监听logwatch及分派满足条件的log的代码。使用方式同syslogd。  
# logwatch
logwatch是一个命令行工具，来获取指定的日志。
# log
log在这里只是一个示例，模拟日志的产生。


## 编译和安装
logserver依赖glib库，编译之前安装最新版本的glib即可([https://developer.gnome.org/glib/](https://developer.gnome.org/glib/ "https://developer.gnome.org/glib/"))。
在当前目录执行make，即可生成syslogd、log和logwatch。

## 使用
logserver:作为daemon运行。
log：删除main的代码后，包含在需输出日志的项目代码中。然后在项目中设置环境变量LIBLOGFAF_SERVER及LIBLOGFAF_PORT指定syslogd的host及port, port默认为514.
lowatch：作为应用运行。

## 示例


