CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -I/usr/local/include
LIBS = -lcurl -lgumbo -lcassandra
SOURCES = sitemap.cpp
EXECUTABLE = sitemap

all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(EXECUTABLE) $(LIBS)

clean:
	rm -f $(EXECUTABLE)
