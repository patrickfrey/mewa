# Target Platforms

The tests currently use a LLVM IR target template file for the following platforms:

 * [Linux x86_64 (Intel 64 bit)](../examples/target/x86_64-pc-linux-gnu.tpl)
 * [Linux i686 (Intel 32 bit)](../examples/target/i686-pc-linux-gnu.tpl)

The target template files were created with the command
```sh
echo "int main(){}" | clang -x "c" -S -emit-llvm -o - -
```
And some editing, removing the ```datalayout```, minimizing processor capabilities declared (```target-features```) and replacing the main program declaration with the middle section of the existing target files with the variables to substitute when generating the compiler output. The configure scripts then tries to set your default target file to your platform. If you add a target platform you might have to update the configure script too.

Contributions for other platforms are appreciated and welcome.

