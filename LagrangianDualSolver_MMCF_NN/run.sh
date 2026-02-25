for i in 1 2 3 4 5 6 7 8 9 10; do for j in 0 1 2 3 4 5; do  sed -i '140s/.*/dbltStar   1e+'${i}'/' BSPar.txt;  sed -i '151s/.*/dbltInit   1e-'${j}'/' BSPar.txt; for f in ./data/CanadN/pN[1,2,3,4].*;  do   ./LDS_MMCF_test ${f} s 5 ; done; mv times/ redCosts/ logs/ logTimes/ tStarZero/1e-${j}/1e+${i}/; mkdir times/ redCosts/ logs/ logTimes/; done; done

