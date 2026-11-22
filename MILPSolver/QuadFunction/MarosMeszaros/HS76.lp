\ HS76, from HS76.mat of qpbenchmark
Minimize
 obj: - 1.0 x1 - 3.0 x2 + 1.0 x3 - 1.0 x4
 + [ + 2.0 x1^2 - 2.0 x1 * x3 + 1.0 x2^2 + 2.0 x3^2 + 2.0 x3 * x4 + 1.0 x4^2
  ] / 2
Subject To
 c1_hi: + 1.0 x1 + 2.0 x2 + 1.0 x3 + 1.0 x4 <= 5.0
 c2_hi: + 3.0 x1 + 1.0 x2 + 2.0 x3 - 1.0 x4 <= 4.0
 c3_lo: + 1.0 x2 + 4.0 x3 >= 1.5
Bounds
 0.0 <= x1
 0.0 <= x2
 0.0 <= x3
 0.0 <= x4
End
