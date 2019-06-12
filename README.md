# parallel-lab3-quicksort
Paralell course work containing source MPI c++ code for lab work 3.

# Instructions
1. compile code. For example: g++ -I"`<path-to-MPI>`\Include" -o a.exe main.cpp "`<path-to-MPI>`\Lib\x64\msmpi.lib"
2. mpiexec -n 4 a.exe (consider you can use any result of ```2 in power of n```, like 2 4 8 16 etc...)

# Other
There is version of code before refactoring in `old` folder.