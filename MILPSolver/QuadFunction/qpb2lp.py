#!/usr/bin/env python3
"""
Writes as LP files, in the format AbstractBlock::read_lp() reads, the
convex QPs of two test sets of qpbenchmark (https://github.com/qpsolvers):
the .npz of mpc_qpbenchmark, problems of model predictive control in
robotics, and the .mat of maros_meszaros_qpbenchmark, the Maros-Meszaros
test set. Both are under the Apache License 2.0.

A .npz holds the problem in the form of qpsolvers,

  min 1/2 x^T P x + q^T x  s.t.  G x <= h ,  A x = b ,  lb <= x <= ub ,

where G, A, lb and ub may be missing, and a .mat (written by sif2mat.m of
proxqp_benchmark) the form

  min 1/2 x^T P x + q^T x + r  s.t.  l <= A x <= u ,

where the last n rows of A are the identity, i.e., they are the bounds, and
1e20 is infinity. Every column is written in the Objective, also when its
cost is zero, so that the reader numbers them as the file does; the
quadratic part goes in [ ... ] / 2, with the diagonal of P and twice its
off-diagonal entries; a row with two different finite sides is written as
two rows, <name>_hi and <name>_lo, as AbstractBlock::write_lp() does, and
a column whose two bounds are equal is fixed by "x<j> = v" in the Bounds.

usage: qpb2lp.py <output directory> <file.npz | file.mat> ...
"""

import os
import sys

import numpy as np
import scipy.io as spio
import scipy.sparse as spa

INF = 1e20


def num(v):
    """A number as the LP file wants it, the shortest exact repr."""
    return repr(float(v))


def linear(out, coefs, first):
    """Writes the terms of a sparse row, coefs a list of (column, value)."""
    k = 0
    for j, v in coefs:
        if v == 0 and not first:
            continue
        sgn = "-" if v < 0 else "+"
        out.write(f" {sgn} {num(abs(v))} x{j + 1}")
        k += 1
        if k % 8 == 0:
            out.write("\n ")


def write_lp(path, P, q, r, rows, lb, ub, title):
    """rows: list of (name, coefs, lhs, rhs) with lhs/rhs possibly +-inf."""
    n = q.size
    P = spa.triu(spa.csr_matrix(P)).tocoo()
    with open(path, "w") as out:
        out.write(f"\\ {title}\n")
        out.write("Minimize\n obj:")
        linear(out, list(enumerate(q)), True)
        if r:
            out.write(f" {'-' if r < 0 else '+'} {num(abs(r))}")
        if P.nnz:
            out.write("\n + [")
            k = 0
            for i, j, v in zip(P.row, P.col, P.data):
                if v == 0:
                    continue
                w = v if i == j else 2 * v
                sgn = "-" if w < 0 else "+"
                term = f"x{i + 1}^2" if i == j else f"x{i + 1} * x{j + 1}"
                out.write(f" {sgn} {num(abs(w))} {term}")
                k += 1
                if k % 6 == 0:
                    out.write("\n ")
            out.write(" ] / 2")
        out.write("\nSubject To\n")
        for name, coefs, lo, hi in rows:
            if not coefs:
                continue
            if lo == hi:
                out.write(f" {name}:")
                linear(out, coefs, False)
                out.write(f" = {num(hi)}\n")
                continue
            if hi < INF:
                out.write(f" {name}_hi:")
                linear(out, coefs, False)
                out.write(f" <= {num(hi)}\n")
            if lo > -INF:
                out.write(f" {name}_lo:")
                linear(out, coefs, False)
                out.write(f" >= {num(lo)}\n")
        out.write("Bounds\n")
        for j in range(n):
            lo, hi = lb[j], ub[j]
            if lo == hi:
                out.write(f" x{j + 1} = {num(lo)}\n")
            elif lo <= -INF and hi >= INF:
                out.write(f" x{j + 1} free\n")
            elif lo <= -INF:
                out.write(f" -infinity <= x{j + 1} <= {num(hi)}\n")
            elif hi >= INF:
                out.write(f" {num(lo)} <= x{j + 1}\n")
            else:
                out.write(f" {num(lo)} <= x{j + 1} <= {num(hi)}\n")
        out.write("End\n")


def sparse_rows(M):
    """The rows of a matrix as lists of (column, value)."""
    M = spa.csr_matrix(M)
    return [list(zip(M.indices[M.indptr[i]:M.indptr[i + 1]],
                     M.data[M.indptr[i]:M.indptr[i + 1]]))
            for i in range(M.shape[0])]


def clip(v):
    """+-inf, and +-1e20 or beyond, as +-INF."""
    v = np.asarray(v, dtype=float).flatten()
    v[v >= 9e19] = INF
    v[v <= -9e19] = -INF
    return v


def from_npz(path):
    z = np.load(path, allow_pickle=True)

    def get(k):
        v = z[k]
        return None if v.dtype == object and v.shape == () else v

    P, q = get("P"), get("q").astype(float)
    n = q.size
    rows = []
    if get("G") is not None:
        h = clip(get("h"))
        for i, c in enumerate(sparse_rows(get("G"))):
            rows.append((f"g{i + 1}", c, -INF, h[i]))
    if get("A") is not None:
        b = clip(get("b"))
        for i, c in enumerate(sparse_rows(get("A"))):
            rows.append((f"e{i + 1}", c, b[i], b[i]))
    lb = clip(get("lb")) if get("lb") is not None else np.full(n, -INF)
    ub = clip(get("ub")) if get("ub") is not None else np.full(n, INF)
    return P, q, 0.0, rows, lb, ub


def from_mat(path):
    m = spio.loadmat(path)
    n = int(m["n"].flatten()[0])
    P = spa.csc_matrix(m["P"]).astype(float)
    q = m["q"].flatten().astype(float)
    r = float(m["r"].flatten()[0]) if "r" in m else 0.0
    A = spa.csr_matrix(m["A"]).astype(float)
    l, u = clip(m["l"]), clip(m["u"])
    rows = [(f"c{i + 1}", c, l[i], u[i])
            for i, c in enumerate(sparse_rows(A[:-n]))]
    return P, q, r, rows, l[-n:], u[-n:]


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    outdir = sys.argv[1]
    os.makedirs(outdir, exist_ok=True)
    for path in sys.argv[2:]:
        name, ext = os.path.splitext(os.path.basename(path))
        P, q, r, rows, lb, ub = (from_npz if ext == ".npz" else from_mat)(path)
        write_lp(os.path.join(outdir, name + ".lp"), P, q, r, rows, lb, ub,
                 f"{name}, from {os.path.basename(path)} of qpbenchmark")
        print(name, q.size, len(rows))


if __name__ == "__main__":
    main()
