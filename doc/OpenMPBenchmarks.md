# OpenMP Benchmarks
This document outlines the results of benchmarks concerning the OpenMP x TAFFO integration.

## Polybench-ACC
To evaluate the integration's performance, a properly modified version of [Polybench 3.2 with OpenMP pragmas included](https://github.com/cavazos-lab/PolyBench-ACC/tree/master/OpenMP) is used. 
Note that most benchmarks in the original repository have been modified, not only by adding annotations but by fixing syntactical and logical errors in the usage of OpenMP.

Moreover, a not-negligible part of the benchmarks has not only syntactical but also logical errors, when compiling with plain clang. Therefore, they have been excluded and deleted.
The deleted benchmarks, with the related motivation, are listed on the specific [benchmark README](https://github.com/HEAPLab/TAFFO-test/blob/openmp/polybench-c-openmp/README.md).

### Quantitative results
The results obtained with the medium dataset on an 4 core Intel Core i5 4440 are as follows:

|                 | error_percentage               | error_absolute                | fixed_time     | floating_time     | speedup           | 
|-----------------|--------------------------------|-------------------------------|----------------|-------------------|-------------------| 
| covariance      | 9.51816227834747E-05           | 41047797.5653472              | 5.681801       | 5.590435          | 0.983919535372675 | 
| 2mm             | 8.85E-09                       | 89916908.3609044              | 15.878777      | 20.033046         | 1.26162399031109  | 
| 3mm             | 7.45080473244552E-05           | 1092134343786.54              | 4.138677       | 5.255662          | 1.26988938735736  | 
| doitgen         | 0.00146711757481               | 1.90287227630615              | 0.636815       | 0.756243          | 1.18753955230326  | 
| gemm            | 8.85E-09                       | 255.753626909256              | 7.059071       | 9.603164          | 1.36040053995774  | 
| symm            | 5.79050262189691               | 40478069464911.5              | 10.695635      | 12.580137         | 1.17619355933519  | 
| syr2k           | 8.85E-09                       | 511.513935213089              | 4.526563       | 6.161568          | 1.3612023073577   | 
| syrk            | 8.85E-09                       | 255.753072862625              | 2.243197       | 3.073655          | 1.37021180039025  | 
| floyd-warshall  | 0                              | 0.00E+00                      | 35.587033      | 34.688018         | 0.974737568034963 | 
| reg_detect      | 0.061994358513375              | 0.555555555555555             | 0.063146       | 0.065715          | 1.04068349539163  | 
| fdtd-2d         | 6.65355579516224E-05           | 0.000154133333333             | 1.901488       | 1.747293          | 0.918908244490631 | 
| jacobi-1d-imper | 0.000404369938798              | 2.02043675458368E-06          | 0.00227        | 0.002037          | 0.897356828193833 | 
| jacobi-2d-imper | 0.000156162867696              | 0.00039119                    | 0.102793       | 0.093422          | 0.908836204799937 | 
| seidel-2d       | 7.85984582155418E-05           | 0.00019689                    | 0.375291       | 0.318882          | 0.849692638512514 | 

MEDIUM_DATASETS have been used, since most benchmarks are currently failing (with pure Clang) using bigger datasets.

Possible explanation for the lower-than-expected speed-ups are:
- the benchmarks used are based on an outdated version of Polybench with respect to the other one currently used in TAFFO.
- the OpenMP directives in the adapted OpenMP-Polybench are not always correctly used, leading to far different results compared to the serial version.
 In particular, the parallel benchmarks generate numbers with larger order-of-magnitude, requiring more precision in fixed-points and often reaching overflows.

## Other benchmarks
A few other OpenMP benchmarks with floating-points that could be adapted are:
- http://rodinia.cs.virginia.edu/doku.php
- https://gitlab.pnnl.gov/perf-lab-hub/perfect/perfect-suite/-/tree/master/suite
