
if [ ! -d "examples/bin" ]; then
    mkdir "examples/bin"
fi

shopt -s nullglob
for i in examples/*.c; do
    base=$(basename "$i")
    filename="${base%.*}"
    gcc cjson/cjson.c "$i" -I. -O3 -Wall -Wextra -Wpedantic -g -o "examples/bin/$filename"
    printf "Compiled $i --> examples/bin/$filename\n"
done
