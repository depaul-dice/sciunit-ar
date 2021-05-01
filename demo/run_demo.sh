#!/bin/bash
echo -e "Running a demo of the lip archive and computediff functionality\n"
cd .. && cmake . && make
cd demo || { echo "Unable to change directory. Abort!"; exit 1; }
./liptar cf archive_test.lip test/
./liptar cf archive_test2.lip test2/
./computediff archive_test2.lip archive_test.lip