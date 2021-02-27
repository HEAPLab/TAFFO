# TAFFO OpenMP Support
TAFFO has preliminary support for [OpenMP](https://www.openmp.org) parallel constructs.
The goals of the integration are the following:
- The usage of a simple workflow to easily patch the runtime OpenMP function during optimizations,
 requiring the least amount of work necessary for intermediary passes.
- Basic support for the most common OpenMP directives, such as `omp parallel for`. 
 This allows TAFFO to convert the floating-point logic inside the supported OpenMP pragmas to fixed-point.
- Conservativeness in handling not supported directives, such as `omp task`, to avoid failures.
- Evaluate the results with benchmarks adapted to OpenMP and containing floating-points.

Note that a thorough analysis of the effects of data-dependency in the parallel execution is out of the scope of the integration.

**If you are a TAFFO user and not interested in the technical details**, note the following:
- No additional metadata is needed to integrate OpenMP code with TAFFO.
- The following constructs are tested and TAFFO correctly converts floating-point logic in fixed-point; do not trust the VRA though, and use `final` when possible:
    - `omp parallel` (with full support of shared variables)
    - `omp for` 
    - `omp section`
- The `omp reduce` is partially supported; existing code should not break and floating-point will be converted into fixed-points inside the OpenMP region,
 but the variables will be re-converted into floating-point before being reduced. 
- The `omp task` is currently not supported, therefore no conversion into fixed-points will happen in parallel regions with tasks.
 Note that the compiler will be conservative and no breaking optimizations will be made.

## Patching OpenMP Runtime
The OpenMP implementation of the LLVM Framework inserts various indirect functions to manage parallelism;
moreover, the functions are not contained in the source code that is being compiled.
Therefore, TAFFO can't natively follow the flow of execution.
 
The source below describes a simple OpenMP example program:
```c++
#include <stdio.h>

int main() {
  float container[16] __attribute((annotate("target('container') scalar()")));

#pragma omp parallel for shared(container) // container is a shared variable between the OpenMP threads
  for (int i = 0; i < 16; i++) {
    // Do parallel computation
    float result = i*0.05;
    
    // Save the result in the shared variable
    container[i] = result;
  }

  return 0;
}
```

A simplified `main` in the LLVM IR produced by TAFFO is as follows:

```
define dso_local i32 @main() #0 {
entry:
  // Setup variables
  // Setup OpenMP threads with OpenMP runtime library functins
  call void (%struct.ident_t*, i32, void (i32*, i32*, ...)*, ...) @__kmpc_fork_call(%struct.ident_t* %.kmpc_loc.addr, i32 1, void (i32*, i32*, ...)* bitcast (void (i32*, i32*, float*)* @.omp_outlined. to void (i32*, i32*, ...)*), float* %container)
  ret i32 0
}
```

The ```main``` contains only library functions and variables allocation.
The logic contained in the parallel region is outlined in another function, the ```.omp_outlined.```.
The function directly called is the ```__kmpc_fork_call```, whose source is not present in the LLVM IR.
Additionally, the ```__kmpc_fork_call``` receives the following arguments:
- Two parameters for internal OpenMP configurations.
- The reference to the ```.omp_outlined.```, where the actual logic is.
- A dynamic number of variables that corresponds to the reference to the shared variables.

### Taken approach
To bypass the runtime library functions, it is necessary to let TAFFO understand what functions will be called.

Although the integration started with the goal to instruct every pass to bypass the indirect function and analyze the outlined one,
the path was discarded since substantial modifications were required in every pass of TAFFO.
 
A simpler but more effective approach is currently implemented, with modifications only to the Initialization and Conversion passes.
Instead of resolving the indirections at each pass, the library functions are replaced with the outlined functions during the Initialization pass. 
Finally, during the last pass, the library function calls are re-inserted, accounting for the new fixed-point functions created by TAFFO.

The chosen approach revolves around so-called "trampolines". A trampoline is a custom function that directly calls an indirect function. 
This is convenient because it allows TAFFO to analyze the indirect calls as if they were direct calls, which are already supported.

#### Initialization pass
From the above example of the `@__kmpc_fork_call`, a corresponding `@__kmpc_fork_call_trampoline` is created and inserted in place of the original one. 
The body of this function is very simple and contains:
- the direct call to the outlined function, with the related arguments (the shared variables in OpenMP).
- an additional instruction that is used to preserve information about the original function. 
 In particular, the original function is saved in a local variable using a bitcast instruction. 
 This is necessary because otherwise the following optimization passes, such as Dead Code Elimination (DCE), 
 would remove the unused original function, making it impossible to reinsert the original calls in the Conversion pass.

#### Range Analysis and Data Type Allocation passes
TAFFO will regularly analyze the trampoline calls,  without any modification to these intermediate passes.

#### Conversion pass
At the end of the Conversion pass, it is necessary to transform back the trampolines into their original calls. 
To do so, the original function is picked from the saved bitcast and placed in place of the trampoline call, which will be deleted. 
The original function will have the relevant arguments replaced accordingly, specifically the indirect function to be called and the shared variables needed. 

In the above example, TAFFO will correctly convert the floating-point instructions in fixed-points, and
the call to `@__kmpc_fork_call` will contain `@.omp_outlined..2_fixp` instead of `@.omp_outlined.` and `i32* %foo.s6_26fixp` instead of `float* %foo`.

## Benchmarks
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

A few other OpenMP benchmarks with floating-points that could be adapted are:
- http://rodinia.cs.virginia.edu/doku.php
- https://gitlab.pnnl.gov/perf-lab-hub/perfect/perfect-suite/-/tree/master/suite

## Next Steps
The above sections describe where possible work can be done to reach a more complete integration of TAFFO and OpenMP.
### OpenMP reduce
The OpenMP reduction handling from Clang appears similar to the regular parallel code section handling.
A `@__kmpc_reduce` function (or `@__kmpc_reduce_nowait`) receives as arguments both the code to run and the data to work on. 

However, differently from the fork_call case, the data is passed as `void*`, which becomes `i8*` in LLVM IR. 
This becomes a problem because TAFFO is unable to apply any alias analysis on the reduction data, which is called `%.omp.reduction.red_list`. 

As a consequence, the code that corresponds to the reduction area maintains its usual data types, without any possible float to fixed-point conversion.

The problem could be tackled with: 
- targeted backtracking to find the usage of the reduction variable, which is a duplicated variable created from OpenMP and then inserted into the `%.omp.reduction.red_list`.
- enhancing the alias analysis to take into account the `%.omp.reduction.red_list` and the list of variables that will be passed to the reduce function(s).

### Loops inside parallel regions
The ValueRangeAnalysis pass is unable to correctly understand the range of trivial loops. A simple example could be as follows:
```c++
int main() {
    float foo __attribute((annotate("target('foo') scalar()"))) = 10;

    #pragma omp parallel for shared(foo)
    for (int i = 0; i < 5; i++) {
        #pragma omp critical
        foo += i;
    }
    printf("%f\n", foo);

    return 0;
}
```

The executable compiled with TAFFO prints 4.000000 instead of the expected 20.000000. 
The underlying problem is that the ScalarEvolution, used by the VRA to calculate the trip counts, fails to detect the range of the loop.

A simple fix for similar situations are:
- set manually a final range in the annotations: `scalar(range(0, 20) final)` is enough to "fix" the wrong range.
- manually set the ScalarEvolution trip-count for the VRA.
Note that, although the solution has been employed in the benchmarks, it's not scalable and greatly reduces the usefulness of the VRA.

The problem could be tackled with additional simplification passes, but they do not seem effective and 
risk creating additional problems for the TAFFO's passes.

### OpenMP tasks
OpenMP tasks aren't supported and cause errors in Conversion, specifically causing it to hit an assertion error (`assert(ip && "ip mandatory for non-instruction values");`).
In particular, the error is raised during the store of a float in an OpenMP struct, used for the shared variables in the task.

Although the behaviour has not been fixed, a workaround is present to manage them. 
To be conservative, TAFFO prevents the indirect parent functions from being converted if they contain tasks. 
This could have undesired behaviour in cases such as this one:
```
int main() {
    float task_variable __attribute((annotate("target('task_variable') scalar()"))) = 1.0f;
    float not_task_variable __attribute((annotate("target('not_task_variable') scalar()"))) = 363;
    #pragma omp parallel
    {
        #pragma omp single
        {
            #pragma omp task
            {
                task_variable *= 2.0f;
            }
            #pragma omp task
            task_variable *= 3.0f;
            
            not_task_variable += 4;
        }
    }
}
```
The variable `not_task_variable` will not be converted even if it isn't used in the tasks, but at least the Conversion pass will not crash.

The problem could be tackled in different ways:
- target specifically the variables used in the tasks (by analyzing the shared variables in the task OpenMP function).
 This could allow, in the example above, the conversion of `not_task_variable`, meanwhile keeping `task_variable` as a floating-point.
- supporting OpenMP tasks, in a similar way to the current `__kmpc_fork_call` support. 

## Additional links
During the development of the integration, another team paved way for [parallelism-aware optimizations in LLVM](https://www.openmp.org/wp-content/uploads/OpenMPOpt-in-LLVM-SC20-JD.pdf).

This could be extremely interesting in solving the problems, in case the aforementioned indirect functions will be substituted by direct functions.
