#!/bin/bash

#
# kernel ディレクトリにあるカーネルをbuildする。
# devenv/buildenv.sh を実行することで、コンパイル・リンクに必要な環境変数の設定が行われる。
# ビルドの詳しい内容は、kernel/Makefileを参照。
#
source devenv/buildenv.sh

cd kernel
make
cd ..

#
# QEMU の実行。
# run_qemu.shでは、第一引数にefiファイルを、第二引数にその他のファイルを受け付けている。
# edk2のビルドが成功していれば、~/edk2/Build/JinuxLoaderX64/DEBUG_CLANG38/X64/Loader.efiに
# OSブートローダの実行ファイルが格納されているはずである。
#
# スクリプト内部では、qemuを実行する前に同ディレクトリにあるmake_image.shと言うシェルスクリプトを実行している。
# そこで、第一引数のファイルを/EFI/BOOT/BOOTX64.efi という名前でコピーし、
# 第二引数以降を、ルート配下に同じ名前でコピーしている。
# これは、./mnt というディレクトリ内で行われ、ファイルシステムには FAT32 が使用されている。
# 最後に、作ったファイルシステムをイメージファイルに変換している。
#
# 大事なのは、ブート用の EFI ファイル以外は、ルート配下に置かれているということ。
#
devenv/run_qemu.sh ~/edk2/Build/Jinux2022LoaderX64/DEBUG_CLANG38/X64/Loader.efi kernel/kernel.elf

# 
# qemu を実行することで、disk.img というディスクイメージが作成される
# これを、mnt ディレクトリにマウントすることで、起動時に用いたディスクを
# 確認することができる。マウントは、ループバック機能を使用する。
#
# sudo mount -o loop disk.img mnt

#
# 確認が終わったら、アンマウントしておく
#
# sudo umount mnt
#
