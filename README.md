# Sekiro-DebugMenu-LangSwitch
## 简介

对原本的只狼开发者菜单加了自定义汉化功能。

由于开发者菜单的所有文字显示都来自程序内部定义，一次性全导出的话文本量太大而且还得进行清洗，所以目前更适合“用到哪汉化到哪”。这个项目的定位不是完整汉化补丁，而是一个**支持开发者菜单汉化显示、文本持续采集、页面导出辅助**的版本。

## 键鼠操作的注意事项
有个比较神奇的点就是，键鼠操作必须依托于mod引擎的`chaindll`加载功能，就是使用mod引擎去加载开发者菜单，否则键鼠没法操作

## 数据目录说明

所有相关数据都会保存在 [`dbgmenu_data`](dbgmenu_data) 目录下：

- [`dbgmenu.ini`](dbgmenu_data/dbgmenu.ini)：功能开关配置文件
- [`zh-cn.json`](dbgmenu_data/zh-cn.json)：汉化映射表
- [`menu_text.txt`](dbgmenu_data/menu_text.txt)：持续采集到的菜单文本
- [`dbgmenu.log`](dbgmenu_data/dbgmenu.log)：运行日志
- [`dbgmenu_boot.log`](dbgmenu_data/dbgmenu_boot.log)：启动和 Hook 安装阶段日志
- [`page_dump.txt`](dbgmenu_data/page_dump.txt)：按页面导出的文本快照
- [`filter_rules.txt`](dbgmenu_data/filter_rules.txt)：文本过滤规则

---

## debugmenu.ini配置说明

### 汉化显示模式

如果你想控制菜单里文字的显示方式，请修改 [`DisplayMode`](dbgmenu_data/dbgmenu.ini:10)。

支持三种模式：
- `full`：只显示汉化文本
- `mix`：汉化和原文同时显示
- `off`：关闭汉化，直接显示原文

---

## 汉化文本补充流程

1. 在 [`dbgmenu.ini`](dbgmenu_data/dbgmenu.ini) 中把 [`PageExport`](dbgmenu_data/dbgmenu.ini:11) 设置为 `on`
2. 打开开发者菜单并停留在目标页面（注意只会导出你能肉眼实际看到的部分，比较长的话记得把菜单也拉长让文本完全显示出来）
3. 按 键盘的`P`键
4. 查看 [`page_dump.txt`](dbgmenu_data/page_dump.txt)
5. 将需要的文本整理后写入 [`zh-cn.json`](dbgmenu_data/zh-cn.json)并写入对应汉化文本

---

## tip
- 我不打算自己再增加更多的汉化，如果你想为这个项目增加更多的汉化，可以自己 fork 一个然后提 PR，或者修改 [`zh-cn.json`](dbgmenu_data/zh-cn.json) 后在自己的仓库发布 release

## credit

- 原作者：[LukeYui](https://github.com/LukeYui)
- [阿里巴巴普惠体](https://www.alibabafonts.com/#/home)
- [mojo](https://github.com/mojo-lv/ModEngine)：其实原本在文本按键导出这块这块卡了很久，不过看到确实有人做了，就又尝试做了，这下功能算是完整了
