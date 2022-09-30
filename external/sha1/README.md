# SHA1

```cpp
void example(){
    const char *text = "quick brown fox jumps over the lazy dog";

    char hex[SHA1_HEX_SIZE];
    char base64[SHA1_BASE64_SIZE];

    // constructor can be empty or take a const char*
    sha1("The ")
        // can be chained
        // can add single chars
        .add(text[0])
        // number of bytes
        .add(&text[1], 4)
        // 0-terminated const char*
        .add(&text[5])
        // finalize must be called, otherwise the hash is not valid
        // after that, no more bytes should be added
        .finalize()
        // print the hash in hexadecimal, 0-terminated
        .print_hex(hex)
        // print the hash in base64, 0-terminated
        .print_base64(base64);

    printf("SHA1(The quick brown fox jumps over the lazy dog)\n");
    printf("\n");
    printf("hexadecimal\n");
    printf("calculated: %s\n", hex);
    printf("expected  : 2fd4e1c67a2d28fced849ee1bb76e7391b93eb12\n");
    printf("\n");
    printf("base64 encoded\n");
    printf("calculated: %s\n", base64);
    printf("expected  : L9ThxnotKPzthJ7hu3bnORuT6xI=\n");
}
```
