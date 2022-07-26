void main() {
    int i = 0;
    int s = 0;
    while (1) {
        for (int j = 0; j < 0x10000000; j++) {
            i+=j;
        }
        i %= 1000;
        s += *(int *)(0xffff800000000000 + i * 0x1234);
    }
}