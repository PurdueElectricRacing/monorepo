# Linear algebra

Linear algebra with no fancy optimizations or frivolous features.

`vec3_t` provides:
- addition
- subtraction
- scaling
- magnitude
- normalization

`mat3_t` provides:
- addition
- subtraction
- scaling
- matrix and vector multiplication
- column extraction
- outer product
- symmetrization
- Cholesky factorization

`cholesky3()` requires an SPD matrix and does not throw

$$
\text{Result} =
\begin{bmatrix}
    1 & 0 & 0 \\
    0 & 1 & 0 \\
    0 & 0 & 1
\end{bmatrix}
\begin{bmatrix}
    1 \\
    2 \\
    3
\end{bmatrix}

$$
```c
mat3_t matrix = {{
    {1, 0, 0},
    {0, 1, 0},
    {0, 0, 1},
}};
vec3_t vector = {
    1.0f,
    2.0f,
    3.0f,
};
vec3_t result = mat3_mult_vec3(&matrix, &vector);
```
