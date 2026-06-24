for f in inst/garr/*.nod; 
do
echo $f
./SFDCRBlock_test $f
done

for f in inst/sndlib/*.nod; 
do
echo $f
./SFDCRBlock_test $f
done

for f in inst/waxman/*.nod; 
do
echo $f
./SFDCRBlock_test $f
done

for f in inst/topo/*.nod; 
do
echo $f
./SFDCRBlock_test $f
done