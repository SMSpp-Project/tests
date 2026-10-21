\ QPTEST, from QPTEST.mat of qpbenchmark
Minimize
 obj: + 1.5 x1 - 2.0 x2
 + [ + 8.0 x1^2 + 4.0 x1 * x2 + 10.0 x2^2 ] / 2
Subject To
 c1_lo: + 2.0 x1 + 1.0 x2 >= 2.0
 c2_hi: - 1.0 x1 + 2.0 x2 <= 6.0
Bounds
 0.0 <= x1 <= 20.0
 0.0 <= x2
End
