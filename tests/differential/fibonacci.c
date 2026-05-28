int main() {
    int a = 0, b = 1;
    for (int i = 2; i <= 10; i++) {
        int temp = a + b;
        a = b;
        b = temp;
    }
    return b; // 55
}
