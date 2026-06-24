for f in inst/garr/*.nod; 
do
echo $f >> inst.out
./SFDCRBlock_test $f
done
