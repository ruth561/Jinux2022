# Jinux2022開発環境

### **説明**
seccamでOS自作をするための開発環境。元々ローカルで作っていたjinuxを、バージョン管理しながら開発していくように新しく作った。jinuxは、USBドライバの途中で開発が止まっており、ログの実装をしていなかったり、C++構文を極力使わなかったりと修正する所が山ほどあるので、適宜リファクタリングしながら進めていく。

Jinux2022のOSローダーは、元々作っていたjinuxのOSローダーを流用することにする。

### **buildの手順**
手元のPCに、edk2がダウンロードされていて（$HOME/edk2）、かつ$HOME/edk2/Jinux2022LoaderPkgからこのディレクトリへのシンボリックリンクが貼られていることを仮定する。

まず、$HOME/edk2/Conf/target.txtを以下のように編集する。
* ACTIVE_PLATFORM       = Jinux2022LoaderPkg/Jinux2022LoaderPkg.dsc
* TARGET                = DEBUG
* TARGET_ARCH           = X64
* TOOL_CHAIN_CONF       = Conf/tools_def.txt
* TOOL_CHAIN_TAG        = CLANG38
* BUILD_RULE_CONF = Conf/build_rule.txt

次に、コマンド`source $HOME/edk2/edksetup.sh`を実行し、
target.txtで設定した値を環境変数に設定する。cwdを$HOME/edk2に変更し、`build`を実行する。「Done」が表示されればビルドは成功である。成功していれば、$HOME/edk2/Build/JinuxLoaderX64/DEBUG_CLANG38/X64/Loader.efiというファイルが作成されているはずである。

ローダーのビルドが終わったら、`cd $HOME/Documents/Jinux2022/`を実行し、本ディレクトリに戻って来て、`./run.sh`を実行すればカーネルのビルドとQEMUによるエミュレートが実行される。run.shについて詳しくはファイルの中に説明してあるので、そちらを参考にする。
