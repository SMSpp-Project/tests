for f in inst/sndlib/*.nod; 
do
echo $f >> inst.out
./SFDCRBlock_test $f
done
