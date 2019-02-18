all: Main ProcMain

Main: Main.cpp
	@g++ -o Main Main.cpp

ProcMain: ProcMain.cpp LogicalClock.cpp 
	@g++ -std=c++11 ProcMain.cpp LogicalClock.cpp -o ProcMain