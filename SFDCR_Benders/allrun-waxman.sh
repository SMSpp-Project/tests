for f in inst/waxman/*.nod; 
do
echo $f >> inst.out
./SFDCRBlock_test $f
done
