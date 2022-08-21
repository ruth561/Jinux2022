# RTL8139の実装メモ


### PCIデバイスとしての操作
(0x00w) vendor ID: 0x10EC  
(0x02w) device ID: 0x8139  
(0x04w) command
(0x10d) IO Address Register: IOアドレス空間にマップされたOperational Registerのオフセット（256byteアラインメント）
(0x14d) MEMory Address Register: メモリにマップされたOperational Registerの先頭アドレス（256byteアラインメント）


### 参考文献
RTL8139Dデータシート  
https://www.cs.usfca.edu/~cruse/cs326f04/RTL8139D_DataSheet.pdf  

