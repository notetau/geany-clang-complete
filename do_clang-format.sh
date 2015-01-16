#!/bin/bash
# code formatting by clang-format

files=`find -name *.*pp`
for file in ${files}
do
clang-format -style=file $file > $file
done
