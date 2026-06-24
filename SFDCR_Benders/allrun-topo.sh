for f in inst/topo/*.nod; 
do
echo $f >> inst.out
./SFDCRBlock_test $f
done
