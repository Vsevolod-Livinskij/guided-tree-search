The purpose of this file is to keep track of missing parts and questions that we
need to answer. In the future, answer to those should be moved to instruction 
file.


## Internal reduction
**TODO:**
A paper that explains implementation of internal reduction in Hypothesis:
[MacIver, D.R. and Donaldson, A.F., Test-case reduction via test-case generation: Insights from the hypothesis reducer (tool insights paper).](https://www.doc.ic.ac.uk/~afd/homepages/papers/pdfs/2020/ECOOP_Hypothesis.pdf)
Explanation of [Creduce approach](https://blog.regehr.org/archives/1678).
We need to analyse them to come up with a good API for internal reduction.
Ideally, reduction passes should be reusable by the mutator.


## Coverage-guided mutations
We want to be able to perform mutations on the choice sequence that are likely
to be localized. This means that a mutation has not to escape the scope where 
it was performed. It sounds like we need a way to align new and old choice
sequences.


## Connection to [YARPGen](https://github.com/intel/yarpgen)

There are two main goals that we want to achieve with the library: internal 
reduction and coverage-guided fuzzing.

### Library vs std::rand
Evaluation shows that the Default guide and the C++ standard library random 
generator have similar performance in terms of optimization counters and testing
speed.

### AFL++ integration
We will use AFL instead of AFL++ in this document.

AFL++ [highly recommends](https://aflplus.plus/docs/fuzzing_in_depth/#e-instrumenting-the-target)
to avoid instrumenting the shared libraries.
Therefore, don't forget to pass `-DBUILD_SHARED_LIBS=OFF` for LLVM.

#### Instrumentation
When testing optimization, we want to skip the instrumentation of the irrelevant
parts, such as parser. To do that, we can use [partial instrumentation](https://github.com/AFLplusplus/AFLplusplus/blob/stable/instrumentation/README.instrument_list.md#3a-how-to-use-the-partial-instrumentation-mode).

**TODO:**
There is a lot of different [options for instrumentation](https://github.com/AFLplusplus/AFLplusplus/blob/stable/instrumentation/README.llvm.md#3-options).
[Link-time instrumentation](https://github.com/AFLplusplus/AFLplusplus/blob/stable/instrumentation/README.lto.md)
looks like the most promising one, so we should start with it.

**TODO:**
Currently, we have problems with map density. We need to try [this](https://github.com/AFLplusplus/AFLplusplus/issues/1735)
and [this](https://aflplus.plus/docs/status_screen/#map-coverage). Also, a 
better instrumentation could probably help.

**TODO:**
Work on afl-instrumented gcc.

#### Coverage analysis
[afl-cov](https://github.com/mrash/afl-cov) looks like the most promising tool
to analyze coverage results.
Other alternatives are [afl-cov](https://github.com/axt/afl-cov) and 
[AFL-Cast](https://github.com/cloudfuzz/AFL-Cast), but they look less polished.

How to use afl-cov:
1. Download [afl-cov](https://github.com/mrash/afl-cov)
2. Build LLVM with gcov code coverage support:
```shell
CC=afl-clang-fast CXX=afl-clang-fast++ 
AFL_LLVM_ALLOWLIST=/home/vlivinsk/workspace/llvm/afl-allow-list.txt 
cmake -GNinja \
      ../llvm_src_cov/llvm \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_ENABLE_ASSERTIONS=ON \
      -DLLVM_ENABLE_PROJECTS="llvm;clang" \
      -DBUILTINS_CMAKE_ARGS=-DCOMPILER_RT_ENABLE_IOS=OFF \
      -DLLVM_TARGETS_TO_BUILD=X86 \
      -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage" \
      -DCMAKE_C_FLAGS="-fprofile-arcs -ftest-coverage" \
      -DCMAKE_LINK_FLAGS="-fprofile-arcs -ftest-coverage" \
      -DCMAKE_EXE_LINKER_FLAGS="-fprofile-arcs -ftest-coverage" \
      -DCMAKE_LD_FLAGS="-fprofile-arcs -ftest-coverage"
ninja opt
```
We probabyl don't really need all of the `-fprofile-arcs -ftest-coverage` flags,
but I'm not sure which ones are redundant.
3. From the build folder, create `in` and `out` folders, pu initial seed in `in`.
Essentially, perform all the steps that are necessary to run AFL.
4. Run AFL with `afl-cov`:
```shell
FILEGUIDE_COMMENT_PREFIX=\;\  \
FILEGUIDE_GENERATOR=/home/vlivinsk/workspace/alive2/build/quick-fuzz \
AFL_SKIP_CPUFREQ=1 \
AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
AFL_CUSTOM_MUTATOR_ONLY=1 \
AFL_CUSTOM_MUTATOR_LIBRARY=$HOME/workspace/guided-tree-search/aflplusplus/guide-gen.so \
~/workspace/afl-cov/afl-cov -d out/ --live --coverage-cmd "./bin/opt -O3 AFL_FILE" --code-dir . --ignore-core-pattern --coverage-at-exit
```
You can try to adjust when you want to collect the coverage information.
relevant options are `--cover-corpus` and `--coverage-at-exit`. Per queue entry
coverage looks like a bad idea, because it takes a really long time to collect
the coverage information for `opt`.
5. From the same directory, in a separate terminal, run `afl-fuzz` as usual:
```shell
AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
AFL_SKIP_CPUFREQ=1 \
FILEGUIDE_COMMENT_PREFIX=\;\  \
FILEGUIDE_GENERATOR=/home/vlivinsk/workspace/alive2/build/quick-fuzz \
AFL_CUSTOM_MUTATOR_ONLY=1 \
AFL_CUSTOM_MUTATOR_LIBRARY=$HOME/workspace/guided-tree-search/aflplusplus/guide-gen.so \
afl-fuzz -i ./in -o out -- ~/workspace/llvm/build-cov/bin/opt -O3 @@
```
6. After AFL finishes or gets killed, `afl-cov` will generate a report in `out/` 

It looks like `afl-cov` is a fancy wrapper around `gcov`, but that should be 
enough for our purposes.

#### Performance
The performance of instrumented clang is roughly half of the performance of
non-instrumented clang. On the other hand, the AFL++ does not have a big
performance impact on the fuzzing speed.

**TODO:** Add multi-core fuzzing.

#### Testing
**TODO:**
Add instruction on how to test yarpgen with AFL++.
Don't forget to pass `-x c++` to clang to tell it that `.cur_input` is a C++ 
file.

**TODO:**
Out goal is to use AFL++ to find miscompilations. To do that, we need to find a 
way to execute the test and evaluate execution results correctness.

First, we need to find a way to communicate the info about seed and test files
between AFL, yarpgen and a compiler. Here is a possible solution:
1. Create a random ID (RID) for each mutation sequence 
2. Create a `/tmp/RID`
3. Write the sequence to `/tmp/RID/seed.txt`
4. Fork, exec, call `yarpgen --choice-seq-load-file=/tmp/RID/seed.txt -o /tmp/RID/`
5. Fork, exec, call `clang++ /tmp/RID/func.cpp /tmp/RID/driver.cpp -I/tmp/RID/ -o /tmp/RID/a.out`
   I think that the coverage info will be propagated to the AFL if we instrument clang
6. Delete `/tmp/RID` to save some space

Second, we need to find a way to check if the execution results are correct.
One option is to use yarpgen's test self-check feature to check execution 
results. Another is to use differential testing.

**TODO:**
We need to check if coverage information is propagated through bash scripts or
fork-exec calls.

Here is how to run it:
```shell
AFL_DEBUG_CHILD=1 \
FILEGUIDE_COMMENT_PREFIX=\/\/\  \
FILEGUIDE_GENERATOR=/home/vlivinsk/workspace/yarpgen/build/yarpgen \
AFL_SKIP_CPUFREQ=1 \
AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
AFL_CUSTOM_MUTATOR_ONLY=1 \
AFL_CUSTOM_MUTATOR_LIBRARY=$HOME/workspace/guided-tree-search/aflplusplus/guide-gen.so \
afl-fuzz -t 10000 -i ./in -o out -- ~/workspace/llvm/build-afl/bin/clang++ -O3 -x c++ -I/tmp -w -c @@
```
