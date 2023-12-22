CXX = g++
CXXFLAGS = -std=c++17 -fsanitize=address -Wall -Wextra -pedantic -I/usr/local/include
LIBS = -lcurl -lgumbo -lcassandra
SOURCES = $(wildcard *.cpp)
HEADERS = $(wildcard *.h)
EXECUTABLE = sitemap

all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(EXECUTABLE) $(LIBS)

clean:
	rm -f $(EXECUTABLE)
