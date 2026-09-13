badcmd | sort
echo "line one" > a.txt
echo "line two" > b.txt
cat < a.txt < b.txt
cat < missing.txt
echo hi > out1.txt > out2.txt
cat out1.txt
cat out2.txt
echo again >> out1.txt > out3.txt
cat out1.txt
cat out3.txt
printf "b\na\n" | sort
