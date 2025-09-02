CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 #-Wpedantic -pthread

# final executable
agg: agg.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# compile .cpp to .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f agg *.o
