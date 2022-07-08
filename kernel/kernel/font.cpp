#include "font.hpp"
#include <stddef.h>



/* これら3つの変数は、hankaku.o内で定義されたシンボルである。
   hankaku.oには、各文字のフォントを表す16bytesのデータの配列が
   詰まっている。
   c番目の文字のデータのベースアドレスは、以下の式によって導出される。
            _binary_hankaku_bin_start + 16 * c
   このデータがあるのは、オブジェクトファイルにあり、リンク時に
   具体的なアドレスが紐付けられる。 */      
extern const uint8_t _binary_hankaku_bin_start;
extern const uint8_t _binary_hankaku_bin_end;
extern const uint8_t _binary_hankaku_bin_size;


/* 引数で受け取った文字のフォントデータのあるアドレスを返す
   フォントデータは、縦16bit横8bitのデータ形式なので、
   一文字あたり16*1=16bytes使用している。
   外部からの参照は想定していないという認識でいいか？
   その場合、staticをつけたほうがいいと思うのだが。。 */
const uint8_t* GetFont(char c)
{
    uint8_t *res = (uint8_t *) (&_binary_hankaku_bin_start + 16 * (unsigned int) c);
    if ((uint64_t) res > (uint64_t) &_binary_hankaku_bin_end) {
        return NULL;
    }
    return res;
}


/* (x, y) の位置に文字cを書き込む。文字の色は、colorで指定できる。 */
void WriteAscii(PixelWriter* writer, int x, int y, char c, const PixelColor *color)
{
    const uint8_t *kFont = GetFont(c);
    if (kFont == NULL) return;
    for (int dx = 0; dx < 8; dx++) {
        for (int dy = 0; dy < 16; dy++) {
            if ((kFont[dy] << dx) & 0x80u) {
                writer->Write(x + dx, y + dy, color);
            }
        }
    }
}


void WriteString(PixelWriter *writer, int x, int y, const char* s, const PixelColor *color)
{
    for (int i = 0; *s != '\0'; i++, s++) {
        WriteAscii(writer, x + 8 * i, y, *s, color);
    }
}